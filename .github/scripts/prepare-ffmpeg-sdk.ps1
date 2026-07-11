$ErrorActionPreference = 'Stop'

$archive = Join-Path $env:RUNNER_TEMP 'ffmpeg-sdk.zip'
$sdkParent = Join-Path $env:RUNNER_TEMP 'ffmpeg-sdk'
$headers = @{
    Accept = 'application/octet-stream'
    Authorization = "Bearer $env:GITHUB_TOKEN"
    'X-GitHub-Api-Version' = '2022-11-28'
}

Invoke-WebRequest "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/assets/$env:FFMPEG_ASSET_ID" `
    -Headers $headers -OutFile $archive
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
