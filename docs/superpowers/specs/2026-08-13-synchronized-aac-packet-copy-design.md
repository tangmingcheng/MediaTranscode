# Synchronized AAC Packet Copy Design

## Objective

Realtime audio branch selection must be decided exclusively from typed source facts, explicit output requests, and the selected output protocol contract. `AudioVideo` synchronization is not itself a reason to decode and re-encode AAC. Windows and Linux/RKMPP use the same planner, builders, scheduler, and MPEG-TS mux product; only genuine platform APIs and hardware backends may differ.

## Current Defect

`MediaRealtimeRtpTranscodePlanner` currently sets `requireFrameTranscode` for every `AudioVideo` request. The synchronized builder then rejects packet copy because correction and lineage are modeled only for decoded audio. This hides an architectural gap by permanently selecting `AudioDecode -> AudioResample -> AudioEncode`, including when source and output AAC contracts match.

Project MPEG-TS also hard-codes AAC-LC 48 kHz stereo. MPEG-TS/ADTS can represent other supported AAC sampling rates, so 48 kHz is not a platform constraint. Together with explicit CLI bitrate requests, this prevents legitimate packet copy and raises CPU on both Windows and RKMPP.

## Planner Authority

The audio planner compares these facts before DAG construction:

- codec and executable profile;
- sample rate and channel layout;
- an explicitly requested bitrate or encoder-only control;
- codec frame size and packet layout required by the selected output;
- output-container restrictions that are actual protocol requirements.

An exact match produces `CopyPacket`. Any required codec, profile, sample-rate, channel, bitrate, frame-layout, or encoder-control change produces `TranscodeFrame`. Missing or inconsistent facts fail planning. Runtime nodes never change the selected branch and never fall back.

The realtime CLI must not inject an audio bitrate merely to obtain a working chain. An explicit bitrate remains authoritative and intentionally forces transcode when it differs from the source.

## Synchronized Copy Product

The synchronized audio plan has two typed component-bound products:

- packet-copy bounds, derived from the validated input access-unit duration and packet/scheduler queue capacities;
- frame-transcode bounds, derived from selected decoder, resampler, encoder, correction mailbox, and their sample domains.

Packet copy reuses the existing raw RTP preparation, source-clock binding, generation gate, canonical startup release, A/V scheduler, and output mux/packetizer. It preserves the released encoded AAC packet and its generation/timestamp lineage. It does not create an audio decoder, resampler, encoder, or sample-correction node. Source RTP/RTCP clock mapping and the canonical scheduler remain responsible for output timing; no packet-domain substitute silently imitates sample correction.

Frame transcode keeps the existing audio correction path. The builder maps only the planner-selected typed product. A copy plan presented with correction-node requirements, or a transcode plan without complete component facts, fails before runtime start.

## MPEG-TS AAC Contract

Project MPEG-TS continues to require AAC-LC and the supported channel contract. Its ADTS sampling-frequency index is derived from the resolved audio output instead of a fixed 48 kHz constant. Unsupported AAC sample rates fail planning. PMT/PES/PCR scheduling remains unchanged.

The existing H.264-only MPEG-TS video restriction is outside this focused change; HEVC MPEG-TS support remains a separate required work item in the RKMPP plan.

## Cross-Platform Isolation

All planner, component-bound, branch-builder, scheduler, AAC framing, and MPEG-TS changes are shared. No `#ifdef`, RKMPP-only option, Linux fallback, or Windows compatibility branch is introduced. Platform-specific socket and hardware codec/filter implementations are not changed by this design.

Every shared failure is validated on Windows with a source that exposes it and on RKMPP with the same media facts. A platform-only failure must be traced to an operating-system API, driver, hardware backend, or platform library before separate code is allowed.

## Verification

Temporary TDD probes are permitted only for red/green evidence and are deleted before delivery. They cover:

- matching AAC-LC source/output facts select `CopyPacket`;
- 44.1 kHz to an explicit 48 kHz output selects `TranscodeFrame`;
- an explicit differing bitrate selects `TranscodeFrame`;
- synchronized copy produces copy component bounds and no correction/codec nodes;
- supported resolved AAC sample rates generate the correct ADTS index;
- unsupported or incomplete facts fail before DAG start.

Final acceptance uses production CLIs and real media only. Windows runs absolute-path FFmpeg, CLI, and visible VLC commands. RKMPP may use a temporary target-side script invoked over SSH. Neither environment may reduce source resolution, duration, frame rate, bitrate, codec conversion, scaling requirement, or acceptance criteria to hide a defect. The final RKMPP test remains 2560x1440 to 1280x720 with strict RKMPP video processing, separate RTP input with automatic video fmtp, and MPEG-TS/RTP output, while recording process/thread CPU, RSS, queues, A/V drift, packet-copy/transcode selection, and exit reason.
