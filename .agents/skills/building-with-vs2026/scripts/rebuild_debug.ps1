[CmdletBinding()]
param([switch]$ValidateOnly)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepoRoot = 'D:\Code\MyCode\MediaTranscode'
$BuildDirectory = Join-Path $RepoRoot 'out\build\x64-debug'
$InstallPrefix = 'D:/Code/MyCode/MediaTranscode/out/install/x64-debug'
$VsDevCmd = 'D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat'
$CMake = 'D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$Ninja = 'D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$TimeoutSeconds = 120
$RequiredProducts = @('media_transcode_local_video_cli.exe', 'media_transcode_realtime_video_cli.exe')

function Assert-RequiredPath([string]$LiteralPath, [string]$Label) {
    if (-not (Test-Path -LiteralPath $LiteralPath)) { throw "$Label not found: $LiteralPath" }
}

function Invoke-DeadlineProcess(
    [string]$FilePath,
    [string]$Arguments,
    [string]$WorkingDirectory,
    [System.Diagnostics.Stopwatch]$Clock,
    [switch]$HideOutput,
    [switch]$RejectIncludeTrace
) {
    $remaining = [Math]::Floor(($TimeoutSeconds * 1000) - $Clock.Elapsed.TotalMilliseconds)
    if ($remaining -le 0) { throw "Build timed out after $TimeoutSeconds seconds." }
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = $Arguments
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw "Could not start process: $FilePath" }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($remaining)) {
            & taskkill.exe /PID $process.Id /T /F *> $null
            throw "Build timed out after $TimeoutSeconds seconds; CMake/Ninja/CL process tree terminated."
        }
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        $combined = "$stdout`r`n$stderr"
        if ($RejectIncludeTrace -and
            $combined -match '(?im)(?:/showIncludes|^\s*Note:\s+including\s+file:)') {
            throw 'Compiler include trace was emitted; output suppressed and build rejected.'
        }
        if (-not $HideOutput -or $process.ExitCode -ne 0) {
            if ($stdout) { Write-Host $stdout -NoNewline }
            if ($stderr) { Write-Host $stderr -NoNewline }
        }
        if ($process.ExitCode -ne 0) { throw "Process exit code $($process.ExitCode): $FilePath" }
        return $combined
    }
    finally { $process.Dispose() }
}

Assert-RequiredPath $RepoRoot 'Repository root'
Assert-RequiredPath $VsDevCmd 'VsDevCmd'
Assert-RequiredPath $CMake 'VS2026 CMake'
Assert-RequiredPath $Ninja 'VS2026 Ninja'
if (-not (Test-Path -LiteralPath $BuildDirectory)) {
    New-Item -ItemType Directory -Path $BuildDirectory | Out-Null
}

$configureArguments = @(
    '-G', 'Ninja',
    '-DCMAKE_C_COMPILER:STRING=cl.exe',
    '-DCMAKE_CXX_COMPILER:STRING=cl.exe',
    '-DCMAKE_BUILD_TYPE:STRING=Debug',
    "-DCMAKE_INSTALL_PREFIX:PATH=$InstallPrefix",
    "-DCMAKE_MAKE_PROGRAM=$Ninja",
    $RepoRoot
)
$buildArguments = @('--build', '.', '--config', 'Debug', '--clean-first', '--target', 'all', '--parallel')
$developerEnvironment =
    "call $VsDevCmd -arch=amd64 -host_arch=amd64 >nul"
$configureCommand =
    "$developerEnvironment && cd /d $BuildDirectory && " +
    "$CMake $($configureArguments -join ' ')"
$buildCommand =
    "$developerEnvironment && cd /d $BuildDirectory && " +
    "$CMake $($buildArguments -join ' ')"

if ($ValidateOnly) {
    [pscustomobject]@{
        WorkingDirectory = $BuildDirectory
        TimeoutSeconds = $TimeoutSeconds
        Configure = "`"$CMake`" $($configureArguments -join ' ')"
        Build = "`"$CMake`" $($buildArguments -join ' ')"
        RunsTests = $false
    }
    exit 0
}

$clock = [Diagnostics.Stopwatch]::StartNew()
Invoke-DeadlineProcess $env:ComSpec `
    "/d /s /c `"$configureCommand`"" `
    $BuildDirectory $clock | Out-Null
$cachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
Assert-RequiredPath $cachePath 'CMake cache'
$cachedFlags = Get-Content $cachePath | Where-Object { $_ -match '^CMAKE_(?:C|CXX)_FLAGS(?:_[A-Z]+)?:' }
if ($cachedFlags -match '(?i)/showIncludes(?:\s|$)') {
    throw 'CMake cache injects /showIncludes; refusing to alter compiler options or build.'
}

Invoke-DeadlineProcess $env:ComSpec `
    "/d /s /c `"$buildCommand`"" `
    $BuildDirectory $clock -RejectIncludeTrace | Out-Null
Assert-RequiredPath (Join-Path $BuildDirectory 'build.ninja') 'Ninja build graph'
foreach ($product in $RequiredProducts) {
    Assert-RequiredPath (Join-Path $BuildDirectory $product) 'Required product'
}
Write-Host 'VS2026 Debug clean-first all-target rebuild succeeded; configure/build exit codes were 0.'
exit 0
