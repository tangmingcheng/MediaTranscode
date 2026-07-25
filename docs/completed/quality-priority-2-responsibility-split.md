# Quality Priority 2 Completion

Priority 2 splits oversized planning, capability scanning, RTP muxing, and runtime responsibilities while keeping decisions in the planner.

## Result

- Capability scanning delegates input opening, stream inspection, audio inspection, video candidate scanning, and hardware symbol probing to focused components.
- Realtime planning delegates request validation, classification, input planning, and output policy construction.
- RTP muxing delegates FFmpeg session ownership, protocol writes, and constrained lifecycle state transitions.
- Runtime compilation/registration and lifecycle execution are separated behind the runtime facade.
- Missing audio RTP pacing bitrate fails during planning; no default or fallback bitrate remains.
- Audio decode waits for codec metadata before consuming packets.

## Verification

Commands were executed from the repository root in PowerShell. Build commands are omitted.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure
ctest --test-dir out/build/x64-debug -C Debug -R media_transcode_event_runtime_tests --repeat until-fail:20 --output-on-failure
```

Result: 2/2 tests passed; event-runtime passed 20 consecutive executions.

Separate RTP used explicit H264/AAC targets with output audio sample rate 44.1 kHz, naturally selecting video and audio transcode branches. Both final 60-second paths passed after device-probe and mux-state fixes: CUDA/NVENC average CLI process CPU was 1.0094%; software average/P95 normalized CLI CPU was 16.2449%/24.1424%.

MPEG-TS UDP used the same H264/AAC 44.1 kHz target. Both final 60-second paths passed: hardware produced 1851 frames at 1.1823% average normalized CLI CPU; software produced 1788 frames at 14.6819%.

Local MP4 H264/AAC 44.1 kHz output was repeatedly transcoded and ffprobe-validated for 60 seconds per path. Hardware completed 18 loops and software completed 10 loops with zero command or stream-validation failures.

CPU metrics for realtime tests sampled only `media_transcode_realtime_video_cli`.
