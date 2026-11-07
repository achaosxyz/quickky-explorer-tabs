# Synthetic release checks: local fixture repositories and a mocked GitHub CLI only.
[CmdletBinding()]
param()
. "$PSScriptRoot/common.ps1"
$root = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $root ('build/release-tests/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Message)
    $threw = $false
    try { & $Action | Out-Null } catch { $threw = $true }
    Assert-True $threw $Message
}

function New-ReleaseFixture {
    param([string]$Name)
    $directory = Join-Path $testRoot $Name
    $remote = Join-Path $testRoot "$Name.git"
    Invoke-Checked git @('init', '--quiet', '--bare', '--initial-branch=main', $remote) | Out-Host
    Invoke-Checked git @('init', '--quiet', '--initial-branch=main', $directory) | Out-Host
    Invoke-Checked git @('-C', $directory, 'config', 'user.name', 'Release Test') | Out-Host
    Invoke-Checked git @('-C', $directory, 'config', 'user.email', 'test@example.invalid') | Out-Host
    New-Item -ItemType Directory -Path (Join-Path $directory 'scripts') | Out-Null
    foreach ($file in @('common.ps1', 'preview-release.ps1', 'publish-release.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $directory 'scripts')
    }
    Write-Utf8File (Join-Path $directory '.gitignore') "/build/`n"
    Write-Utf8File (Join-Path $directory 'VERSION') "0.0.1`n"
    Invoke-Checked git @('-C', $directory, 'add', '.') | Out-Host
    Invoke-Checked git @('-C', $directory, 'commit', '--quiet', '-m', 'feat: initial fixture') | Out-Host
    Invoke-Checked git @('-C', $directory, 'remote', 'add', 'origin', $remote) | Out-Host
    Invoke-Checked git @('-C', $directory, 'push', '--quiet', 'origin', 'main') | Out-Host
    Invoke-Checked git @('-C', $directory, 'switch', '--quiet', '-c', 'develop') | Out-Host
    Write-Utf8File (Join-Path $directory 'change.txt') "Tested source`n"
    Invoke-Checked git @('-C', $directory, 'add', '.') | Out-Host
    Invoke-Checked git @('-C', $directory, 'commit', '--quiet', '-m', 'feat: tested change') | Out-Host
    Invoke-Checked git @('-C', $directory, 'push', '--quiet', 'origin', 'develop') | Out-Host
    $plan = Get-ReleasePlan $directory
    $artifacts = Join-Path $directory 'build/release'
    New-Item -ItemType Directory -Path $artifacts -Force | Out-Null
    Write-Utf8File (Join-Path $artifacts 'release-plan.json') ($plan | ConvertTo-Json)
    Write-Utf8File (Join-Path $artifacts 'RELEASE_NOTES.md') "Fixture notes`n"
    Write-Utf8File (Join-Path $artifacts $plan.packageName) 'Mock ZIP; package smoke is checked by build.ps1.'
    return @{ Root = $directory; Remote = $remote; Plan = $plan; Artifacts = $artifacts }
}

# This function shadows the CLI only in this test script's scope and its children.
function gh {
    $arguments = @($args)
    $state = $global:QTabsReleaseTestState
    $global:LASTEXITCODE = 0
    if ($arguments[0] -ceq 'api') {
        @{ body = "Fixture notes`n"; draft = $state.Draft;
            assets = @(@{ name = 'QTabs_v0.0.1.zip'; digest = $state.Digest }) } | ConvertTo-Json -Depth 4 -Compress
        return
    }
    switch ($arguments[1]) {
        'list' {
            if ($state.Release) {
                ConvertTo-Json -InputObject @(@{ tagName = 'v0.0.1'; isDraft = $state.Draft }) -Compress
            } else { '[]' }
        }
        'create' {
            $state.Release = $true
            $state.Draft = $true
            $zip = Join-Path $state.Fixture.Artifacts $state.Fixture.Plan.packageName
            $state.Digest = 'sha256:' + (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        'edit' {
            if ($state.FailPublish) { $global:LASTEXITCODE = 1; return }
            $state.Draft = $false
            if ($state.RaceDevelop) {
                $tree = (Invoke-Checked git @('-C', $state.Fixture.Root, 'show', '-s',
                    '--format=%T', $state.Fixture.Plan.sourceSha)).Trim()
                $racer = (Invoke-Checked git @('-C', $state.Fixture.Root, 'commit-tree', $tree,
                    '-p', $state.Fixture.Plan.sourceSha, '-m', 'feat: concurrent change')).Trim()
                Invoke-Checked git @('-C', $state.Fixture.Root, 'push', '--quiet', 'origin',
                    "${racer}:refs/heads/develop") | Out-Host
                $state.Racer = $racer
            }
        }
        'view' {
            if ($arguments -notcontains '--jq') { throw 'Unexpected release view command.' }
            'false'
        }
        default { throw 'Unexpected mocked GitHub command.' }
    }
}

$savedEnvironment = @{}
if (Get-Variable -Name QTabsReleaseTestState -Scope Global -ErrorAction SilentlyContinue) {
    throw 'A release test is already active in this PowerShell session.'
}
$global:QTabsReleaseTestState = @{}
foreach ($name in @('GITHUB_ACTIONS', 'GITHUB_EVENT_NAME', 'GITHUB_REF', 'GITHUB_REPOSITORY', 'GITHUB_SHA')) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name)
}
try {
    $nativeShell = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    $nativeOutput = @(Invoke-Checked $nativeShell @('-NoProfile', '-NonInteractive', '-Command',
        "[Console]::Error.WriteLine('Native progress'); Write-Output 'Native output'; exit 0") `
        2> (Join-Path $testRoot 'native-stderr.log'))
    Assert-True ($nativeOutput -contains 'Native output') 'Successful native stderr must not abort or pollute stdout.'
    Assert-Throws { Invoke-Checked $nativeShell @('-NoProfile', '-NonInteractive', '-Command', 'exit 7') } `
        'A nonzero native exit must still fail.'
    Assert-Throws { Invoke-Checked 'qtabs-test-missing-command' @() } 'A missing native command must still fail.'
    Assert-True ((Get-NextMinorVersion '0.0.1') -ceq '0.1.0') 'Increment MINOR and reset PATCH.'
    Assert-True ((Get-NextMinorVersion '2.9.7') -ceq '2.10.0') 'Carry a multi-digit minor version.'
    Assert-Throws { Get-NextMinorVersion '1.65535.0' } 'Reject resource overflow.'
    $versionRoot = Join-Path $testRoot 'versions'
    New-Item -ItemType Directory -Path $versionRoot | Out-Null
    foreach ($invalid in @('01.2.3', '1.2', '1.2.3-rc1', '1.2.65536', 'version=1.2.3')) {
        Write-Utf8File (Join-Path $versionRoot 'VERSION') $invalid
        Assert-Throws { Get-ProductVersion $versionRoot } "Reject invalid version $invalid."
    }
    Invoke-Checked git @('init', '--quiet', '--initial-branch=develop', $versionRoot) | Out-Host
    Invoke-Checked git @('-C', $versionRoot, 'config', 'user.name', 'Release Test') | Out-Host
    Invoke-Checked git @('-C', $versionRoot, 'config', 'user.email', 'test@example.invalid') | Out-Host
    foreach ($version in @('0.0.1', '0.1.0')) {
        Write-Utf8File (Join-Path $versionRoot 'VERSION') ($version + "`n")
        Invoke-Checked git @('-C', $versionRoot, 'add', 'VERSION') | Out-Host
        Invoke-Checked git @('-C', $versionRoot, 'commit', '--quiet', '-m', "feat: version $version") | Out-Host
        Invoke-Checked git @('-C', $versionRoot, 'tag', "v$version") | Out-Host
    }
    $plan = Get-ReleasePlan $versionRoot
    Assert-True ($plan.previousTag -ceq 'v0.0.1') 'A retry must exclude its own current release tag.'
    Write-Utf8File (Join-Path $versionRoot 'VERSION') "0.0.1`n"
    Assert-Throws { Get-ReleasePlan $versionRoot } 'Reject a version older than reachable release history.'

    $env:GITHUB_ACTIONS = 'false'
    Assert-Throws { & "$PSScriptRoot/publish-release.ps1" -SourceSha invalid -Repository invalid } 'Reject local publication.'
    $env:GITHUB_ACTIONS = 'true'
    $env:GITHUB_EVENT_NAME = 'workflow_dispatch'
    $env:GITHUB_REF = 'refs/heads/develop'
    $env:GITHUB_REPOSITORY = 'fixture/fixture'

    foreach ($scenario in @('success', 'failed-publication', 'concurrent-develop')) {
        $script:fixture = New-ReleaseFixture $scenario
        if ($scenario -ceq 'success') {
            & (Join-Path $fixture.Root 'scripts/preview-release.ps1') -OutputDirectory $fixture.Artifacts | Out-Host
            $notesPath = Join-Path $fixture.Artifacts 'RELEASE_NOTES.md'
            $previewNotes = [IO.File]::ReadAllText($notesPath)
            Assert-True ($previewNotes.Contains('runtime DLLs are included; no separate runtime installation is needed.')) `
                'Preview notes must describe the bundled runtime, not require a separate installer.'
            Write-Utf8File $notesPath "Fixture notes`n"
        }
        $global:QTabsReleaseTestState = @{ Fixture = $fixture; Release = $false; Draft = $false;
            Digest = ''; Racer = ''; FailPublish = $scenario -ceq 'failed-publication'; RaceDevelop = $scenario -ceq 'concurrent-develop' }
        $env:GITHUB_SHA = $fixture.Plan.sourceSha
        $publish = Join-Path $fixture.Root 'scripts/publish-release.ps1'
        $publishArguments = @{ ArtifactDirectory = $fixture.Artifacts; SourceSha = $env:GITHUB_SHA; Repository = 'fixture/fixture' }
        Assert-Throws { & $publish @publishArguments } 'Publication requires main promotion before smoke and release.'
        Assert-True (-not $global:QTabsReleaseTestState.Release) 'No GitHub release is created before main promotion.'
        & $publish @publishArguments -PromoteMain
        $promotedMain = (Invoke-Checked git @('--git-dir', $fixture.Remote, 'rev-parse', 'main')).Trim()
        Assert-True ($promotedMain -ceq $fixture.Plan.sourceSha) 'Promote the exact develop source before hosted smoke.'
        Assert-True (-not $global:QTabsReleaseTestState.Release) 'Promotion alone does not publish a release.'
        Assert-True ((Get-ProductVersion $fixture.Root) -ceq '0.0.1') 'Promotion alone does not bump the version.'
        $originalMain = (Invoke-Checked git @('--git-dir', $fixture.Remote, 'rev-parse', 'main')).Trim()
        if ($scenario -ceq 'success') {
            & $publish @publishArguments
        } else {
            Assert-Throws { & $publish @publishArguments } 'The failing scenario must stop publication/finalization.'
            $main = (Invoke-Checked git @('--git-dir', $fixture.Remote, 'rev-parse', 'main')).Trim()
            Assert-True ($main -ceq $originalMain) 'Publication failure must preserve the already promoted main.'
        }
        Invoke-Checked git @('-C', $fixture.Root, 'switch', '--quiet', '--detach', $fixture.Plan.sourceSha) | Out-Host
        if ($scenario -ceq 'failed-publication') {
            Assert-True $global:QTabsReleaseTestState.Draft 'Publication failure leaves an inspectable draft.'
            Assert-Throws { & $publish @publishArguments } 'Never adopt or overwrite an existing draft automatically.'
            # Simulate a maintainer inspecting and publishing the draft before retry.
            $global:QTabsReleaseTestState.Draft = $false
            $global:QTabsReleaseTestState.FailPublish = $false
            & $publish @publishArguments
        } elseif ($scenario -ceq 'concurrent-develop') {
            $remoteDevelop = (Invoke-Checked git @('--git-dir', $fixture.Remote, 'rev-parse', 'develop')).Trim()
            Assert-True ($remoteDevelop -ceq $global:QTabsReleaseTestState.Racer) 'The fixture must actually advance develop concurrently.'
            Assert-True ($remoteDevelop -cne $fixture.Plan.sourceSha) 'The concurrent commit must differ from the tested source.'
            Assert-Throws { & $publish @publishArguments } 'Retry must preserve concurrent develop work.'
            continue
        }
        $main = (Invoke-Checked git @('--git-dir', $fixture.Remote, 'rev-parse', 'main')).Trim()
        $next = (Invoke-Checked git @('--git-dir', $fixture.Remote, 'show', 'develop:VERSION')).Trim()
        Assert-True ($main -ceq $fixture.Plan.sourceSha) 'main must match the tested commit.'
        Assert-True ($next -ceq '0.1.0') 'develop must advance to the next minor version.'
        Invoke-Checked git @('-C', $fixture.Root, 'switch', '--quiet', '--detach', $fixture.Plan.sourceSha) | Out-Host
        & $publish @publishArguments
        $global:QTabsReleaseTestState.Digest = 'sha256:not-the-tested-zip'
        Assert-Throws { & $publish @publishArguments } 'Never trust only the name of an existing release asset.'
    }
    Write-Host 'Release checks passed: versions, publication gate, finalization, retry, and concurrent-branch protection.'
    $global:LASTEXITCODE = 0
} finally {
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name])
    }
    Remove-Variable -Name QTabsReleaseTestState -Scope Global
}
