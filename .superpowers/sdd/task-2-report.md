# Task 2 Report: Exact Canonical Audio Intervals

## Status

Implemented the Task 2 canonical-media contract and explicit factory migration.
No `AudioDecodeNode` or `MediaAudioDecodeInputView` production behavior was
changed.

## RED evidence

The first attempted build did not count as RED because the Visual Studio
development environment was not loaded:

```powershell
cmake --build out/build/x64-debug --target media_transcode_canonical_lineage_tests --clean-first
```

Result: exit `2`; compilation stopped before the target test because the
standard library headers `string` and `cstdint` were unavailable.

After loading `D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat`, the
canonical-buffer contract produced the expected RED:

```powershell
cmake --build out/build/x64-debug --target media_transcode_canonical_lineage_tests --clean-first
```

Result: exit `2`. Relevant compiler diagnostics:

```text
canonical_lineage_tests.cpp: error C2660:
MediaCanonicalAccessUnitBuffer::create does not take 3 arguments
canonical_lineage_tests.cpp: error C2039:
audioSampleInterval is not a member of MediaCanonicalAccessUnitBuffer
```

The canonical-input refresh contract independently produced the expected RED:

```powershell
cmake --build out/build/x64-debug --target media_transcode_av_sync_canonical_input_tests --clean-first
```

Result: exit `2`. Relevant compiler diagnostic:

```text
av_sync_canonical_input_tests.cpp: error C2039:
audioSampleInterval is not a member of MediaCanonicalAccessUnitBuffer
```

Both RED failures were caused by the missing Task 2 contract rather than test
setup or an unrelated assertion.

## GREEN implementation

- `MediaCanonicalAccessUnitBuffer::create` now requires an explicit optional
  audio interval, rejects audio without one, rejects video with one, and
  rejects invalid intervals.
- Canonical media exposes the immutable interval through
  `audioSampleInterval()`.
- `MediaCanonicalInputNode` creates one
  `MediaCanonicalAudioSourceTimeline` from the planner-provided sample rate,
  appends the planner-provided sample count once per locked audio access unit,
  preserves the cursor across presentation mapping refreshes, rejects
  readiness/generation changes, and clears the timeline with node state.
- `MediaEncodedAudioCanonicalizerNode` propagates its already validated encoded
  fragment interval `{begin, end, outputSampleRate}`.
- All production and test factory call sites now pass an explicit third
  argument. Video passes `std::nullopt`; audio test sources pass their known
  interval facts.

## GREEN evidence

Focused canonical-buffer build:

```powershell
cmake --build out/build/x64-debug --target media_transcode_canonical_lineage_tests --clean-first
```

Result: exit `0`; target linked successfully.

Focused canonical-buffer executable:

```powershell
$env:PATH = 'D:\mabs\local64\bin-video;' + $env:PATH
out\build\x64-debug\media_transcode_canonical_lineage_tests.exe
```

Result: exit `0`.

Focused canonical-input build:

```powershell
cmake --build out/build/x64-debug --target media_transcode_av_sync_canonical_input_tests --clean-first
```

Result: exit `0`; target linked successfully.

Focused canonical-input executable:

```powershell
$env:PATH = 'D:\mabs\local64\bin-video;' + $env:PATH
out\build\x64-debug\media_transcode_av_sync_canonical_input_tests.exe
```

Result: exit `0`. The test emitted 600 adjacent `1024 @ 44100` intervals,
verified the final exact sample end, rejected non-locked readiness and a later
generation, then proved the original cursor remained usable.

Strongest safe compilation:

```powershell
cmake --build out/build/x64-debug --clean-first
```

Result: exit `0`; all 512 build steps completed, including every migrated
factory call site.

Strongest safe relevant executable suite:

```text
media_transcode_canonical_audio_source_timeline_tests.exe exit=0
media_transcode_canonical_lineage_tests.exe exit=0
media_transcode_av_sync_canonical_input_tests.exe exit=0
media_transcode_audio_lineage_node_tests.exe exit=0
media_transcode_audio_codec_lineage_tests.exe exit=0
media_transcode_core_tests.exe exit=0
media_transcode_planner_tests.exe exit=0
media_transcode_node_tests.exe exit=0
```

`media_transcode_av_sync_production_runtime_integration_tests.exe` was never
run.

## Files changed

