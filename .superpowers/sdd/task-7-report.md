# Task 7 - Audio drift servo and SWR compensation

## Result

- Added planner-owned audio servo policy validation and deterministic drift correction generation.
- Split servo decisions from immutable compensation commands. Only the quantizer
  constructs commands; buffers, nodes, and the SWR executor accept commands only.
- Added a single planner/runtime limits authority and a shared sample-domain lead
  validator used by both plan validation and servo creation.
- Integrated correction input into the audio encode segment. `ExternalCorrectionRequired` requires an explicit generation and source endpoint; `Disabled` creates no correction port or edge.
- Integrated one-quantum SWR execution into `AudioResampleNode`, including same-format forced SWR, contiguous correction windows, actual-sample PTS accounting, output backpressure, and one-shot EOF.
- Local and realtime builders explicitly select `Disabled` until the correction producer is connected by the owning integration task.
- A required codec metadata channel that closes before binding now fails with a
  typed error instead of waiting forever.

## Contract coverage

- Recovery entry/exit uses phase, frequency, and hold duration.
- Old generations produce `DropOldGeneration`; future generations produce `Reacquire` without silently resetting the current servo.
- Measurement rollback, sample-rate mismatch, gaps, hard discontinuities, anti-windup, slew, and normal/recovery clamps are covered.
- Long-term quantization covers `-500/-100/-50/+50/+100/+500 ppm` with alternating 990/1010 ms measurement intervals.
- Command lead validation uses a floor lower bound for available lead samples, a
  ceil upper bound for the maximum-gap output at positive recovery correction,
  and a mandatory one-sample margin. The 1 s gap / 1.001 s lead / +5000 ppm
  boundary is rejected; the first representable passing 48 kHz boundary is covered.
- Executor rejects generation mismatch, sequence reuse, overlap, and post-first window gaps before execution.
- Node-level SWR tests cover 44.1 kHz to 48 kHz conversion with real -977/+977 ppm
  commands, 1023/1025-sample windows, PTS advancement by actual output samples,
  EOF exactly once, bounded correction/frame fairness, and output `WouldBlock`
  recovery with a one-buffer output queue.

## Verification

- `ctest --test-dir out/build/x64-debug -L deterministic --output-on-failure`:
  6/6 passed in 4.08 seconds.
- `ctest --test-dir out/build/x64-debug -L integration --output-on-failure
  --timeout 180`: 3/3 passed in 136.39 seconds. The realtime integration test
  passed in 85.28 seconds, MPEG-TS integration in 15.78 seconds, and crafted UDP
  fault integration in 35.33 seconds.
- A clean-first rebuild completed successfully, and the immediate rebuild check
  reported `ninja: no work to do` before the test tiers ran.
- Task-owned `git diff --check`: exit 0.
- Task-owned source and test files use UTF-8 without BOM and CRLF line endings.
- Independent final review: 0 Critical / 0 Important / 0 Minor, PASS.

## Remaining integration boundary

- This task deliberately does not create correction measurements or connect the correction producer. The owning integration task must provide contiguous commands from the shared A/V clock system and explicitly select `ExternalCorrectionRequired`.
