# Project-Owned MPEG-TS Output Design

## Scope

Replace FFmpeg `mpegtsenc` only for synchronized MPEG-TS output. The project-owned path must serialize real 188-byte transport packets whose PAT, PMT, elementary-stream PIDs, PTS, DTS, PCR, continuity counters, and output cadence are direct consequences of a complete planner product and one `MediaPlaybackEpoch`.

The supported synchronized topology remains MPEG-TS input to MPEG-TS output. Separate RTP input to MPEG-TS output remains unsupported. Task 11 implements the output protocol and node/session boundary; Task 12 performs production graph wiring and removes obsolete pacing authorities.

## Planner Contract

The planner produces one immutable `MediaTsMuxPlan`. Construction fails unless every field is present and valid:

- transport-stream ID, program number, PAT PID, PMT PID, video PID, audio PID, and PCR PID;
- PAT/PMT version and repetition interval;
- video stream type, explicit H.264 input layout (`AnnexB` or `LengthPrefixed`), NAL length width, and SPS/PPS emission policy;
- audio stream type and explicit AAC AudioSpecificConfig fields required to create ADTS headers;
- 90 kHz PTS/DTS time base, PCR interval, maximum PCR gap, and maximum PCR jitter;
- a positive `transportDecodeLead` separating packet emission/PCR time from content decode time;
- transport packet size of exactly 188 bytes, explicit initial continuity counters per planned PID, and an explicit maximum packets-per-datagram value for UDP output;
- the playback epoch and generation used by `MediaTsOutputClockGenerator`.

The PCR PID must equal a selected elementary-stream PID. PID collisions, unsupported codecs/layouts, incomplete AAC configuration, invalid repetition periods, and unsupported output transports fail during planning. Runtime components do not supply defaults or select alternatives.

## Components and Responsibilities

### Output sink

`MediaOutputByteSink` is a protocol-neutral synchronous interface with `write`, `flush`, and `close`. `FFmpegAvioOutputByteSink` owns the opened `AVIOContext` with RAII and implements only byte transport. `MediaOutputByteSinkBuffer` transfers sole sink ownership from `FileOutputNode` to the mux node.

`FileOutputNode` consumes the planner-selected URL/path and output transport parameters, opens exactly that sink, and publishes it. It does not create an `AVFormatContext`, select MPEG-TS parameters, group media packets, or generate timestamps.

The UDP sink treats `maximumPacketsPerDatagram` only as an upper bound. It may submit a shorter datagram containing one or more complete 188-byte packets when a PSI/PCR deadline, explicit flush, or finish requires delivery. It never waits past a deadline merely to fill a datagram and never splits a TS packet.

### Elementary-stream framing

`MediaTsMaterializedVideoConfig` contains the explicit H.264 layout, NAL length width, and validated SPS/PPS bytes obtained from the encoder configuration. It is runtime data, not a second policy source, and must match `MediaTsMuxPlan`. Session creation fails if the planned emission policy requires parameter-set injection but the materialized configuration does not contain both SPS and PPS.

`MediaTsH264AccessUnitFramer` consumes only the validated materialized configuration and planner policy. It validates Annex-B input or converts the declared length-prefixed layout using the exact planned NAL length width. It applies only the planned SPS/PPS emission policy and never sniffs payloads, invents parameter sets, or infers keyframes.

`MediaTsAacAdtsFramer` creates one ADTS frame from planner-supplied AAC configuration and one raw AAC access unit. It rejects unsupported MPEG version, object type, sampling-frequency index, channel configuration, and oversized frames. It does not infer configuration from payload bytes.

### PSI and PES serialization

`MediaTsPsiSerializer` serializes PAT and PMT sections, including section lengths and MPEG-2 CRC-32, exclusively from `MediaTsMuxPlan`. PSI version changes are not invented at runtime.

`MediaTsPesSerializer` creates bounded video and audio PES headers from `MediaTsPacketClock`. Video PES may use an unbounded packet length as required by the standard; audio PES length must be representable. PTS-only and PTS+DTS encodings are selected from the explicit timestamp relationship, not from arrival timing.

### Transport packetization

`MediaTsTransportPacketizer` emits exactly 188-byte packets. It owns continuity counters per PID, payload-unit-start flags, pointer fields, stuffing, adaptation fields, random-access indication, and PCR encoding. Counters begin at the planner-selected values. Payload continuity increments only for packets carrying payload; adaptation-only PCR packets do not advance the payload continuity counter.

PCR is inserted from `MediaTsOutputClockGenerator::advancePcrThrough`. Input PCR evidence is absent from the output API. Every emitted PCR retains its planned generation/master-time identity so deterministic byte parsing can validate the serialized value.

### Mux session and node lifecycle

`MediaTsAccessUnitView` is a synchronous, non-owning view containing payload, stream, generation, presentation/decode/emission master times, and an explicit `randomAccess` flag supplied by validated upstream packet metadata. Payload bytes are never scanned to decide random access.

