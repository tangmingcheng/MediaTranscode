# Task 5 Report: Protocol Output Generation Rollover

## Result

- Replaced observation-style protocol state with exact generation permit and
  commit validation.
- Scheduler, scheduled RTP, scheduled TS adapter, and MPEG-TS plan source now
  require an open transition permit and exact active generation before commit.
- RTP generation reset preserves the UDP transport while replacing
  generation-owned sender state.
- MPEG-TS plan output remains open across generations and republishes a fresh
  runtime plan after the next planned activation.
- The project MPEG-TS mux owns one stable byte sink while rebuilding its
  generation-owned clock, packetizer, continuity, table, writer, and watermark
  state for each permitted runtime plan.
- No legacy fallback or default downstream decision path was added.

## TDD Evidence

RED clean-first build failed on the deliberately missing
`permitActivatedGeneration` and `validateCommitGeneration` API.

GREEN test commands and results:

```powershell
out/build/x64-debug/media_transcode_av_sync_scheduled_output_tests.exe
# exit 0: A/V scheduled output tests passed

out/build/x64-debug/media_transcode_scheduled_rtp_output_node_tests.exe
# exit 0

out/build/x64-debug/media_transcode_scheduled_mpeg_ts_output_node_tests.exe
# exit 0: Scheduled MPEG-TS output node tests passed

out/build/x64-debug/media_transcode_av_reacquisition_runtime_assembly_tests.exe
# exit 0
```

The final build used `--clean-first --parallel 4` for all three focused
targets.

## Review

- Commit gates re-read the transition snapshot and reject closed, poisoned, or
  mismatched generations.
- Purge closes output before the next exact activation sequence can reopen it.
- Old scheduled TS units are discarded; future-generation units fail fast.
- Focused test fixtures use atomic output edges and planner-produced TS policy
  and correction timing.
- The MPEG-TS rollover test verifies that permit closure blocks old bytes,
  generation 2 accepts a reset timeline value, PAT continuity restarts from
  its planned seed, and the same sink remains open until final finish.
- The unrelated dirty
  `tests/unit/mpeg_ts_observed_avio_integration_tests.cpp` was not modified or
  staged by this task.

## Remaining Risks

- High-memory and long-running integration tests were intentionally not run.
- Live network transport was not exercised; the bounded mux test uses an
  injected recording byte sink.
