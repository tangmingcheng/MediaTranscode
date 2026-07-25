# Task 10.2B: Scheduled RTP sender and transactional RTCP dispatch

## Context

Task 10.1 provides the shared NTP epoch, RTP clock mapper, strict RTCP SR/SDES/BYE generator, and transactional SR schedule. Task 10.2A provides an application-authoritative H264/AAC-LATM RTP packetization session with synchronous datagram sink and terminal poisoning after RTP sink failure. Task 10.2B composes those primitives into one single-stream sender session. Actual UDP socket ownership remains a separate protocol-I/O adapter for Task 10.2C; graph/planner production wiring and legacy removal remain Task 12.

## Required design

- Add a distinct single-responsibility sender-session orchestrator under `nodes/mux`; do not add scheduled behavior to legacy `RtpMuxNode`, `RtpOutputNode`, or `RtpMuxFfmpegSession`.
- Its complete immutable configuration requires a move-owned `ScheduledRtpMuxStreamConfig`, `MediaSharedNtpEpoch`, `MediaRtpOutputClockMapper`, `MediaRtcpSenderReportSchedule`, common CNAME, and the schedule generation. Reject zero/mismatched generation, mapper clock/stream mismatch, mapper/base/SSRC inconsistencies, invalid CNAME, or any incomplete input before opening FFmpeg.
- The session receives separate synchronous RTP and RTCP datagram sinks. It is single-thread confined and explicitly non-reentrant: an operation guard covers the complete AU callback and SR prepare/send/commit regions, and any same-thread sink reentry is rejected before state mutation. Reject empty sinks before opening FFmpeg.
- Sending an access unit requires an explicit canonical presentation `MediaRunningTime`. Map that time with `MediaRtpOutputClockMapper` and pass the resulting `MediaRtpTimestamp` to Task 10.2A. FFmpeg packet PTS/DTS never becomes output RTP clock authority.
- Count RTCP sender packet/octet counters only after each rewritten RTP datagram is successfully accepted by the RTP sink. Before invoking the external sink, preflight both increments against the RTCP uint32 wire limit; after sink success, commit both counters atomically. Packet count is RTP datagram count, including every H264 fragment. Octet count is RTP payload-format bytes, including FU/AAC-LATM format headers while excluding RTP header, CSRC, extension, and padding. Counter overflow fails before the sink and before mutation; no wrapping/truncation.
- Extend the existing rewriter result, without adding a second RTP parser/validator, to return the validated media-payload octet count while writing into the reusable scratch buffer. Task 10.2A exposes a typed rewritten-datagram sink carrying that metadata; keep one rewriter API and no per-datagram allocation.
- Periodic SR dispatch uses `MediaRtcpSenderReportSchedule::prepare(now,generation)` without state mutation. If not due, return an explicit typed `NotDue` result. If due, map the decision's exact `reportInstant` through the same NTP epoch and RTP mapper, serialize SR+SDES with the current successful RTP counters, synchronously send it to the RTCP sink, then and only then commit the schedule token and return typed `Sent` diagnostics.
- The RTCP sink contract defines returned `Status` failure as not accepted with zero external side effect; only that failure preserves the schedule deadline and remains retryable. A thrown sink exception has ambiguous external side effect and permanently poisons the sender. A successful RTCP send followed by an impossible/stale commit is likewise inconsistent and permanently poisons the sender. RTP sink/packetizer/counter failure permanently poisons the sender until full recreation. Preserve the first structured terminal error.
- The returned SR dispatch diagnostics must contain scheduled deadline, actual report instant, next deadline, lateness, skipped intervals, NTP wire value, RTP wire value, and sender counters; it must not expose a forgeable commit token.
- No sleep, pacing, system/steady clock read, socket/URL open, default/fallback epoch, first-packet rebase, `+1` repair, retry queue, or hidden time inference.
- Do not implement SDP, BYE lifecycle, UDP sockets, graph registration, planner option projection, or legacy timing removal in this subtask.
- Correct Task 10.2A AVIO ownership before composition: add a lifecycle regression and release the current caller-provided `AVIOContext::buffer` with `av_freep` before `avio_context_free`, matching the existing observed-read AVIO ownership pattern.

## TDD matrix

RED first and record the expected failure:

- Complete config rejects missing/invalid CNAME, zero or mismatched generation, invalid mode/clock pairing, and mapper/packetizer clock-rate mismatch before session open.
- H264 FU-A counts every successfully delivered fragment and exact reassembled media payload octets; AAC-LATM counts the first and consecutive access units independently.
- RTP sink failure after partial FU-A delivery counts only prior successful fragments, returns the first sink error, and poisons all later AU/SR operations.
- Counter boundary tests cover exact `uint32_t` wire maximum accepted in SR and next increment overflow rejected before mutation. Provide a deterministic counter-seed seam only through complete construction if needed; do not add test-only mutators.
- SR not-due sends nothing and does not mutate schedule; due SR contains exact SSRC/CNAME/NTP/RTP/counters and commits once; next call before deadline is not due.
- RTCP sink failure sends once, does not advance deadline, and retry uses the same scheduled deadline but the retry call's actual report instant/current counters. Successful retry commits exactly once. Duplicate/stale schedule progression is rejected.
- Same master instant maps to the exact RTP timestamp used for SR and the exact NTP epoch value; include RTP wire wrap.
- Compile-time checks prevent default construction of complete config/dispatch diagnostics and prevent external commit-token construction.

## Verification and report

- Add focused deterministic tests to the existing node target.
- Run focused build and direct test executable. Record exact RED/GREEN commands and results in `.superpowers/sdd/task-10-2b-report.md`.
- Self-review RAII, single responsibility, planner-only explicit values, counter overflow, exact-once schedule, unnecessary copies, Task 10.2C/12 boundaries, UTF-8 no BOM, and CRLF.
- Do not stage, commit, or push; do not touch unrelated workspace changes.
