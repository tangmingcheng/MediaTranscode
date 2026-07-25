$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$workflowPath = Join-Path $repoRoot '.github/workflows/test-tiers.yml'
$sdkScriptPath = Join-Path $repoRoot '.github/scripts/prepare-ffmpeg-sdk.ps1'
$expectedTag = 'autobuild-2026-07-14-13-19'
$expectedAsset = 'ffmpeg-n8.1.2-22-g94138f6973-win64-gpl-shared-8.1.zip'
$expectedSha256 = '3cd74234a1c9acf2546143bcc68b3ffe84e16f32e3be32f5096d61844739b17d'

function Assert-True {
    param(
        [Parameter(Mandatory)]
        [bool]$Condition,
        [Parameter(Mandatory)]
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$workflow = Get-Content $workflowPath -Raw
$sdkScript = Get-Content $sdkScriptPath -Raw

Assert-True ($workflow -match "FFMPEG_RELEASE_TAG:\s*`"$([regex]::Escape($expectedTag))`"") `
    'Workflow must pin the immutable FFmpeg release tag.'
Assert-True ($workflow -match "FFMPEG_ASSET_NAME:\s*`"$([regex]::Escape($expectedAsset))`"") `
    'Workflow must pin the FFmpeg asset name.'
Assert-True ($workflow -match "FFMPEG_SHA256:\s*`"$expectedSha256`"") `
    'Workflow must pin the approved FFmpeg checksum.'
Assert-True ($workflow -match 'run:\s*tests/ci/test_prepare_ffmpeg_sdk_contract\.ps1') `
    'Workflow must execute the FFmpeg SDK contract test.'
Assert-True ($workflow -notmatch 'FFMPEG_ASSET_ID') `
    'Workflow must not depend on a mutable release asset ID.'
Assert-True ($sdkScript -notmatch 'FFMPEG_ASSET_ID|releases/latest|/latest/') `
    'SDK preparation must not depend on latest release discovery or an asset ID.'

$savedEnvironment = @{}
$environmentNames = @(
    'FFMPEG_RELEASE_TAG',
    'FFMPEG_ASSET_NAME',
    'FFMPEG_SHA256',
    'RUNNER_TEMP',
    'GITHUB_WORKSPACE'
)
foreach ($name in $environmentNames) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name)
}

$global:ffmpegSdkDownloadUri = $null
function global:Invoke-WebRequest {
    param(
        [Parameter(Position = 0)]
        [string]$Uri,
        [string]$OutFile
    )

    $global:ffmpegSdkDownloadUri = $Uri
    [IO.File]::WriteAllText($OutFile, 'small deterministic test payload')
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) "ffmpeg-sdk-contract-$([guid]::NewGuid())"
New-Item $testRoot -ItemType Directory | Out-Null
try {
    $env:FFMPEG_RELEASE_TAG = $null
    $env:FFMPEG_ASSET_NAME = $expectedAsset
    $env:FFMPEG_SHA256 = $expectedSha256
    $env:RUNNER_TEMP = $testRoot
    $env:GITHUB_WORKSPACE = $testRoot

    $missingEnvironmentError = $null
    try {
        & $sdkScriptPath
    }
    catch {
        $missingEnvironmentError = $_.Exception.Message
    }
    Assert-True ($missingEnvironmentError -match 'FFMPEG_RELEASE_TAG.*not set') `
        'SDK preparation must reject a missing release tag before downloading.'
    Assert-True ($null -eq $global:ffmpegSdkDownloadUri) `
        'SDK preparation must validate required environment variables before downloading.'

    $env:FFMPEG_RELEASE_TAG = $expectedTag
    $checksumError = $null
    try {
        & $sdkScriptPath
    }
    catch {
        $checksumError = $_.Exception.Message
    }

    $expectedUri = "https://github.com/BtbN/FFmpeg-Builds/releases/download/$expectedTag/$expectedAsset"
    Assert-True ($global:ffmpegSdkDownloadUri -eq $expectedUri) `
        "SDK preparation used an unexpected download URI: $global:ffmpegSdkDownloadUri"
    Assert-True ($checksumError -match '^FFmpeg SDK checksum mismatch:') `
        'SDK preparation must fail closed on a checksum mismatch.'
}
finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name])
    }
    Remove-Item Function:\global:Invoke-WebRequest -ErrorAction SilentlyContinue
    Remove-Item Variable:\global:ffmpegSdkDownloadUri -ErrorAction SilentlyContinue
    Remove-Item $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'FFmpeg SDK preparation contract passed.'
