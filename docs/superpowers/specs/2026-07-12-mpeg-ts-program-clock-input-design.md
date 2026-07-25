# MPEG-TS Program Clock Input Design

## Goal

Provide real MPEG-TS PAT, PMT, PCR, discontinuity, PTS, and DTS evidence to the DAG through one input byte owner, then map audio and video timestamps into one source-time domain without arrival-time, wall-clock, DTS-as-PTS, PID, or epoch fallback.

## Chosen Boundary

The implementation uses one FFmpeg protocol AVIO opened with `avio_open2()` for the planned UDP URL. A caller-owned outer AVIO created with `avio_alloc_context()` is the only input presented to the explicitly selected FFmpeg MPEG-TS demuxer. Its read callback reads from the inner AVIO, observes the successfully read bytes, and returns the same bytes in the same order and length.

This is one socket and one byte consumer. It does not add a side socket, relay, duplicate reader, private FFmpeg access, or maintained FFmpeg patch. The observer never modifies input bytes and never changes a successful read result because of clock-evidence state.

The stable input capability initially supports 188-byte transport packets. A different detected stride is rejected during preflight as unsupported; the demux path must not silently accept a framing that the evidence parser cannot prove.

## Components and Responsibilities

### FFmpegObservedReadAvio

`FFmpegObservedReadAvio` owns the inner protocol AVIO, the outer custom AVIO, their buffers, and the interrupt state. It performs byte transport only and invokes the concrete MPEG-TS byte observer with immutable spans. Protocol options and demux options are separate planner products.

Its RAII release order is:

1. stop or interrupt active reads;
2. close the `AVFormatContext` that uses custom I/O;
3. free the outer AVIO buffer and context;
4. close the inner protocol AVIO.

The inner AVIO interrupt callback observes the session cancellation state. A planned read timeout is classified as waiting, not EOF. Explicit test-source closure is EOF. Stop and abort wake a blocked read and produce structured cancellation.

### MediaTsPacketParser and MediaTsPsiSectionAssembler

`MediaTsPacketParser` incrementally synchronizes 188-byte packets across arbitrary AVIO read boundaries. It parses the TS header, adaptation field, PID, continuity counter, discontinuity indicator, and full 42-bit PCR value. It reports malformed framing and continuity loss explicitly.

`MediaTsPsiSectionAssembler` reassembles PAT and PMT sections by PID and validates section length, CRC, version, `current_next`, program number, PMT PID, PCR PID, and elementary stream PID membership. It publishes immutable inventory snapshots and change events. It never chooses a program.

### MediaTsEvidenceTimeline

The timeline stores immutable evidence checkpoints indexed by absolute input byte offset. Each checkpoint contains the observed program inventory, selected-program PCR evidence, discontinuity state, and generation. Capacity and maximum supported position regression are planner products. Overflow, evidence eviction needed by a legal query, or regression beyond the planned bound fails closed.

The timeline is non-destructive: audio and video packets may be returned by FFmpeg in a position order different from the read order. A query returns the newest checkpoint whose offset is not greater than `AVPacket.pos`. Missing or negative packet position is an explicit runtime error for the synchronized MPEG-TS path.

### MediaTsProgramClockTracker

The tracker accepts only observations matching the planner-selected program, PMT PID, elementary PIDs, and PCR PID. It unwraps PCR with `MediaTimestampUnwrapper`, validates planned PCR interval, jitter residual, maximum gap, legal wrap, regression, and discontinuity generation transitions. Program or PCR PID changes that do not match the immutable plan are errors rather than replanning requests.

### MediaTsSourceClockMapper

The mapper aligns independently unwrapped 90 kHz PTS and DTS values to the selected program PCR epoch and returns strong signed-nanosecond source time. Presentation and decode time remain separate optionals. Missing PTS remains missing; DTS is never substituted for PTS. The mapper does not inspect sockets, select PIDs, or use receive time.

### MediaTsInputSession and MpegTsDemuxNode

`MediaTsInputSession` composes the observed AVIO, TS parser, PSI assembler, evidence timeline, and prepared FFmpeg format context. The prepared session is move-only and transferred once from preflight to runtime.

`MpegTsDemuxNode` replaces the generic demux node only for planner-selected synchronized MPEG-TS input. After each `av_read_frame()`, it queries evidence by `AVPacket.pos`, validates the selected stream PID, and attaches mapped source timing and generation to the graph packet boundary. It orchestrates components but owns no program selection or fallback policy.

