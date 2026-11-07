# Shared release primitives. Native failures must never turn into a successful build.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param([string]$FilePath, [string[]]$Arguments)
    $command = Get-Command -Name $FilePath -ErrorAction Stop
    $preference = $ErrorActionPreference
    try {
        # Windows PowerShell can treat redirected native stderr as an ErrorRecord.
        # Git progress on stderr is not failure; the process exit code decides.
        $ErrorActionPreference = 'Continue'
        & $command @Arguments
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $preference
    }
    if ($exitCode -ne 0) {
        throw "$FilePath failed with exit code $exitCode."
    }
}

function Get-ProductVersion {
    param([string]$Root)
    $value = [IO.File]::ReadAllText((Join-Path $Root 'VERSION')).Trim()
    if ($value -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
        throw 'VERSION must contain canonical MAJOR.MINOR.PATCH.'
    }
    foreach ($part in $value.Split('.')) {
        if ([uint64]$part -gt 65535) { throw 'Version components must fit Windows version resources.' }
    }
    return $value
}

function Get-NextPatchVersion {
    param([string]$Version)
    $parsed = [version]$Version
    if ($parsed.Build -ge 65535) { throw 'The next patch version exceeds the Windows resource limit.' }
    return '{0}.{1}.{2}' -f $parsed.Major, $parsed.Minor, ($parsed.Build + 1)
}

function Write-Utf8File {
    param([string]$Path, [string]$Text)
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

function Get-ReleasePlan {
    param([string]$Root)
    $version = Get-ProductVersion $Root
    $source = (Invoke-Checked git @('-C', $Root, 'rev-parse', 'HEAD')).Trim()
    $tags = @(Invoke-Checked git @('-C', $Root, 'tag', '--merged', $source,
        '--list', 'v*', '--sort=-v:refname'))
    $previous = ''
    foreach ($tag in $tags) {
        if ($tag -cnotmatch '^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { continue }
        if ([version]$tag.Substring(1) -gt [version]$version) {
            throw 'VERSION is older than a reachable release tag.'
        }
        if ([version]$tag.Substring(1) -lt [version]$version) {
            $previous = $tag
            break
        }
    }
    return [ordered]@{
        version = $version
        tag = "v$version"
        sourceSha = $source
        previousTag = $previous
        nextVersion = Get-NextPatchVersion $version
        packageName = "QTabs_v$version.zip"
    }
}
