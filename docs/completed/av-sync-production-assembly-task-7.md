# A/V Sync Production Assembly Task 7 Completion

Task 7 assembles one planner-bound shared A/V output scheduler and one typed
output router. Media units retain one scheduling authority, while typed terminal
controls cross the video and audio boundary atomically.

## Result

- `MediaRealtimeAvSchedulerSegmentBuilder` consumes the planned sync-group key
  and exact synchronized-packet policy without invoking or duplicating planner
  decisions. It rejects incomplete inputs and duplicate scheduling authority.
- `MediaScheduledOutputRouterNode` validates each scheduled stream discriminator
  and routes the immutable access unit to exactly one output. The single-output
  hot path uses the channel push outcome directly and retains pending work across
  backpressure without transaction allocations or payload cloning.
- EOF, Flush, and Abort use `MediaAtomicOutputTransaction` to validate capacity
  and lifecycle for both outputs, commit both queues, and publish visibility only
  after the complete fanout is present.
- `MediaChannel` coordinates push, pop, close, abort, and clear mutations without
  busy waiting. Blocking-producer and blocking-consumer metrics retain their
  acceptance-report semantics.
- `MediaAvBoundReleaseExtractorNode` uses the same atomic output contract and
  rejects an incompatible output policy during `start`, before accepting input.

Implementation commit: `77cb296` (`feat: share av output scheduler`).

## Verification

Commands were executed from the repository root in PowerShell. Build commands
are omitted.

```powershell
ctest --test-dir out/build/x64-debug -R '^media_transcode_channel_atomic_output_tests$' --repeat until-fail:20 --output-on-failure
ctest --test-dir out/build/x64-debug -R '^media_transcode_av_sync_scheduled_output_tests$' --repeat until-fail:20 --output-on-failure
ctest --test-dir out/build/x64-debug -R '^(media_transcode_core_tests|media_transcode_planner_tests|media_transcode_builder_tests|media_transcode_runtime_tests|media_transcode_node_tests|media_transcode_av_sync_production_release_tests|media_transcode_av_sync_canonical_input_tests|media_transcode_av_sync_audio_control_tests|media_transcode_canonical_lineage_tests|media_transcode_video_codec_lineage_tests|media_transcode_audio_codec_lineage_tests|media_transcode_audio_lineage_node_tests)$' --output-on-failure -j 4
.\out\build\x64-debug\media_transcode_integration_tests.exe --task5-av-sync-input
```

Results:

- Atomic channel lifecycle, pre-blocked consumer visibility, metrics, and retry
  coverage passed 20/20 in 0.59 seconds.
- Active shared scheduler and typed router coverage passed 20/20 in 132.99
  seconds.
- The selected core, planner, builder, runtime, node, release, audio-control,
  canonicalization, and lineage suite passed 12/12 in 42.40 seconds.
- Focused Task 5 A/V-sync input integration exited 0 and reported
  `Task 5 A/V sync input tests passed`.
- The final clean rebuild completed the full 494-step DAG successfully. Generated
  compiler rules contained 64 exact `/showIncludes=0` command tokens, with no
  doubled or bare token.
- The complete integration executable still reports exactly three pre-existing
  RTP/Opus audio-batch timing expectations. No Task 7 regression was reported.

## Review

The first independent review rejected per-access-unit use of the multi-output
transaction and requested coverage for consumers already blocked before atomic
commit. The hot path now uses direct channel push outcomes, and the deterministic
test starts both blocking consumers before transaction acquisition.

The final independent review passed both specification and code-quality axes
with Critical, Important, and Minor findings at `0/0/0`.

## Remaining Scope

- Task 8 must connect the routed scheduled units to the production RTP senders
  and publish one dual-media SDP.
- Task 9 must connect the routed units to the project-owned MPEG-TS mux path.
- Hardware RTP and MPEG-TS playback, drift diagnostics, two-minute stability,
  and process-scoped CPU measurement remain later production-assembly acceptance
  work.
- Implementation commit `77cb296` is pushed to
  `codex/quality-priority-improvements`.
