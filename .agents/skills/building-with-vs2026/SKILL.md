---
name: building-with-vs2026
description: Use when configuring or fully rebuilding MediaTranscode x64 Debug or Release with Visual Studio 2026.
---

# Building with VS2026

Use [scripts/rebuild_debug.ps1](scripts/rebuild_debug.ps1) as the only build entry point. It establishes amd64, configures the fixed Ninja tree, and performs a clean-first all-target rebuild within one 120-second deadline. `Debug` is the default; request `Release` explicitly when Release artifacts are required.

## Build command

```powershell
& "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "D:\Code\MyCode\MediaTranscode\.agents\skills\building-with-vs2026\scripts\rebuild_debug.ps1" -Configuration Release
```

Use `-Configuration Debug` for Debug. The script sets `out/build/x64-<configuration>`, matching install prefix, C/C++ `cl.exe`, VS2026 bundled Ninja, and source `D:\Code\MyCode\MediaTranscode`.

## Required invariants

- Initialize `VsDevCmd.bat -arch=amd64 -host_arch=amd64` through the script.
- “全部重新生成” means `--clean-first --target all`; incremental, partial, and exploratory compilation are prohibited.
- Never add `/showIncludes`. Fail on cached injection without changing compiler options. Ninja internal MSVC dependency collection is not user-visible include trace.
- At 120 seconds, terminate the active CMake/Ninja/CL process tree and report timeout.
- This build workflow does not run tests. Real media acceptance remains governed by the repository's `AGENTS.md`.

## Success contract

Require configure and build exit code `0`, `CMakeCache.txt`, `build.ninja`, `media_transcode_local_video_cli.exe`, and `media_transcode_realtime_video_cli.exe`. Any nonzero exit, timeout, include trace, cache conflict, or missing artifact is failure. A successful build does not establish media acceptance.
