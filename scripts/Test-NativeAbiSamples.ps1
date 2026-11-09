[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build/vs2017-tabs',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$ResearchDirectory = 'build/research/abi-versions-20261107'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$research = Join-Path $root $ResearchDirectory
$executable = Join-Path (Join-Path (Join-Path $root $BuildDirectory) $Configuration) 'QuickkyNativeAbiTests.exe'
if (!(Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Build the ABI tests first: $executable" }
$inventory = Get-Content -LiteralPath (Join-Path $research 'inventory.json') -Raw | ConvertFrom-Json
$returned = Get-Content -LiteralPath (Join-Path $research 'server-returned-inventory.json') -Raw | ConvertFrom-Json
$fixtures = @(Get-ChildItem -LiteralPath (Join-Path $root 'tests/abi_reference') -Filter '10.*.cpp' |
    ForEach-Object { $_.BaseName })
$samples = @($inventory.Samples) + @($returned.Samples)
$supportedVersions = @($samples | Where-Object { $fixtures -contains $_.Version } | ForEach-Object { $_.Version })
if ($supportedVersions.Count -ne $fixtures.Count -or
    @(Compare-Object ($fixtures | Sort-Object) ($supportedVersions | Sort-Object -Unique)).Count) {
    throw 'The inventories must contain each reference fixture exactly once.'
}
$negativeManifest = Get-Content -LiteralPath (Join-Path $root 'tests/abi_reference/rejections.json') -Raw | ConvertFrom-Json
$negativeVersions = @($negativeManifest.MissingNativeInterface) + @($negativeManifest.ExcludedAccessorContract)
if (!$negativeVersions.Count -or
    @($negativeVersions | Sort-Object -Unique).Count -ne $negativeVersions.Count -or
    @($negativeVersions | Where-Object { $fixtures -contains $_ }).Count) {
    throw 'Negative reference registrations must be nonempty, unique and disjoint from supported fixtures.'
}
$inventoryNegatives = @($samples | Where-Object {
    $negativeVersions -contains $_.Version -or $_.Status -eq 'MissingNativeInterface' -or
    $_.ScopeDisposition -eq 'ExcludedAccessorContract'
})
if ($inventoryNegatives.Count -ne $negativeVersions.Count -or
    @(Compare-Object ($negativeVersions | Sort-Object) ($inventoryNegatives.Version | Sort-Object -Unique)).Count) {
    throw 'The inventories must contain each negative reference exactly once.'
}
foreach ($sample in $inventoryNegatives) {
    if (!(($negativeManifest.MissingNativeInterface -contains $sample.Version -and $sample.Status -eq 'MissingNativeInterface') -or
        ($negativeManifest.ExcludedAccessorContract -contains $sample.Version -and $sample.ScopeDisposition -eq 'ExcludedAccessorContract'))) {
        throw "Negative reference category differs from its registration: $($sample.Version)"
    }
}
$accepted = 0
$rejected = 0
foreach ($sample in $samples) {
    $supported = $fixtures -contains $sample.Version
    $excluded = $negativeVersions -contains $sample.Version
    if (!$supported -and !$excluded) { continue } # Failed acquisition is not a negative ABI fixture.
    $image = $sample.ImagePath
    if (!$image -and $sample.RequestedVersion) {
        $provenance = Get-Content -LiteralPath (Join-Path $research "server-returned/$($sample.Version)/provenance.json") -Raw |
            ConvertFrom-Json
        $image = $provenance.SampleImagePath
    }
    if (!$image -or !(Test-Path -LiteralPath $image -PathType Leaf)) { throw "Missing verified image: $($sample.Version)" }
    # The executable accepts any registered image. Check inventory association
    # too, so a stale/wrong path cannot silently count one DLL as another version.
    if (!$sample.ActualSha256 -or (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash -ne $sample.ActualSha256) {
        throw "Image bytes differ from the verified inventory: $($sample.Version)"
    }
    $argument = if ($supported) { '--image' } else { '--unsupported-image' }
    & $executable $argument $image
    if ($LASTEXITCODE -ne 0) { throw "ABI sample failed: $($sample.Version), $argument, exit $LASTEXITCODE" }
    if ($supported) { ++$accepted } else { ++$rejected }
    Write-Host "$($sample.Version): $argument passed"
}
if ($accepted -ne $fixtures.Count) { throw "Not all reference fixtures exercised: $accepted/$($fixtures.Count)" }
if ($rejected -ne $negativeVersions.Count) { throw "Not all negative references exercised: $rejected/$($negativeVersions.Count)" }
Write-Host "Mode recognition: $accepted accepted references, $rejected explicit exclusions/missing-interface references."
