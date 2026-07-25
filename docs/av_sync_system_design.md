# A/V Synchronization System Design

## Scope

This design replaces the current local startup-barrier and per-mux pacing behavior with one graph-level synchronization domain.

Supported synchronized topologies are deliberately limited to:

1. Separate RTP video/audio input to separate RTP video/audio output.
2. MPEG-TS input to MPEG-TS output.

Separate RTP input to MPEG-TS output is not part of this work. The planner rejects that topology instead of adding a compatibility path.

The required result is synchronized startup, stable playback, and no long-term A/V drift. RTP playback validation opens the generated SDP directly in VLC; it must not pass through a TS or Matroska capture before subjective review.

## Selected Architecture

The synchronization system uses four layers:

1. Protocol source clocks map RTP/RTCP or TS PCR timestamps into one canonical running-time domain.
2. A graph-level startup coordinator selects one common decodable start point and atomically releases both streams.
3. A steady monotonic master clock drives scheduling. An audio servo absorbs small clock-rate differences, while video follows the same timeline through hold/drop/repeat decisions.
4. Protocol output clocks derive RTP/RTCP or TS timestamps from the same canonical timeline.

An audio-only master clock was rejected because it cannot define RTP sender-report and TS PCR semantics consistently. A video/PCR-only master was rejected because it cannot absorb normal audio clock drift without audible discontinuities. The selected architecture keeps transport timing protocol-neutral while prioritizing continuous audio in the drift controller.

## Planner-Owned Model

The planner produces one complete immutable `MediaAvSyncPlan`. Builders and runtime nodes only execute it; they do not supply defaults, infer topology, or select fallback clocks.

The plan contains:

- supported topology and source clock mode;
- canonical running-time base;
- source stream identities and clock rates;
- startup wait, preroll, keyframe, trim, and maximum-skew policy;
- steady master-clock mode and output lead time;
- audio servo deadband, control windows, gains, slew rate, normal correction limit, recovery limit, and hard discontinuity threshold;
- video early-hold, late-display, drop/repeat, and maximum consecutive recovery limits;
- discontinuity thresholds and reacquisition timeout;
- RTP input SR/CNAME requirements and timeouts;
- RTP output SSRC, common CNAME, SR interval, and shared NTP epoch policy;
- TS program, stream PID, PCR PID, PCR interval, PCR gap, and PTS/DTS time base;
- required metrics and acceptance thresholds.

Planner validation requires:

- exactly one of the two supported topologies;
- explicit and valid stream clock rates and protocol identities;
- a unique TS program and PCR PID for TS;
- common RTCP CNAME and valid sender reports for guaranteed separate-RTP synchronization;
- normal audio correction not exceeding 1000 ppm;
- recovery correction not exceeding 5000 ppm;
- ordered, positive startup, recovery, and discontinuity thresholds;
- all required output clock parameters.

An incomplete or inconsistent plan fails before graph construction.

## Canonical Running Time

All media time is represented by a strong `MediaRunningTime` type using signed nanoseconds. Raw timestamp ticks must never be stored or compared without their source time base.

The time subsystem has separate responsibilities:

- `MediaTimestampUnwrapper` expands RTP 32-bit timestamps, TS 33-bit PTS/DTS, and PCR wraparound.
- `MediaRtpSourceClockMapper` maps RTP timestamps through RTCP SR RTP/NTP pairs.
- `MediaTsSourceClockMapper` maps PTS/DTS through the selected program PCR epoch.
- `MediaCanonicalTimeMapper` converts protocol source time into graph running time and tags every result with an epoch generation.
- `MediaSteadyMasterClock` exposes only monotonic graph time.
- `MediaSharedNtpEpoch` converts graph running time to NTP for RTP sender reports; it is not a scheduling clock.

A mapped timestamp contains presentation time, decode time, duration, epoch generation, source identity, and mapping confidence. Missing source evidence is a structured error. Arrival time, DTS-as-PTS, and first-packet-zero are not fallback modes.

## Separate RTP Input Clock

Each input stream tracks RTCP sender reports containing SSRC, RTP timestamp, and NTP timestamp. Audio and video are accepted into the same synchronization group only when their SDES CNAME matches.

The mapper:

1. unwraps the RTP timestamp;
2. converts it to the common NTP source domain using the latest valid SR estimate;
3. applies a robust offset/rate estimator without changing the published running-time epoch;
4. maps the common source time to canonical running time.