`RealtimeInputNode` remains responsible only for transferring the complete prepared input binding. Non-MPEG-TS and unsynchronized paths retain their existing demux nodes.

## Planner Contract

Preflight builds a program inventory from public `AVProgram` and `AVStream` fields and cross-checks it with the primary PAT/PMT parser snapshot. Video and audio must belong to one program. Exactly one program containing the selected streams must be identified; missing or ambiguous inventory is a planning error.

The immutable plan carries:

- program number and PMT PID;
- video, audio, and PCR PIDs;
- transport packet size, fixed to 188 for this stable capability;
- protocol URL and protocol I/O options;
- demux options;
- AVIO buffer and maximum datagram sizes;
- evidence timeline capacity and maximum packet-position regression;
- PCR interval, jitter residual, maximum gap, and discontinuity policy;
- lifecycle and read-timeout policy.

The builder serializes every required value. Missing values fail before node construction. Nodes do not infer standard PID values, choose the first program, reuse decoder time bases as PCR policy, or copy defaults from FFmpeg.

## Data Flow

```text
planner-owned UDP protocol options
              |
              v
     inner FFmpeg protocol AVIO
              |
              v
       FFmpegObservedReadAvio
          |             |
          |             +--> immutable bytes --> TS parser --> PSI/PCR evidence timeline
          v
 explicit MPEG-TS demuxer
          |
          v
 AVPacket + packet.pos + independent PTS/DTS
          |
          v
 timeline query --> clock tracker --> source clock mapper
          |
          v
 packet timing + readiness generation
```

FFmpeg read-ahead and `avformat_find_stream_info()` packet caching are expected. Evidence is therefore correlated by byte position, never by the latest observed PCR or callback time.

## Lifecycle and Error Semantics

- Preflight and runtime share one move-only prepared session; runtime does not reopen the URL.
- Start validates all required plan products before activating the session.
- Stop and abort signal the AVIO interrupt state and unblock protocol reads.
- Reset clears node-local execution state but does not reopen or silently recreate a consumed prepared session; a new run requires a new preflight binding.
- EOF is propagated only from explicit source closure.
- Timeout becomes waiting and schedules another event-driven attempt without busy polling.
- Parser, PSI, timeline, PID, PCR, and mapping failures retain structured error categories and are not reduced to an FFmpeg error string.
- PAT/PMT version or selected PID changes, PCR regression, excessive gap, discontinuity, and timeline invalidation advance generation and require reacquisition according to the plan.

## Test Design

Deterministic tests use valid-CRC PAT/PMT/PCR/PES transport bytes and the same production parser/session interfaces. They cover:

1. arbitrary AVIO read fragmentation and 188-byte resynchronization;
2. PAT/PMT section assembly, CRC, version, `current_next`, and multi-program ambiguity;
3. planner-selected program, PMT, video, audio, and PCR PID locking;
4. independent PTS presentation and DTS decode mapping, including missing PTS;
5. PCR and PTS/DTS wrap;
6. PCR interval jitter, skipped legal intervals, excessive gap, and regression;
7. adaptation discontinuity, continuity loss, program change, and PCR PID change;
8. read-ahead, preflight packet caching, packet-position rollback, negative position, and timeline overflow;
9. timeout, EOF, stop, abort, reset, and RAII release ordering;
10. no arrival-time, wall-clock, latest-PCR, DTS-as-PTS, PID, or program fallback.

Integration tests run a real FFmpeg UDP MPEG-TS sender through the production observed-AVIO path and `av_read_frame()`. They prove one socket owner, complete audio/video packets, real PCR evidence, bounded callback/worker activity while idle, and clean cancellation. Crafted real transport bytes inject discontinuity and table changes through the same production path.

## Rejected Alternatives

### FFmpeg public evidence extension

An opt-in MPEG-TS packet observer or side-data API inside libavformat could provide post-resynchronization evidence, but it requires an external API/ABI patch, rebuilt DLLs, deployment coordination, and long-term maintenance. The public custom-AVIO design satisfies the current requirement without that dependency.

### Application-owned full MPEG-TS demux

Replacing libavformat PAT/PMT/PES demux would provide complete ownership but duplicate a mature demuxer and substantially increase protocol, codec, and security risk. The chosen design parses only the clock and identity evidence that FFmpeg does not expose.

## Task Boundary

This design implements Task 4 input evidence and source-clock mapping only. It does not implement canonical playback epoch selection, packet gating, A/V atomic release, audio drift correction, output RTP clocks, or output MPEG-TS PCR generation. Those remain in their existing later tasks.
