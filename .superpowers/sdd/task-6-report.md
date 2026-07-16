# Task 12 / Task 6 Report: Audio generation lineage closure

## Outcome

The audio and video codec/filter lineage closure is implemented and has passed
the final deterministic verification described below. Decode, StartupTrim,
Resample, Encode, VideoDecode, VideoFilter, VideoFrameRate, and VideoEncode each
own a typed lifecycle-stable purge target. Production graph wiring and human
playback acceptance remain a separate Task 12 boundary.

## Delivered

- Immutable vector-valued audio lineage fragments; exact accumulator aggregation, splitting, residue detection, generation continuity, and sample-rate continuity.
- Typed `AudioDecodeLineageState`, `MediaAudioStartupTrimLineageState`, `AudioResampleLineageState`, and `AudioEncodeLineageState`; no generic variant or callback purge seam.
- Production decoder and encoder codec adapters are owned by stable state and flushed during generation purge under the lineage lock.
- Actual capacity-one Decode, StartupTrim, Resample, and Encode purge transitions: retained old output and delayed codec/SWR output are removed; only the next generation emerges once and in order.
- Decode and Encode capacity checks execute before codec acceptance. Tests verify a second capacity-one input is rejected without increasing codec send count.
- Non-FIFO `frame_size == 0` Encode EAGAIN retains the exact `AVFrame*`, PTS, and fragment vector until acceptance.
- Startup trim is consumed exactly once per playback origin and is not re-applied to later fragments.
- Decode emits the complete startup-trim directive only on the first decoded envelope. StartupTrim owns the remaining count across any number of full frames, validates the first retained sample after exact-frame trimming, rejects repeated directives, and fails terminal input while trim or first-sample validation remains unresolved.
- Resample uses cumulative checked 44.1 kHz to 48 kHz projection. Source cumulative addition, rescale, projected-start addition, projection extension, Decode interval end, and applied correction accumulation fail closed on overflow.
- SWR correction authorization is based on the checked net delta of fully applied windows. Pure-negative, pure-positive, positive-to-negative net-zero, and negative-to-positive net-zero cases are covered. Terminal residue must exactly match the outstanding net authorized drop; under- and over-authorization fail.
- Correction window-end arithmetic is checked before enqueue. External correction terminal settlement rejects pending or partially executed windows instead of discarding them.
- SWR EOF completion treats `swr_get_out_samples(0)` only as a capacity upper bound. `swr_get_delay` classifies the pre-drain state as `NoDelay` or `MayProduce`, but neither classification proves exhaustion. Only a positive-capacity `swr_convert(..., nullptr, 0)` returning zero creates typed `AudioSwrResamplerExhausted` evidence. A partially executed non-zero compensation window fails closed at exhaustion because exact application cannot be proven; a zero-delta tail may settle. Mid-stream Flush retains active and pending correction windows.
- Synchronized Resample verifies that the checked sum of immutable input fragments exactly equals `AVFrame::nb_samples` before mutating generation or SWR state.
- Decode and Resample preserve exact multi-fragment envelopes; Encode FIFO splitting preserves fragment ownership and payload association.
- Retained generic Control EOF is generation-bound in Decode, StartupTrim,
  Resample, and Encode. Purge cancels the base pending-finish transfer, clears
  typed terminal/eof state, leaves outputs open, and accepts next-generation
  media.
- Encoder codec metadata is generation-invariant planned configuration; an
  exact retained codec-context buffer drains once after output backpressure.
- Resample responsibilities are separated into typed lineage state,
  transactional lineage mapper, and RAII SWR session. Node code retains only
  correction/terminal orchestration, timestamps, and output commit.
- Decode, Encode, and Resample lifecycle reset and generation purge reuse one
  typed-state storage reset path with explicit codec/SWR differences.
- Task-specific node tests were removed from the monolithic node suite and
  retained in focused audio lineage, startup-trim, resample, and codec targets.

## Verification

The following test commands were run against the final clean rebuild. The
clean rebuild itself completed successfully; build commands are intentionally
omitted from this report.

```powershell
ctest --preset deterministic --output-on-failure
```

Result: 18/18 passed, 100%, 19.12 seconds.

```powershell
ctest --test-dir out\build\x64-debug-deterministic -R "media_transcode_(runtime_tests|av_generation_transition_tests|audio_lineage_node_tests|audio_startup_trim_node_tests|video_codec_node_lineage_tests|video_filter_node_lineage_tests)" --repeat until-fail:20 --output-on-failure
```

Result: six generation/lifecycle targets each passed 20 consecutive runs,
100%, 14.20 seconds.

Focused executables also exited with code 0:

```powershell
out\build\x64-debug\media_transcode_audio_lineage_node_tests.exe
out\build\x64-debug\media_transcode_audio_codec_lineage_tests.exe
out\build\x64-debug\media_transcode_audio_startup_trim_node_tests.exe
out\build\x64-debug\media_transcode_audio_origin_envelope_tests.exe
out\build\x64-debug\media_transcode_node_tests.exe
out\build\x64-debug\media_transcode_builder_tests.exe
out\build\x64-debug\media_transcode_runtime_tests.exe
out\build\x64-debug\media_transcode_video_codec_node_lineage_tests.exe
out\build\x64-debug\media_transcode_video_filter_node_lineage_tests.exe
```

The first test discovery immediately after the deterministic clean rebuild saw
17 executables as temporarily unavailable while the build output was still
becoming visible. The same preset rerun after process completion passed 18/18;
the transient discovery result is retained here and is not reported as a pass.

During clean video verification, CDB identified an access violation caused by
reading and moving the same `shared_ptr` in delegating-constructor arguments.
The state now moves the registry first and derives synchronization from the
stored member. The clean rebuilt video codec lineage target and all regressions
above pass with that correction.

Independent review then found that StartupTrim lifecycle restart cleared only
retained-control freshness. A regression first reproduced the stale origin and
trim state after stop. `resetForLifecycle()` now clears owned trim state and
base generation state through one state-owner method used by start, stop, and
abort. Both stop-to-start and abort-to-start regressions pass in the repeated
suite above.

The final independent read-only review reported PASS with zero Critical,
Important, or Minor findings for the lifecycle correction.

## Remaining boundary

If a node has already delivered part of a multi-output batch downstream when a generation transition occurs, local purge cannot retract that accepted output. The Task 11 downstream queue purge/atomic generation transition remains the required system boundary for removing already-enqueued stale output. This task does not claim RTP or MPEG-TS human playback acceptance; that belongs to the final Task 12 production wiring and playback gate.

`MediaQueuePolicy::allowFlushControlBypass` currently has no production queue
consumer and therefore does not reserve capacity or authorize dropping media.
Task 6 deliberately does not invent those semantics; the field name remains
queue-policy debt. Audio-scoped terminal broadcast is valid only when every
output is Audio. Nodes with mixed output streams must use a generic Control
terminal so stream compatibility remains explicit.
