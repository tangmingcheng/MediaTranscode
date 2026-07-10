# Realtime RTP Audio+Video Smoothness

## Root Cause

- Realtime RTP plans owned low-latency policies, but builders and CLI did not consistently consume them. RTP data edges could still become blocking queues, and CLI workers inherited the default `idleSleepMs=1`.
- Raw RTP AAC input always planned audio decode/resample/encode, so AAC-to-AAC realtime audio could not use packet-copy.
- Raw RTP audio+video shared one input read point. Removing blocking backpressure helped smoothness, but it did not provide true A/V input isolation.
- Realtime drop policy treated some expected drops as push failures, and keyframe-aware dropping could evict queued keyframes.
- Separate RTP output had per-stream muxers but no explicit A/V startup gate. Audio could begin before the first video packet was ready.
- Waiting only for an encoded output keyframe was insufficient for raw RTP input. If the graph joined the sender mid-stream, the video decoder could already consume damaged input packets before output gating.
- RTP output needed explicit planner-owned pacing, packet timestamp monotonicity, and a pacing session baseline that starts after the receiver-ready delay.

## Fix Summary

- Realtime plans now expose planner-owned threading policy and lane-specific edge policies. Realtime workers use `idleSleepMs=0` with bounded spin/yield, and RTP data lanes use `SpscRing`.
- Realtime builders and branch segments consume the planned edge policy set. Audio lanes use audio-appropriate overflow policy instead of video-only `DropNonKeyFrame`.
- Raw RTP AAC-to-AAC with matching sample rate, channels, and encode options plans `CopyPacket`; requested target changes still plan `TranscodeFrame`.
- Raw RTP A/V now builds isolated video and audio input/demux/split chains.
- Queue overflow drops are normal backpressure, and keyframe-aware drop preserves queued keyframes. If a full queue contains no droppable non-key packet, incoming keyframes are no longer reported as successful drops; blocking `push()` waits for space and `tryPush()` returns false.
- Separate RTP mux nodes use planner-owned pacing and monotonic packet timestamp options.
- Separate RTP A/V output now passes encoded packet streams through `AvPacketStartBarrierNode`, which opens when both first video and audio packets are available and keeps the latest pre-open packet for each stream.
- Raw RTP video transcode now inserts a planner-enabled `PacketStartGateNode` before decode. The gate drops pre-start delta packets and opens on the first keyframe so mid-stream RTP joins do not feed damaged GOP fragments into the decoder.
- RTP mux startup delay now resets both packet pacing and paced-AVIO byte pacing baselines immediately before first packet write. This prevents the delay period from turning into a catch-up burst.
- Graph fixed sleeps are covered by a unit audit. Fixed sleep remains only classified non-realtime idle backoff, and realtime execution disables it by planner policy.

## Test Commands And Results

```powershell
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
```

Result: passed after clean rebuild. The suite now covers raw RTP AAC copy/transcode decisions, isolated A/V raw RTP inputs, realtime non-blocking data edges, planner threading policy, CLI policy application, graph sleep classification, keyframe/drop semantics, full-keyframe queue pressure, video input start gate behavior, RTP mux pacing/timestamp policy, and A/V start barrier topology/behavior.

```powershell
ctest --test-dir out/build/x64-debug --output-on-failure
```

Result: passed. CTest ran `media_transcode_realtime_graph_tests` and reported `100% tests passed, 0 tests failed out of 1`.

```powershell
powershell -ExecutionPolicy Bypass -File out\build\x64-debug\codex_av_scheduler_smoke.ps1
```

Result: passed after clean rebuild.

```powershell
powershell -ExecutionPolicy Bypass -File out\build\x64-debug\codex_av_diag_smoke.ps1
```

Result: passed for a 20s RTP audio+video diagnostic chain. The CLI selected `cuda-nvenc`; `packet_start_gate.open`, `av_packet_start_barrier.open`, `rtp_mux.startup_delay_elapsed`, `rtp_mux.pacing_session_started`, and first audio/video packet writes were logged. The FFmpeg receiver log had no `RTP: missed`, no H264 decode/conceal/corrupt messages, and reported `drop_frames=0`.

```powershell
powershell -ExecutionPolicy Bypass -File out\build\x64-debug\codex_av_scheduler_smoke.ps1
```

Result: passed for a 60s hardware RTP audio+video chain after the keyframe-preserving queue fix. The CLI selected `cuda-nvenc` (`h264_cuvid` -> `h264_nvenc`), exited 0, and reported:

```text
[CLI] final runtime report: queued=41, peakQueued=41, workers=0, workerErrors=0, totalPushed=29473, totalPopped=29432, encodedPacketsPushed=8547, encodedPacketsPopped=8506, backpressureItems=34
[CLI] realtime video validation stopped successfully
```

The FFmpeg SDP receiver progress log reached `frame=1662`, `out_time=00:00:56.076190`, `speed=1.02x`, and `drop_frames=0`. The CLI and receiver logs had no `RTP: missed`, no H264 decode/conceal/corrupt messages, and no non-monotonic DTS messages.

```powershell
powershell -ExecutionPolicy Bypass -File out\build\x64-debug\codex_human_vlc_60s.ps1
```

Result: passed by human observation for a 60s visible VLC audio+video run. Observed result: audio and video appeared together, with no visible stutter, corruption, or color breakup. CLI selected the hardware chain and exited successfully:

```text
[CLI] final runtime report: queued=83, peakQueued=83, workers=0, workerErrors=0, totalPushed=29882, totalPopped=29799, encodedPacketsPushed=8682, encodedPacketsPopped=8599, backpressureItems=34
[CLI] realtime video validation stopped successfully
```

Automated VLC dummy mode is not used as the primary pass/fail signal for this issue. Its log still reports dummy/Windows video blending failures and late-picture diagnostics even when FFmpeg receiver and visible VLC playback are clean.

```powershell
git diff --check
```

Result: passed. No whitespace or line-ending errors were reported.

## Remaining Risks

- Visible VLC playback passed by observation, but the verbose VLC log still contained one `live555 debug: lost 8038 bytes` entry near VLC HTTP/update-style log output. It did not correlate with visible impairment and did not reproduce in the FFmpeg receiver smoke.
- The current smoke script starts the RTP sender before the CLI binds input sockets, which can intentionally create startup input loss. A pre-pass exposed one input-side `RTP: missed`/decoder conceal event; the immediate rerun after the same build passed cleanly. A P1 harness improvement should add an explicit input-readiness handshake for deterministic validation.
- The current start barrier intentionally keeps only the latest pre-open packet per stream. This avoids carrying old audio PTS into pacing, but a future policy could make the startup hold/drop strategy planner-configurable.
- The worker idle path still contains fixed sleep for non-realtime backoff. Realtime plans disable it now; a P1 runtime change should replace the fallback with event-driven wakeup or bounded scheduler wait.
- Output pacing is per RTP mux. It is sufficient for the current separate RTP smoke, but a future multi-stream scheduler should own cross-stream pacing policy explicitly instead of using per-node pacing clocks.
- The `PacketStartGateNode` currently relies on packet keyframe flags. If future RTP demuxers fail to mark H264 keyframes reliably, the next step should be a planner-owned codec-specific start detector that validates SPS/PPS/IDR readiness.
