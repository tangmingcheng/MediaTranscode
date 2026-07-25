# A/V Sync Production Task 9 Completion

Task 9 connects the shared A/V scheduler to the project-owned MPEG-TS mux without introducing another media pacing authority.

## Result

- The production path is scheduler -> serialized A/V router -> scheduled TS adapter -> `FileMuxNode` -> project MPEG-TS mux -> byte sink.
- The immutable mux plan and exact playback epoch are published once before scheduled media is accepted.
- H.264 packet layout is an explicit encoder capability fact. Missing facts fail with `Unsupported`; no codec-name inference or fallback is used.
- RTP sender lead and MPEG-TS transport lead are consumed from complete planner output.
- The mux uses the sync-group master clock only for PSI/PCR transport deadlines and advances one deadline per poll.
- FFmpeg codec configuration is materialized under explicit plan contracts, and sink/session ownership remains RAII and close-once.
- Exact terminal MPEG-TS input parsing was committed separately as `fix: finalize exact mpeg ts input eof`.

## Verification

Commands were executed from the repository root in PowerShell. Build commands are omitted.

```powershell
ctest --test-dir out/build/x64-debug --output-on-failure -L deterministic
ctest --test-dir out/build/x64-debug --output-on-failure -R "^media_transcode_mpeg_ts_output_decode_integration_tests$"
```

Results:

- Clean full rebuild: passed, 532 build steps.
- Deterministic tier: 26/26 passed in 60.97 seconds.
- Scheduled project MPEG-TS decode integration: 1/1 passed in 4.04 seconds.
- Integration verified H.264/AAC decode through EOF, PAT/PMT/PIDs, continuity, PCR, ADTS, and playback-epoch-derived video/audio PTS/DTS.
- Independent standards review: PASS.
- Independent specification review: PASS.

## Remaining Boundary

- Real encoders, including `h264_nvenc`, remain `Unsupported` for project MPEG-TS output until Task 10 publishes a proven packet-layout capability or plans an explicit converter.
- Task 12 must extend timestamp diagnostics from representative stream matches to full-sequence drift measurement before human-eye playback.
