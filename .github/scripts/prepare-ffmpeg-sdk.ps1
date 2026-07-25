$ErrorActionPreference = 'Stop'

$requiredEnvironmentVariables = @(
    'FFMPEG_RELEASE_TAG',
    'FFMPEG_ASSET_NAME',
    'FFMPEG_SHA256',
    'RUNNER_TEMP',
    'GITHUB_WORKSPACE'
)
foreach ($name in $requiredEnvironmentVariables) {
    if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name))) {
        throw "Required environment variable $name is not set"
    }
}

$archive = Join-Path $env:RUNNER_TEMP 'ffmpeg-sdk.zip'
$sdkParent = Join-Path $env:RUNNER_TEMP 'ffmpeg-sdk'
$downloadUri = "https://github.com/BtbN/FFmpeg-Builds/releases/download/$env:FFMPEG_RELEASE_TAG/$env:FFMPEG_ASSET_NAME"

Invoke-WebRequest $downloadUri -OutFile $archive
$actualHash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualHash -ne $env:FFMPEG_SHA256) {
    throw "FFmpeg SDK checksum mismatch: $actualHash"
}

Expand-Archive $archive -DestinationPath $sdkParent
$sdk = Get-ChildItem $sdkParent -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'include/libavcodec') } |
    Select-Object -First 1
if (-not $sdk -or -not (Test-Path (Join-Path $sdk.FullName 'lib'))) {
    throw 'FFmpeg SDK layout is unsupported'
}

$projectFfmpeg = Join-Path $env:GITHUB_WORKSPACE '3rds/ffmpeg'
$projectInclude = Join-Path $projectFfmpeg 'include'
$projectDebugLib = Join-Path $projectFfmpeg 'debug/lib'
$projectLib = Join-Path $projectFfmpeg 'lib'
$projectBin = Join-Path $projectFfmpeg 'bin'
New-Item $projectInclude,$projectDebugLib,$projectLib,$projectBin -ItemType Directory -Force | Out-Null
Copy-Item (Join-Path $sdk.FullName 'include/*') $projectInclude -Recurse -Force
Copy-Item (Join-Path $sdk.FullName 'lib/*.lib') $projectDebugLib -Force
Copy-Item (Join-Path $sdk.FullName 'lib/*.lib') $projectLib -Force
Copy-Item (Join-Path $sdk.FullName 'bin/*.dll') $projectBin -Force
Copy-Item (Join-Path $sdk.FullName 'bin/ffmpeg.exe') $projectBin -Force
