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
