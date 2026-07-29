---
name: building-with-vs2026
description: Use when configuring or fully rebuilding the MediaTranscode x64 Debug tree with Visual Studio 2026, bundled CMake, Ninja, and MSVC under a strict 120-second wall-clock limit.
---

# Building with VS2026

## Overview

Use the bundled script as the only build entry point. It establishes amd64, configures the fixed Ninja Debug tree, performs a clean-first all-target rebuild, and enforces one 120-second deadline.

## Required workflow

```powershell
& "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
    -NoProfile -ExecutionPolicy Bypass -File `
    ".agents\skills\building-with-vs2026\scripts\rebuild_debug.ps1"
```

The script fixes cwd at `out/build/x64-debug` and configures `Ninja`, C/C++ `cl.exe`, `Debug`, install prefix `out/install/x64-debug`, VS2026 bundled Ninja, and source `D:\Code\MyCode\MediaTranscode`.

Treat “全部重新生成” as `--clean-first --target all`; never use incremental, partial, or exploratory compilation.

## Hard constraints

- Start with `VsDevCmd.bat -arch=amd64 -host_arch=amd64`.
- Never add `/showIncludes`. Ninja internal MSVC dependency collection is not user-visible include trace.
- If cached C/C++ flags contain `/showIncludes`, report failure without changing compiler options.
- At 120 seconds, terminate the active CMake/Ninja/CL process tree and report timeout. Never keep waiting.
- Do not run tests.

## Success contract

Require configure and build exit code `0`, `CMakeCache.txt`, `build.ninja`, `media_transcode_local_video_cli.exe`, and `media_transcode_realtime_video_cli.exe`. Any nonzero exit, timeout, include trace, cache conflict, or missing artifact is failure.

## Common mistakes

| Mistake | Required response |
|---|---|
| Deadline is nearly met | Terminate at 120 seconds. |
| Incremental build seems faster | Run clean-first all targets. |
| `/showIncludes` seems useful | Do not add it; fail on cached injection. |