It rejects NTP regression, invalid RTP/NTP slope, CNAME changes, unexplained SSRC changes, and SR residuals beyond the discontinuity policy. SR timeout first enters a bounded degraded extrapolation state. Expiry of the maximum extrapolation window triggers group reacquisition. Guaranteed synchronization does not silently fall back to packet-arrival time.

## MPEG-TS Input Clock

The planner selects the program, audio/video PIDs, and PCR PID. PCR establishes the common program source clock. PTS determines presentation time; DTS determines decode order only.

PCR regression, excessive PCR gaps, TS discontinuity indicators, PTS discontinuity, program changes, and PCR PID changes trigger group-level reacquisition. Input PCR jitter is filtered for source-time estimation and is not copied into output PCR.

## Startup Coordinator

The graph-level startup state machine is:

```text
Idle -> AcquiringClock -> PrimingStreams -> Armed -> Released -> Running
```

For RTP, `AcquiringClock` requires media packets, valid SR mappings, and a common CNAME for both streams. For TS, it requires a locked program/PCR clock and valid mapped PTS for both streams.

`PrimingStreams` requires a decodable video keyframe, complete audio samples, and a common buffered time window. The common start point is the latest of the first decodable video time and first complete audio time.

Before release:

- video presentation before the common start is suppressed, while the minimum keyframe-led decode preroll required to decode the first presented frame is retained;
- audio is sample-accurately trimmed to the common start;
- both streams buffer the planner-defined preroll;
- one `MediaPlaybackEpoch` containing source start, master release time, and generation is published atomically.

The coordinator never releases only one stream. Startup timeout is an explicit runtime error.

## Master Clock and Output Scheduling

`MediaSteadyMasterClock` is the only scheduling authority. Nodes do not use `system_clock` for pacing.

A single `MediaAvOutputSchedulerNode` owns both audio and video scheduling for the synchronization group. It orders mapped access units by canonical presentation time and dispatches them to protocol muxers when the shared master clock reaches the target time. Separate RTP mux workers do not sleep independently.

Muxers consume complete scheduled packets. They do not invent duration, increment timestamps by one tick, rebase individual streams, or select pacing epochs.

## Audio Drift Servo

Audio continuity has priority for small steady clock differences. `MediaAudioDriftServo` measures audio playhead error against the master timeline and A/V phase error through a filtered PI/PLL controller with anti-windup.

The controller rules are:

- a deadband prevents correction noise;
- locked-mode correction is limited to plus or minus 1000 ppm;
- recovery-mode correction is limited to plus or minus 5000 ppm;
- correction changes are slew-limited;
- `swr_set_compensation` or an equivalent resampler interface applies the requested correction over an explicit sample window;
- timestamps are not rewritten to pretend correction occurred;
- error beyond the hard discontinuity threshold triggers reacquisition rather than more resampling.

The audio execution node receives only the requested correction and window. It does not calculate policy.

## Video Synchronization

`MediaVideoSyncController` compares mapped presentation time with the master timeline:

- early frames are held;
- frames inside the normal late window are displayed immediately without shifting later timestamps;
- recoverably late non-key frames may be dropped;
- a previous frame may be repeated only when the plan permits recovery repetition;
- exceeding the maximum consecutive recovery actions triggers group reacquisition.

Video does not use sustained speed changes to follow audio.

## Discontinuity and Reacquisition

Discontinuity handling is group-scoped. An individual audio or video stream must not independently rebase the shared clock.

Triggers include invalid RTP/SR movement, SSRC or CNAME change, TS discontinuity, PCR/PTS jump, program change, hard A/V error, or loss of queue continuity.

The recovery state path is:

```text
Running -> Suspect -> Recovering -> Running
                         |
                         v
                    Reacquiring
                         |
                         v
AcquiringClock -> PrimingStreams -> Armed -> Released -> Running
```

Reacquisition increments the generation, clears old-generation queues and servo state, closes the old playback epoch, and waits for new protocol clock evidence plus a new common decodable point. Packets from different generations are never joined.

## Separate RTP Output Contract

Both RTP streams derive timestamps from canonical running time:

```text
RTP timestamp = planned base timestamp
              + rescale(running time - output epoch, stream clock rate)
```

Both RTCP sender reports derive NTP from one `MediaSharedNtpEpoch` and the steady master clock. The output contract requires:

