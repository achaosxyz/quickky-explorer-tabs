[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build/release-build',
    [string]$OutputDirectory = 'build/release',
    [string]$Generator,
    [string]$Toolset,
    [string]$WindowsSdk
)
. "$PSScriptRoot/common.ps1"
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $version = Get-ProductVersion $root
    $build = [IO.Path]::GetFullPath($BuildDirectory)
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    $configure = @('-S', $root, '-B', $build, '-A', 'x64', '-DBUILD_TESTING=ON')
    if ($Generator) { $configure += @('-G', $Generator) }
    if ($Toolset) { $configure += @('-T', $Toolset) }
    if ($WindowsSdk) { $configure += "-DCMAKE_SYSTEM_VERSION=$WindowsSdk" }
    Invoke-Checked cmake $configure | Out-Host
    Invoke-Checked cmake @('--build', $build, '--config', 'Release', '--target',
        'QuickkyExplorerTabs', '--parallel', '4') | Out-Host
    Invoke-Checked ctest @('--test-dir', $build, '-C', 'Release', '-R', '^app\.smoke$',
        '--no-tests=error', '--output-on-failure') | Out-Host

    # Use an isolated staging directory; never recursively remove a caller's path.
    $stage = Join-Path $build ('package-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage, $output -Force | Out-Null
    foreach ($file in @('QTabs.exe', 'QuickkyExplorerHook.dll')) {
        Copy-Item -LiteralPath (Join-Path $build "Release/$file") -Destination $stage
    }
    $runtimes = @(Get-Content -LiteralPath (Join-Path $build 'generated/runtime-files.txt') -Encoding UTF8 |
        Where-Object { $_.Trim().Length -gt 0 })
    if (-not $runtimes.Count) { throw 'CMake did not locate the MSVC redistributable runtime.' }
    foreach ($runtime in $runtimes) {
        Copy-Item -LiteralPath $runtime -Destination $stage
    }
    foreach ($file in @('VERSION', 'LICENSE', 'README.md', 'README.zh-CN.md')) {
        Copy-Item -LiteralPath (Join-Path $root $file) -Destination $stage
    }
    $assets = Join-Path $stage 'resources'
    New-Item -ItemType Directory -Path $assets | Out-Null
    foreach ($file in @('logo.png', 'tab-preview.png')) {
        Copy-Item -LiteralPath (Join-Path $root "resources/$file") -Destination $assets
    }
    $zip = Join-Path $output "QTabs_v$version.zip"
    Compress-Archive -Path "$stage/*" -DestinationPath $zip -Force

    # Test the distributed layout, not just the executable in the CMake tree.
    $unpacked = Join-Path $build ('smoke-' + [guid]::NewGuid().ToString('N'))
    Expand-Archive -LiteralPath $zip -DestinationPath $unpacked
    $process = Start-Process -FilePath (Join-Path $unpacked 'QTabs.exe') `
        -ArgumentList '--smoke-test' -WorkingDirectory $unpacked -PassThru
    if (-not $process.WaitForExit(10000)) {
        $process.Kill()
        throw 'Packaged application smoke test timed out.'
    }
    if ($process.ExitCode -ne 0) { throw "Packaged application returned $($process.ExitCode)." }
    [pscustomobject]@{ Version = $version; Executable = Join-Path $stage 'QTabs.exe'; Package = $zip }
} finally {
    Pop-Location
}
