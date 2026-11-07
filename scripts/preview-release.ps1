[CmdletBinding()]
param([string]$OutputDirectory = 'build/release')
. "$PSScriptRoot/common.ps1"
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $plan = Get-ReleasePlan $root
    $range = $plan.sourceSha
    if ($plan.previousTag) { $range = "$($plan.previousTag)..$($plan.sourceSha)" }
    $commits = @(Invoke-Checked git @('log', '--no-merges', '--format=- %s (%h)', $range))
    $changes = $commits -join "`n"
    if (-not $changes) { $changes = '- No new non-merge commits.' }
    $notes = @"
# QTabs $($plan.version)

Experimental preview: general Windows 10/11 compatibility and complete native-tab
integration are not verified. A smoke-test pass does not establish Explorer compatibility.

## Changes

$changes

## Download

- Windows x86_64: ``$($plan.packageName)``
- Extract all files and run ``QTabs.exe`` without elevation.
- Matching Microsoft Visual C++ runtime DLLs are included; no separate runtime installation is needed.

Source commit: ``$($plan.sourceSha)``
"@
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    New-Item -ItemType Directory -Path $output -Force | Out-Null
    Write-Utf8File (Join-Path $output 'release-plan.json') (($plan | ConvertTo-Json) + "`n")
    Write-Utf8File (Join-Path $output 'RELEASE_NOTES.md') ($notes.Trim() + "`n")
    if (@(Invoke-Checked git @('status', '--porcelain')).Count -ne 0) {
        Write-Warning 'Release notes describe committed HEAD only; uncommitted changes are not included.'
    }
    Write-Host $notes
    Write-Host "Next develop version: $($plan.nextVersion). No Git refs or GitHub releases were changed."
} finally {
    Pop-Location
}
