[CmdletBinding()]
param(
    [string]$ArtifactDirectory = 'build/release',
    [Parameter(Mandatory = $true)][string]$SourceSha,
    [Parameter(Mandatory = $true)][string]$Repository,
    [switch]$PromoteMain
)
. "$PSScriptRoot/common.ps1"

# Promotion/publication require an explicit, same-repository dispatch.
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:GITHUB_EVENT_NAME -ne 'workflow_dispatch' -or
    $env:GITHUB_REF -ne 'refs/heads/develop' -or $Repository -cne $env:GITHUB_REPOSITORY) {
    throw 'Publication must run through the develop release workflow in the target repository.'
}
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if (@(Invoke-Checked git @('status', '--porcelain')).Count -ne 0) {
        throw 'Publication requires a clean checkout.'
    }
    $expected = Get-ReleasePlan $root
    if ($SourceSha -cne $expected.sourceSha -or $SourceSha -cne $env:GITHUB_SHA) {
        throw 'The checkout must match the exact dispatched source commit.'
    }
    Invoke-Checked git @('fetch', 'origin', 'main', 'develop', '--tags') | Out-Host
    $main = (Invoke-Checked git @('rev-parse', 'origin/main')).Trim()
    $develop = (Invoke-Checked git @('rev-parse', 'origin/develop')).Trim()
    if ($PromoteMain) {
        if ($develop -cne $SourceSha) { throw 'develop changed before main promotion.' }
        Invoke-Checked git @('merge-base', '--is-ancestor', $main, $SourceSha) | Out-Host
        Invoke-Checked git @('push', 'origin', "${SourceSha}:refs/heads/main") | Out-Host
        Write-Host 'main now selects the pinned candidate; smoke testing must pass before publication.'
        return
    }
    $artifacts = [IO.Path]::GetFullPath($ArtifactDirectory)
    $plan = Get-Content -LiteralPath (Join-Path $artifacts 'release-plan.json') -Raw | ConvertFrom-Json
    foreach ($key in $expected.Keys) {
        if ($plan.$key -cne $expected[$key]) { throw "Release plan mismatch: $key." }
    }
    $tag = $plan.tag
    $zip = Join-Path $artifacts $plan.packageName
    $notesPath = Join-Path $artifacts 'RELEASE_NOTES.md'
    if (-not (Test-Path -LiteralPath $zip -PathType Leaf)) { throw 'The tested ZIP is missing.' }
    $notes = [IO.File]::ReadAllText($notesPath).Trim()

    if ($main -cne $SourceSha) { throw 'main must already match the smoke-tested source.' }
    $finalized = $false
    if ($develop -cne $SourceSha) {
        $parent = (Invoke-Checked git @('show', '-s', '--format=%P', $develop)).Trim()
        $files = @(Invoke-Checked git @('diff-tree', '--no-commit-id', '--name-only', '-r', $develop))
        $nextVersion = (Invoke-Checked git @('show', "${develop}:VERSION")).Trim()
        $subject = (Invoke-Checked git @('show', '-s', '--format=%s', $develop)).Trim()
        $finalized = $main -ceq $SourceSha -and $parent -ceq $SourceSha -and
            $files.Count -eq 1 -and $files[0] -ceq 'VERSION' -and
            $nextVersion -ceq $plan.nextVersion -and
            $subject -ceq "chore(release): start $($plan.nextVersion) development"
        if (-not $finalized) { throw 'develop changed after dispatch. Inspect the release before retrying.' }
    }

    $hasTag = @(Invoke-Checked git @('tag', '--list', $tag)).Count -ne 0
    if ($hasTag) {
        $tagSource = (Invoke-Checked git @('rev-list', '-n', '1', $tag)).Trim()
        if ($tagSource -cne $SourceSha) { throw 'The release tag already refers to a different commit.' }
    }
    $releases = ((Invoke-Checked gh @('release', 'list', '--repo', $Repository,
        '--limit', '100', '--json', 'tagName,isDraft')) -join "`n") | ConvertFrom-Json
    $existing = @($releases | Where-Object { $_.tagName -ceq $tag })
    if ($existing.Count -ne 0) {
        $release = ((Invoke-Checked gh @('api', "repos/$Repository/releases/tags/$tag")) -join "`n") | ConvertFrom-Json
        if ($release.draft) {
            throw 'An existing draft requires maintainer inspection; it will not be overwritten or published.'
        }
        $assets = @($release.assets | Where-Object { $_.name -ceq $plan.packageName })
        if (-not $hasTag -or $release.body.Trim() -cne $notes -or
            $assets.Count -ne 1) {
            throw 'The existing published release does not match this release plan.'
        }
        # A filename alone cannot establish that an existing asset passed this run's checks.
        $digest = 'sha256:' + (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($assets[0].digest -cne $digest) { throw 'The published ZIP differs from the tested artifact.' }
    } elseif ($finalized) {
        throw 'Branches look finalized but the published release is missing.'
    }
    if ($finalized) {
        Write-Host "Release $tag and its branch updates are already complete."
        return
    }

    # Prepare the successor locally; develop is not advanced before publication.
    Invoke-Checked git @('switch', '--detach', $SourceSha) | Out-Host
    Write-Utf8File (Join-Path $root 'VERSION') ($plan.nextVersion + "`n")
    Invoke-Checked git @('add', '--', 'VERSION') | Out-Host
    Invoke-Checked git @('-c', 'user.name=github-actions[bot]', '-c',
        'user.email=41898282+github-actions[bot]@users.noreply.github.com', 'commit',
        '-m', "chore(release): start $($plan.nextVersion) development") | Out-Host
    $next = (Invoke-Checked git @('rev-parse', 'HEAD')).Trim()

    if ($existing.Count -eq 0) {
        if (-not $hasTag) {
            Invoke-Checked git @('tag', $tag, $SourceSha) | Out-Host
            Invoke-Checked git @('push', 'origin', "refs/tags/$tag") | Out-Host
        }
        # Upload first as a draft. A failed upload must not publish an empty release.
        Invoke-Checked gh @('release', 'create', $tag, $zip, '--repo', $Repository,
            '--verify-tag', '--draft', '--title', "QTabs $($plan.version)",
            '--notes-file', $notesPath) | Out-Host
        Invoke-Checked gh @('release', 'edit', $tag, '--repo', $Repository,
            '--draft=false', '--latest') | Out-Host
        $draft = (Invoke-Checked gh @('release', 'view', $tag, '--repo', $Repository,
            '--json', 'isDraft', '--jq', '.isDraft')).Trim()
        if ($draft -cne 'false') { throw 'Release publication was not confirmed.' }
    }

    # Atomic, non-forced fast-forwards preserve concurrent work on either branch.
    Invoke-Checked git @('push', '--atomic', 'origin',
        "${SourceSha}:refs/heads/main", "${next}:refs/heads/develop") | Out-Host
    Write-Host "Published $tag; main now contains the tested source; develop is $($plan.nextVersion)."
} finally {
    Pop-Location
}
