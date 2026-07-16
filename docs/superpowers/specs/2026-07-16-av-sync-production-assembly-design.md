# A/V Sync Production Assembly Design

## Scope

Task 12 replaces the legacy synchronized RTP/MPEG-TS graph with one production
A/V synchronization path. It supports exactly:

1. Separate RTP video/audio input to separate RTP video/audio output.
2. MPEG-TS input to MPEG-TS output.

This task completes the initial locked generation and direct playback path.
Protocol discontinuity or `ReacquireRequired` fails closed with a structured
error. Automatic cross-generation reacquisition remains Task 13. Separate RTP
input to MPEG-TS output remains unsupported.

## Production Data Flow

```text
protocol packets + clock evidence
  -> protocol timestamp binder
  -> canonical input
  -> A/V startup coordinator
  -> playback epoch binder
  -> atomic release extractor
  -> video/audio transcode with lineage and audio correction
  -> one shared A/V output scheduler
  -> protocol output adapters
```

The planner remains the only policy owner. Builders create topology and copy a
complete plan into node options. Runtime evidence supplies observed clock
epochs and generations; runtime nodes do not invent defaults or fallbacks.

## Input Clock Binding

### Separate RTP

Each raw RTP input continues to own its RTP/RTCP transport and depacketizer.
The RTP clock group validates both SR streams and their common CNAME. A focused
RTP timestamp binder joins access units with the latest locked group snapshot,
maps each RTP timestamp through its stream calibration, and normalizes both
streams against one group source epoch. It retains bounded pre-lock input and
never substitutes arrival time.

The binder publishes timing with the common group generation. Acquiring input
waits within planner capacities. Degraded evidence is rejected for Task 12.
Invalidated, mismatched, or `ReacquireRequired` evidence terminates the group
with a structured error.

One clock-snapshot fanout distributes the same immutable locked group snapshot
to the video timestamp binder, audio timestamp binder, and startup-clock
adapter. The clock-group node remains the sole validator; consumers cannot
independently reinterpret readiness.

### MPEG-TS

`MpegTsDemuxNode` remains the protocol clock authority and attaches PCR-derived
source timing to each packet. It also publishes an immutable selected-program
clock-state event for the startup-clock adapter. The canonical stage accepts
only locked timing from the selected program and matching initial generation.
PCR-derived source time is already relative to the program generation, so no
fixed wall-clock or arrival-time epoch is introduced.

## Startup and Epoch Activation

The protocol clock adapter produces the `MediaAvStartupClockBuffer` consumed by
the coordinator. RTP readiness comes from the common clock-group snapshot; TS
readiness comes from the selected program clock state.

The release path is strictly serialized:

```text
coordinator.release
  -> playback epoch binder
  -> bound release extractor
```

The binder activates the playback epoch before forwarding the same immutable
release. Backpressure retains the release without activating it twice. The
extractor then capacity-preflights and atomically commits the released video and
audio batches.

The shared scheduler may start while the group is Registered, but it cannot
consume or dispatch media until the binder makes the group Active. It creates
its controller state exactly once from the active epoch.

## Transcode and Drift Path

Released video and audio enter the existing typed generation-lineage codec
paths. Audio passes through StartupTrim, Resample, and Encode in synchronized
mode. A focused audio drift-controller node observes active playback time and
canonical audio position, executes the planner-owned servo policy, and emits
typed correction windows to `AudioResampleNode`.

The audio drift-controller is placed before Resample. It passes decoded
canonical audio forward, reads only the registered group master clock and
active playback epoch, and emits correction commands on a separate downstream
edge to Resample. This preserves an acyclic graph; no scheduler-to-resampler
feedback edge is introduced.

Video Encode already emits canonical encoded access units. A focused encoded
audio canonicalizer converts the exact contiguous sample fragments and playback
origin into one scheduler unit with checked presentation, duration, generation,
and a monotonic encoded-audio sequence owned by that canonicalizer. It does not
discard fragment validation or reconstruct timestamps from codec packet PTS.

## Output Adapters

### Separate RTP

The shared scheduler is the only pacing authority. One scheduled-output router
validates the typed stream discriminator and sends each immutable scheduled
unit to exactly one video or audio output edge. Its RTP outputs feed two focused
scheduled RTP sender nodes. Each sender owns one
`ScheduledRtpSenderSession`, UDP RTP/RTCP transport, packetizer, counters, and
terminal lifecycle. It consumes planner-produced transport, packetization,
SSRC, base timestamp, clock rate, common CNAME, sender lead, and SR interval.

Codec configuration is supplied explicitly by the corresponding encoder
metadata output. SDP publication consumes the two opened sender descriptions
and codec configurations and atomically writes the dual-media SDP. Legacy
`RtpMux`, `RtpOutput`, per-mux pacing, timestamp repair, and
`AvPacketStartBarrier` are not present in synchronized graphs.

### MPEG-TS

A project MPEG-TS plan source publishes the immutable planner-produced runtime
plan. The same scheduled-output router feeds a scheduled TS adapter, which
converts scheduled video/audio units into typed
`MediaTsAccessUnitBuffer` objects using their canonical dispatch/presentation
times and exact codec configuration. `FileOutputNode` supplies the byte sink and
`FileMuxNode` executes `ProjectMpegTsMuxSessionAdapter`.

The TS mux does not copy input PCR jitter or infer timestamps. PCR, PTS, and DTS
all derive from the shared playback epoch and master clock.

## Failure and Lifecycle Rules

- Missing plan fields fail during planning or graph construction.
- Missing runtime clock evidence waits only within planned bounded capacity.
- Clock mismatch, generation mismatch, degradation, discontinuity, or
  reacquisition request fails closed in Task 12.
- Epoch activation, sender open, SDP publication, TS plan publication, and mux
  open are transactional and first-error preserving.
- Stop and abort clear retained media, pending releases, correction windows,
  codec delay, sender state, and protocol output state through their typed
  owners.
- No synchronized node uses a default clock, codec, queue, duration, endpoint,
  pacing interval, or generation.

## Test Strategy

Implementation begins with failing deterministic tests:

1. RTP packet plus clock-snapshot joining, common epoch, bounded acquiring, and
   fail-closed degraded/reacquire behavior.
2. TS locked timing acceptance and non-locked/generation rejection.
3. Binder activation-before-forwarding, WouldBlock retry, and activation-once.
4. Scheduler Registered-to-Active startup.
5. Encoded audio fragment-to-canonical conversion and servo correction flow.
6. Scheduled RTP and TS adapter lifecycle, configuration, backpressure, EOF,
   abort, timestamp, and protocol output tests.
7. Builder assertions for the complete synchronized topology and absence of all
   legacy synchronization authorities.
8. `buildExecutable()` and runtime registration for both supported topologies.

After clean-first deterministic and integration verification, hardware-only RTP
and MPEG-TS paths run for two minutes each. RTP is opened from the generated SDP
directly in VLC; TS is opened directly in VLC. CPU measurement targets only
`media_transcode_realtime_video_cli`.

## Deferred to Task 13

Task 13 adds group-scoped discontinuity coordination, automatic generation
purge/reacquisition, recovery metrics, and deterministic multi-generation fault
injection. Task 12 exposes explicit failure boundaries needed by that work and
does not add a compatibility bridge.