- distinct audio/video SSRC values;
- the same non-sensitive planner-produced CNAME;
- the same NTP epoch;
- periodic SR for both streams;
- RTP timestamp/SR correspondence for each clock rate;
- SDP containing both media sections, payload/clock mappings, control endpoints, and codec parameters.

The RTP packetizer executes the planned mapping. It does not generate its own timestamp or SR epoch.

## MPEG-TS Output Contract

The TS mux receives access units already mapped into one running-time domain. PTS and DTS are generated from canonical presentation/decode time. PCR is generated periodically from the same master timeline on the planner-selected PCR PID.

Output PCR must be monotonic, remain within the planned interval and jitter limits, and maintain a valid relation to stream PTS. The mux does not copy input PCR jitter or select program/PID/time-base policy.

## Responsibility and Replacement Boundaries

New focused components are grouped under `planner/avsync`, `time`, `sync`, `protocol/rtp`, and `protocol/mpegts`.

The synchronized A/V paths replace:

- `AvPacketStartBarrierNode` and its separate per-stream origin normalization;
- per-mux `MediaGraphPacingClock` sleeps and per-stream rebase state;
- `RtpMuxFfmpegSession` monotonic `+1 tick` repair;
- mux/output decisions for RTP timestamp, NTP epoch, SR, PCR, and start offset;
- arrival-time and first-packet-zero fallback behavior;
- hardcoded audio compensation and video lateness thresholds.

`PacketStartGateNode` remains available for valid single-video paths but is not the synchronization authority for the two A/V topologies. Byte-level transport throttling may remain as a network limiter only if it cannot become a second media clock.

## Errors and Metrics

Planner and runtime use structured synchronization errors for unsupported topology, incomplete plan, missing/expired SR, CNAME mismatch, unmappable timestamp, missing PCR, startup timeout, correction limit, recovery limit, and reacquisition timeout. Errors include topology, state, generation, stream identity, observed value, and threshold.

Required metrics include:

- synchronization state and generation;
- startup wait and initial skew;
- audio/master, video/master, and A/V phase error;
- audio correction ppm and servo mode;
- video hold/drop/repeat counts;
- discontinuity and reacquisition counts/durations;
- RTP SR age/rejection/CNAME status;
- TS PCR age/jitter and PTS/PCR offset;
- per-stream queued duration;
- p50/p95/p99 skew and maximum correction.

## Test-Driven Implementation Order

1. Timestamp unwrap and strong running-time arithmetic.
2. RTP SR/CNAME and TS PCR source-clock mappers.
3. Canonical mapper generation and discontinuity behavior.
4. Startup coordinator, common point selection, audio trim, and atomic release.
5. Audio servo deadband, PI/PLL, clamps, slew, anti-windup, and hard recovery.
6. Video hold/drop/repeat and recovery limits.
7. Shared output scheduler and master clock.
8. RTP timestamp, CNAME, common-NTP SR generation.
9. TS PCR/PTS/DTS generation.
10. Planner support matrix and complete-option validation.
11. Runtime lifecycle, reset, EOF, error, and generation isolation.
12. Metrics, deterministic fault injection, integration, and live acceptance.

Each production behavior starts with a failing deterministic test. Model-layout changes require a full clean rebuild before runtime conclusions.

## Acceptance

### Separate RTP to Separate RTP

- VLC opens the generated SDP directly; no intermediate TS/Matroska remux is used for subjective validation.
- Packet capture verifies distinct SSRC values, common CNAME, periodic SR, common NTP epoch, and correct RTP/SR clock-rate mapping.
- A flash/beep source verifies startup skew at most 40 ms, steady p95 at most 20 ms, and p99 at most 40 ms.
- Zero unexpected drop, decode error, worker error, or progress stall.
- Long-run drift slope is at most 1 ms per hour.

### MPEG-TS to MPEG-TS

- Output program/PID/PCR PID matches the plan.
- PCR is monotonic and within planned gap/jitter limits; PTS/DTS and PTS/PCR relations are valid.
- VLC plays the output TS directly.
- The same flash/beep startup and steady-state skew limits apply.
- Zero unexpected drop, decode error, worker error, or progress stall.
- Long-run drift slope is at most 1 ms per hour.

Both paths are first exercised as hardware-only smoke and reasonable-duration stability runs, as requested. Fault tests additionally cover clock skew, jitter bursts, packet loss, SR/PCR interruption, wraparound, discontinuity, keyframe delay, and wall-clock jumps.
