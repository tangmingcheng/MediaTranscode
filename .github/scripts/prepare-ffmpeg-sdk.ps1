$ErrorActionPreference = 'Stop'

$archive = Join-Path $env:RUNNER_TEMP 'ffmpeg-sdk.zip'
$sdkParent = Join-Path $env:GITHUB_WORKSPACE '.ci/ffmpeg'
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

$debugLib = Join-Path $sdk.FullName 'debug/lib'
New-Item $debugLib -ItemType Directory -Force | Out-Null
Copy-Item (Join-Path $sdk.FullName 'lib/*.lib') $debugLib
"FFMPEG_ROOT=$($sdk.FullName)" | Out-File $env:GITHUB_ENV -Append
(Join-Path $sdk.FullName 'bin') | Out-File $env:GITHUB_PATH -Append
