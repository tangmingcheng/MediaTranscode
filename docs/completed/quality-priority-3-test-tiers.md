# Quality Priority 3 Completion

Priority 3 separates deterministic component tests from explicitly enabled integration, hardware, and performance tiers.

## Result

- Independent targets and CTest labels cover core, planner, builder, runtime, node, integration, hardware, and performance.
- CMake configure, build, and test presets expose deterministic, integration, hardware, and performance workflows.
- Default deterministic configuration excludes environment-dependent tiers.
- Hardware capability absence returns code 77 and is reported as skipped with an unsupported reason.
- Windows CI downloads a fixed FFmpeg 8.1 shared SDK asset, verifies its SHA256, stages it under the repository-owned `3rds/ffmpeg` layout, and copies runtime DLLs beside each tier's executables before testing. CMake remains independent of external machine paths and runtime deployment.
- Audio encoding now waits for required codec metadata before consuming queued frames; this removes the startup race found by the software RTP gate.

No audio transcode switch was added. All acceptance paths request H264/AAC with 44.1 kHz audio, so planner comparison against the 48 kHz source naturally selects audio and video transcoding.

## Verification

Commands were executed from the repository root in PowerShell. Build commands are omitted.

```powershell
cmake -DSOURCE_DIR=D:/Code/MyCode/MediaTranscode -P tests/cmake/verify_test_tiers.cmake
cmake --list-presets=all
ctest --test-dir out/build/x64-debug --output-on-failure
ctest --test-dir out/build/x64-debug --print-labels
```

Result: the tier contract passed; all four configure/build/test preset names were parsed; 9/9 tests passed after a clean rebuild. CUDA device creation passed instead of being skipped. All eight required labels were listed.

On the validation host, required runtime dependencies were copied from `D:/mabs/local64/bin-video` beside the generated executables as a validation-environment step, outside CMake.

The local MP4 gate repeatedly ran the local CLI for at least 60 seconds per path and validated each output using ffprobe. Result: hardware completed 18 loops and software completed 10 loops, with zero command or H264/AAC 44.1 kHz stream-validation failures.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File out/build/x64-debug/codex_av_scheduler_smoke.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File out/build/x64-debug/codex_av_scheduler_smoke.ps1 -DisableHw
```

Result: separate RTP hardware passed with 1.3664% average and 1.8924% P95 normalized CLI CPU; software passed with 14.8930% average and 23.1459% P95. Both produced separate H264 video and AAC 44.1 kHz audio RTP/SDP outputs with no gate errors.

```powershell
out/build/x64-debug/codex_mpegts_60s_gate.ps1 -Name p3_hw -InputPort 8040 -OutputPort 8042 -DurationSeconds 60
out/build/x64-debug/codex_mpegts_60s_gate.ps1 -Name p3_sw -InputPort 8050 -OutputPort 8052 -DurationSeconds 60 -DisableHw
```

Result: MPEG-TS hardware produced 1851 frames at 1.1563% average and 2.2541% P95 normalized CLI CPU. Software produced 1766 frames at 16.6275% average and 24.4583% P95. CPU sampling included only `media_transcode_realtime_video_cli`.