`MediaTsMuxSession` is the only MPEG-TS output state machine. It binds one complete plan, one sink, and explicit video/audio codec configuration before opening. Its lifecycle is `start(emitOnMaster)`, `advanceThrough(emitOnMaster)`, `writeAccessUnit(unit)`, and `finish()`. `advanceThrough` emits periodic PSI/PCR even while encoded input is sparse and publishes its next required wake deadline. Before returning success, it submits every complete 188-byte packet whose deadline has arrived, even when this requires a short UDP datagram. The session emits initial PAT/PMT, serializes scheduled access units in scheduler order, emits all due PCR samples before the related access unit, and closes once after both planned streams terminate. `finish()` submits every remaining complete TS packet.

The session rejects generation changes, missing stream configuration, duplicate binding, late configuration, DTS regression, packets after EOF, partial sink writes, and any clock/serializer failure. The first failure poisons the session; close remains deterministic and idempotent for RAII cleanup.

`FileMuxNode` becomes a lifecycle adapter. It validates that all required buffers arrived, forwards scheduled units and control events to a preselected mux session, propagates backpressure/errors, and owns no PID, timestamp, codec, PSI, PCR, interleave, or fallback policy. The existing FFmpeg container path moves behind a separate `FFmpegFileMuxSession`; selecting the session implementation is a planner/runtime-factory consequence, not a node heuristic.

## Data Flow

1. The planner creates the complete TS plan and rejects unsupported combinations.
2. `FileOutputNode` opens and publishes the sole byte sink.
3. `FileMuxNode` binds the plan, sink, and exact video/audio configuration to `MediaTsMuxSession`.
4. The shared A/V scheduler publishes `MediaScheduledAccessUnit` objects with presentation, decode, and emission master times. The existing `dispatchOnMaster` remains the decode time; Task 12 adds only `emitOnMaster`, avoiding a second decode-time concept. For every unit, `dispatchOnMaster - emitOnMaster` must equal the positive planner-selected `transportDecodeLead`.
5. `MediaTsOutputClockGenerator` projects PTS/DTS from presentation/decode time and produces due PCR samples from emission time and the shared epoch.
6. Elementary framing, PES serialization, and TS packetization generate bytes; the sink writes only complete planned packet batches.
7. The existing TS parser and PSI assembler parse those bytes for deterministic conformance tests.

## Byte-Level Invariants

- Every packet is 188 bytes and begins with sync byte `0x47`; transport-error and scrambling bits are clear, and adaptation-field-control is never zero.
- PAT PID is `0x0000`; PMT, video, audio, and PCR identities exactly match the plan.
- PAT/PMT section length, `current_next_indicator`, version, program mapping, stream types, and CRC are valid.
- PES start codes, video/audio stream IDs (`0xE0`/`0xC0`), header lengths, marker bits, PTS-only/PTS+DTS flags, and payload boundaries are valid.
- H.264 PES payload is Annex-B and follows the planned SPS/PPS policy. Each AAC access unit has a valid seven-byte ADTS header whose frame length is exactly header plus payload and at most 8191 bytes.
- PTS/DTS and PCR use the same output epoch; DTS never exceeds PTS and never regresses per stream. For every access unit, `dispatchOnMaster - emitOnMaster` equals the exact planned transport decode lead.
- Every PCR packet is written for its own planned master time and encodes that time's 27 MHz system clock. Before an access unit is written, every PCR sample whose master time is at or before that unit's emission time has already been submitted.
- PCR is monotonic in extended 27 MHz time, wraps correctly on wire, and never violates the planned interval, gap, or jitter limits. The latest PCR may precede an access-unit emission by the bounded PCR cadence; it is not required to equal that emission time or the transport decode lead.
- Continuity is independent per PID and has no unexplained loss, duplication, or reset.
- Payload-unit-start is set only on PSI-section and PES starts. Final PES packet stuffing is carried in an adaptation field rather than appended to elementary payload.
- Counter and clock state advance only after the corresponding complete packet write succeeds. A sink failure poisons the session immediately; already emitted bytes are never retried or represented as rolled back.

## Verification

TDD proceeds in five independently reviewable units: typed plan, explicit framing contract, and transport decode lead; sink ownership; PSI/PES serializers and H.264/AAC framers; TS packetizer and exact PCR insertion; mux-session/node integration. Every unit starts with a failing deterministic test and ends with a focused review.

The byte gate uses the existing `MediaTsPacketParser` and `MediaTsPsiSectionAssembler` plus a test-only focused PES timestamp inspector. It validates complete generated byte sequences, not node options. Fault tests cover invalid plans, unknown/contradictory framing, missing transport lead, malformed codec payloads, wrap boundaries, generation mismatch, sparse-input PSI/PCR deadlines, sink short-write/failure, duplicate lifecycle transitions, EOF, and poisoned-session behavior.

After Task 12 wiring, acceptance runs the hardware MPEG-TS input/output path for two minutes and checks decoded audio/video, zero decode/worker/drop/continuity errors, no progress stall over five seconds, final absolute A/V drift at most 100 ms, and CPU for `media_transcode_realtime_video_cli` only. RTP acceptance remains a separate path.

## Non-Goals

- No general-purpose MPEG-TS muxer, remux compatibility layer, CBR/null-packet stuffing, multiple programs, subtitles, HEVC, or arbitrary codecs.
- No software-encoding long test in this round.
- No mixed separate-RTP-input to MPEG-TS-output topology.
- No FFmpeg `mpegtsenc` fallback when the project-owned path rejects a plan or fails at runtime.
