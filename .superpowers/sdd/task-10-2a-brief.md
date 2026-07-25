# Task 10.2A: Scheduled RTP packetization boundary

## Context

Task 10.1 provides `MediaSharedNtpEpoch`, `MediaRtpOutputClockMapper`, transactional RTCP SR scheduling, and strict RTCP generation. Task 10.2A must provide an application-owned FFmpeg RTP packetization boundary that emits validated RTP datagrams carrying the exact planned clock identity. It is a component stage only. Task 10.2B will own UDP RTP/RTCP transmission, and Task 12 will atomically wire the new scheduled path into the graph and remove the legacy RTP timing authorities.

## Required design

- Add a single-responsibility RAII custom write AVIO component under `runtime/ffmpeg`. It receives complete FFmpeg RTP mux datagrams through an injected sink, owns its AVIO buffer/context, preserves the first structured sink failure, and has explicit open/close/reset lifecycle. Its complete configuration includes `maximumDatagramBytes`; the AVIO buffer capacity, `pb->max_packet_size`, and `AVFormatContext::packet_size` must explicitly agree and exceed the RTP header size. It must not open a URL/socket, pace, sleep, or read a clock.
- Add a single-responsibility RTP datagram rewrite/validation component under `protocol/rtp`. Its complete immutable configuration requires a dynamic RTP payload type in 96..127, SSRC, and an already mapped `MediaRtpTimestamp`; no defaults or inferred values. The FFmpeg muxer is configured with the same planned payload type, so the rewriter requires an exact input payload-type match before applying the planned value idempotently. It must strictly reject RTCP, RTP version/header/extension/padding/truncation errors, payload-type mismatch, and datagrams that cannot carry the planned timestamp/SSRC. It rewrites only the planned RTP payload type, timestamp, and SSRC while preserving marker, sequence, CSRC, extension, padding, and payload bytes.
- Add a distinct scheduled FFmpeg RTP mux session under `nodes/mux`; do not add dual scheduled/legacy behavior to `RtpMuxFfmpegSession` or `RtpMuxNode`. Its complete configuration requires an explicit packetization mode with exactly the supported values H264 and AAC LATM; the mode is never inferred from codec metadata. MPEG4-GENERIC AAC is not supported by this component because FFmpeg batches small access units across write calls, which violates per-access-unit timestamp ownership. The new session owns one FFmpeg RTP output context, exactly one configured audio or video stream, and the custom AVIO. It uses FFmpeg only for codec packetization, sets and verifies planned payload type/SSRC plus `rtpflags=skip_rtcp` for H264 or `rtpflags=skip_rtcp+latm` for AAC before header writing, never enables `send_bye`, rejects missing/duplicate/inconsistent configuration, and emits rewritten RTP datagrams through the injected sink. A callback without an active access-unit rewrite configuration is an error.
- Each input access unit supplies its explicit mapped RTP timestamp. The session must not use first-packet rebasing, timestamp normalization, `+1` repair, wall/steady time, pacing, transport byte throttling, or FFmpeg-generated RTP/RTCP identity as authority.
- Packet write is transactional at the observable component boundary: a sink failure is returned to the caller and must not be reported as a successful access-unit write even when FFmpeg itself returns success. Because a fragmented access unit can partially reach the sink, any callback/sink failure permanently poisons that session; only reset/recreate is legal. Do not claim physical all-or-none delivery, retry the same FFmpeg context, or add an internal retry queue/compatibility fallback.
- Retain FFmpeg mux stream time-base handling only as required for codec packetization. Stock FFmpeg may internally create a random base timestamp/sequence and read NTP time while opening its RTP muxer; with RTCP suppressed and every emitted RTP datagram rewritten, these values are explicitly non-authoritative. The application-owned datagram rewriter is the final authority for payload type, SSRC, and RTP timestamp, and tests must use FFmpeg PTS values different from mapped RTP timestamps to prove this boundary.
- Custom AVIO cleanup must detach `AVFormatContext::pb` before freeing the format context, then free the AVIO buffer/context exactly once. Do not pass custom AVIO ownership through the existing generic output-context deleter or `avio_closep` path.
- Do not modify production builder/planner/node wiring, `RtpMuxNode`, `RtpOutputNode`, `SdpWriterNode`, or legacy timing removal in this subtask.

## TDD matrix

RED first, with the expected failure recorded in the report:

- Custom AVIO RAII: open/close/reset, explicit packet-size agreement, complete datagram delivery, preserved sink error propagation even when FFmpeg reports success, invalid buffer/sink configuration, and no callback after close.
- Datagram rewrite: minimal header, CSRC, extension, padding, marker/sequence/payload preservation, exact PT/timestamp/SSRC rewrite, wrap value, malformed/truncated input, RTP version error, and RTCP rejection.
- Scheduled mux session with real FFmpeg packetization: artificial H264 Annex-B large NAL/FU-A and AAC LATM codec parameters/ASC, explicit packetization mode plus stream-kind/time-base/codec-mode validation, `skip_rtcp`, first small AAC access unit emitted during its own write, multiple H264 RTP fragments sharing one mapped timestamp, distinct consecutive mapped timestamps, sink failure preservation plus terminal-poisoned state, duplicate lifecycle/config rejection, trailer/reset, and zero emitted RTCP datagrams across header/write/trailer.
- Compile-time/API checks must prevent default construction of complete configuration and mapped timestamp values.

## Verification and report

- Register focused tests in an existing deterministic node/runtime target without creating a mixed-purpose test executable.
- Run the focused target build and direct executable. Record exact RED and GREEN commands/results in `.superpowers/sdd/task-10-2a-report.md`.
- Self-review for RAII, single responsibility, duplicate semantics, defaults/fallback, unnecessary copies, Task 12 boundary, UTF-8 no BOM, and CRLF.
- Do not commit or push. Do not stage unrelated existing workspace files.