- `src/internal/graph/sync/MediaCanonicalAccessUnitBuffer.h`
- `src/internal/graph/sync/MediaCanonicalAccessUnitBuffer.cpp`
- `src/internal/graph/nodes/sync/MediaCanonicalInputNode.h`
- `src/internal/graph/nodes/sync/MediaCanonicalInputNode.cpp`
- `src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.cpp`
- `src/internal/graph/nodes/sync/MediaEncodedAudioCanonicalizerNode.cpp`
- `src/internal/graph/nodes/video/VideoEncodeNode.cpp`
- `tests/unit/canonical_lineage_tests.cpp`
- `tests/unit/av_sync_canonical_input_tests.cpp`
- `tests/unit/audio_lineage_node_tests.cpp`
- `tests/unit/audio_origin_envelope_tests.cpp`
- `tests/unit/av_output_scheduler_tests.cpp`
- `tests/unit/av_sync_production_release_tests.cpp`
- `tests/unit/av_sync_scheduled_output_tests.cpp`
- `tests/unit/mpeg_ts_demux_node_tests.cpp`
- `tests/unit/video_codec_lineage_tests.cpp`
- `tests/unit/video_codec_node_lineage_tests.cpp`
- `tests/unit/fixtures/ScheduledMpegTsOutputIntegrationRuntime.cpp`
- `tests/unit/fixtures/ScheduledRtpOutputIntegrationRuntime.cpp`
- `tests/unit/fixtures/ScheduledRtpOutputIntegrationRuntime.h`
- `.superpowers/sdd/task-2-report.md`

## Self-review and remaining concerns

- Re-ran `rg -n "MediaCanonicalAccessUnitBuffer::create\(" src tests` and
  inspected every result. There is no two-argument factory call left.
- Audio decode consumption remains intentionally unchanged and is the explicit
  responsibility of Task 3.
- The prohibited production runtime integration executable was not exercised;
  remote integration and hardware tiers remain pending.
- Existing unrelated working-tree files, including
  `.superpowers/sdd/progress.md`, were not included in Task 2 scope.

## Review fix: planner-only audio sample count

Review found that canonical audio still accepted
`canonical_input.duration_source=packet` and derived sample count from
`AVPacket::duration` and its runtime time base. This was an unreachable
compatibility branch for validated production plans, but it violated the
planner-only sample-count contract.

### Review RED

Added a test that configures synchronized canonical audio with packet duration,
a sample rate, and no planner sample count:

```powershell
cmake --build out/build/x64-debug --target media_transcode_av_sync_canonical_input_tests --clean-first
```

Result: exit `0`; target linked successfully.

```powershell
$env:PATH = 'D:\mabs\local64\bin-video;' + $env:PATH
out\build\x64-debug\media_transcode_av_sync_canonical_input_tests.exe
```

Result: exit `1`, with the expected assertion:

```text
av_sync_canonical_input_tests.cpp:430:
!harness.runtime->process(harness.execution)
```

The failure proved that the old runtime still accepted and inferred the audio
sample count.

### Review implementation

- Removed `MediaCanonicalInputNode::audioSampleCountFor` and all
  packet-duration/time-base arithmetic used to infer audio samples.
- Removed the redundant stored duration-source state. Video duration uses
  packet evidence; audio duration and exact intervals use only the configured
  planner sample rate and sample count.
- Configuration now accepts only `packet` for video and only `audio_samples`
  for audio. Missing or nonpositive `canonical_input.audio_sample_count` fails
  during configuration.
- Removed the configurator's MPEG-TS packet-duration compatibility path.
  Both RTP and MPEG-TS planner products already contain
  `MediaPlannedAudioSamplesDurationPlan`, and the runtime validator already
  requires its rate/count to match planning facts.
- Replaced the old packet-audio success test with explicit planned
  `1024 @ 48000` evidence and added missing-count rejection coverage.

### Review GREEN

Focused clean-first build:

```powershell
cmake --build out/build/x64-debug --target media_transcode_av_sync_canonical_input_tests --clean-first
```

Result: exit `0`; all 417 build steps completed and the target linked.

Focused executable:

```powershell
$env:PATH = 'D:\mabs\local64\bin-video;' + $env:PATH
out\build\x64-debug\media_transcode_av_sync_canonical_input_tests.exe
```

Result:

```text
media_transcode_av_sync_canonical_input_tests.exe exit=0
```

Final strongest safe clean-first build:

```powershell
cmake --build out/build/x64-debug --clean-first
```

Result: exit `0`; all 512 build steps completed.

Final safe relevant executable suite:

```text
media_transcode_canonical_audio_source_timeline_tests.exe exit=0
media_transcode_canonical_lineage_tests.exe exit=0
media_transcode_av_sync_canonical_input_tests.exe exit=0
media_transcode_audio_lineage_node_tests.exe exit=0
media_transcode_audio_codec_lineage_tests.exe exit=0
media_transcode_core_tests.exe exit=0
media_transcode_planner_tests.exe exit=0
media_transcode_node_tests.exe exit=0
```

The prohibited production runtime integration executable was not run. The
reviewer's minor note about unchecked manual cursor addition in two scheduled
output test fixtures remains recorded and was intentionally not broadened into
this planner-only fix.
