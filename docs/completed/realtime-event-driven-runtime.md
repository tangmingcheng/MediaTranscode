# Realtime Event-Driven Runtime

## Root Cause

Realtime per-node workers and SPSC blocking operations retried with yield loops when no data was available. Event-driven concurrency also exposed a second ownership defect: multiple nodes read mutable input `AVFormatContext` / `AVStream` state while Demux advanced FFmpeg.

## Resolution

- Every runtime node directly returns `Progress`, `Waiting`, or `Finished`; errors remain explicit failures.
- Per-node sequence wakeups provide targeted channel notification and lost-wakeup-safe waiting.
- Multi-input nodes track terminal state by channel identity. Mux completion independently tracks exact config keys, terminal channels, header, pending packets, and trailer.
- Multi-input consumption uses lifecycle-reset round-robin arbitration, preventing a continuously busy first channel from starving later inputs.
- SPSC blocking operations use condition variables with deterministic close and abort behavior.
- The executor constructs all workers before starting threads, avoiding partially initialized runtime observation.
- Input nodes publish immutable per-stream snapshots after probing. Snapshots own deep-copied codec parameters plus format, time, index, and kind.
- Snapshots freeze the FFmpeg-resolved source frame rate while the probed input context is still available, preserving `av_guess_frame_rate`, average-rate, and real-rate behavior.
- Demux exclusively owns and mutates the input format context. Packet normalization, source configuration, and codec resolvers consume snapshots only.

## Test Commands And Results

```powershell
out\build\x64-debug\media_transcode_event_runtime_tests.exe
```

Result: passed; `event runtime tests passed`.

```powershell
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
```

Result: passed; `realtime graph tests passed`.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure
```

Result: passed, 2/2 tests.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File out\build\x64-debug\codex_av_scheduler_smoke.ps1
```

- Hardware CUDA/NVENC, 60 seconds on the final implementation: 0.58% whole-machine CPU, 1.281 CPU seconds, 1663 frames, `drop_frames=0`, `workerErrors=0`.
- Software encoding, 60 seconds: 16.8% whole-machine CPU and 36.953 CPU seconds. The remaining load is codec work rather than scheduler idle spinning.

```powershell
git diff --check
```

Result: passed.

VLC subjective playback completed for 60 seconds. Human observation reported smooth video, normal audio, and no perceptible A/V drift; the CLI completed with `workerErrors=0`.

## Remaining Risks

- Per-node workers still create one OS thread per runtime node; a bounded worker pool remains a separate scalability improvement.
- PR #20 was created and its final implementation head received an independent PASS review with zero Critical, Important, or Minor findings.
