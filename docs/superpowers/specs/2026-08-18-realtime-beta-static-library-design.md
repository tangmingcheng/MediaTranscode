# Realtime Beta Static Library Design

## Scope

This is a temporary Beta test interface for rapid external use. It is not the
future stable SDK and makes no source or binary compatibility promise across
releases. Callers rebuild against the matching header and static library.

The first release supports exactly one route:

- one VideoOnly RTP input socket;
- H.264 or HEVC input with automatic same-socket signaling discovery;
- video frame transcoding to H.264 or HEVC;
- optional resolution and frame-rate conversion requested by the caller;
- CBR or VBR encoding;
- MPEG-TS over RTP output to one destination.

It does not accept audio, SDP paths, manual FMTP, generic URLs, RTSP, MPEG-TS
input, separate output RTP elementary streams, CRF, packet copy, or custom I/O.
Supplying an unsupported enum, field combination, or incomplete fact fails
before starting the asynchronous session.

## Dependency Boundary

The Beta interface is an external wrapper at the same architectural level as
the realtime CLI. No Beta header or implementation is placed under
`src/internal/graph`, and graph code never references the Beta layer.

```text
public C caller -----------+
                           +--> external realtime run controller
realtime CLI --------------+          |
                                      v
                         existing planner -> builder -> production DAG
```

A focused controller under `src/application/realtime` owns only the existing
external orchestration sequence: preflight, executable build, runtime compile,
runtime-node registration, threaded start, progress/report collection,
completion, stop, and reset. The realtime CLI and Beta session call the same
controller so lifecycle logic is not copied. The controller is not a graph
node, planner product, builder, edge, or runtime component.

The Beta layer maps public POD facts into the existing typed request. It does
not parse CLI text and does not implement a second planner, builder, media
path, codec lifecycle, or fallback. Linux maps internally to typed
`RKMPP required`; Windows maps internally to typed `Auto`. The public API does
not expose hardware backend selection.

## Delivery and Language Boundary

The release artifacts are static libraries:

- Windows: `media_transcode_beta.lib`;
- Linux and RKMPP: `libmedia_transcode_beta.a`.

One public C header is callable from both C and C++:

```text
include/media_transcode_beta/realtime.h
```

It uses `extern "C"` when compiled as C++, opaque handles, fixed-width integer
types, enums, POD structs, function pointers, and caller-owned input memory.
It never exposes C++ exceptions, STL, FFmpeg types, graph types, allocator
objects, or platform socket handles. The library catches every exception at
the C boundary.

Because the Beta is statically linked and rebuilt as one version with its
caller, public structs do not contain `struct_size` or `api_version`. Callers
never calculate structure sizes or string lengths. The later formal SDK will
design compatibility independently and will not inherit this Beta contract.

## Public Facts

All text inputs are ordinary null-terminated `const char*`. `start` validates
and deep-copies them before returning. `media_id`, bind address, and destination
address must be non-null and non-empty. Address parsing and family consistency
are validated before asynchronous work begins.

```c
typedef enum mt_beta_video_codec {
    MT_BETA_VIDEO_CODEC_H264 = 1,
    MT_BETA_VIDEO_CODEC_HEVC = 2
} mt_beta_video_codec;

typedef struct mt_beta_rtp_video_input {
    const char* bind_address;
    uint16_t port;
    mt_beta_video_codec codec;
    uint8_t payload_type;
    uint32_t clock_rate;
} mt_beta_rtp_video_input;
```

The caller reads codec, payload type, and clock rate directly from signaling.
The API has no FMTP or SDP field. The library binds one socket, observes the
actual RTP stream, automatically discovers required H.264 SPS/PPS or HEVC
VPS/SPS/PPS, seals the observation before planner finalization, and replays the
retained data through that same socket/session. It does not ask the caller to
derive codec configuration.

Rate control is a tagged union:

```c
typedef enum mt_beta_rate_control_mode {
    MT_BETA_RATE_CONTROL_CBR = 1,
    MT_BETA_RATE_CONTROL_VBR = 2
} mt_beta_rate_control_mode;

typedef struct mt_beta_cbr {
    uint64_t bitrate_bps;
} mt_beta_cbr;

typedef struct mt_beta_vbr {
    uint64_t target_bitrate_bps;
    uint64_t min_bitrate_bps;
    uint64_t max_bitrate_bps;
} mt_beta_vbr;

typedef struct mt_beta_video_output {
    const char* destination_address;
    uint16_t destination_port;
    mt_beta_video_codec codec;
    uint32_t width;
    uint32_t height;
    uint32_t frame_rate_num;
    uint32_t frame_rate_den;
    uint32_t gop_frames;
    mt_beta_rate_control_mode rate_control_mode;
    union {
        mt_beta_cbr cbr;
        mt_beta_vbr vbr;
    } rate_control;
} mt_beta_video_output;

typedef struct mt_beta_realtime_config {
    const char* media_id;
    mt_beta_rtp_video_input input;
    mt_beta_video_output output;
} mt_beta_realtime_config;
```

CBR reads only `rate_control.cbr.bitrate_bps`. VBR reads only the VBR member
and requires `min <= target <= max`, all nonzero. Width, height, frame-rate
parts, GOP, port, payload type, and clock rate are validated as direct caller
facts; hardware-specific alignment remains a capability check and is never
silently adjusted.

MPEG-TS/RTP payload type, clock, TS/RTP/IP overhead, packetization, output
description, and socket construction are library protocol facts. The caller
does not calculate or provide them.

## Intentionally Hidden Beta Profile

The user explicitly authorizes a temporary fixed profile so this wrapper can
be delivered without changing DAG planners or runtime nodes. The exception is
isolated to:

```text
src/media_transcode_beta/MediaRealtimeBetaFixedProfile.h
src/media_transcode_beta/MediaRealtimeBetaFixedProfile.cpp
```

The initial profile contains the current working test values:

```text
metadata queue                       1
packet queue                         256
frame queue                          128
mux queue                            256
startup maximum video unit bytes     4194304
open timeout milliseconds            30000
read timeout milliseconds            2000
analyze duration microseconds         5000000
probe size bytes                      5000000
MPEG-TS/RTP packet size bytes         1328
progress timeout milliseconds        10000
first-output timeout milliseconds    30000
poll interval milliseconds           100
low latency                           true
stream set                            VideoOnly
input layout                          SeparateRtp
output layout                         MpegTs
output transport                      Rtp
```

These values are not public fields and cannot be overridden. They appear in no
other Beta source. Startup diagnostics report the Beta profile identity and
every applied fixed value. This is acknowledged technical debt, not an
industrial resource-admission solution and not a claim of suitability for
arbitrary sources or production deployment. Removing/replacing the Beta API
removes the profile. The formal API must use planner-derived resource products
and cannot reuse these constants.

## Asynchronous API and Ownership

The API has four functions:

```c
typedef struct mt_beta_realtime_session mt_beta_realtime_session;
typedef struct mt_beta_realtime_callbacks mt_beta_realtime_callbacks;
typedef struct mt_beta_realtime_snapshot mt_beta_realtime_snapshot;

typedef enum mt_beta_status {
    MT_BETA_STATUS_OK = 0,
    MT_BETA_STATUS_INVALID_ARGUMENT = 1,
    MT_BETA_STATUS_ALLOCATION_FAILED = 2,
    MT_BETA_STATUS_INVALID_STATE = 3,
    MT_BETA_STATUS_INTERNAL_ERROR = 4
} mt_beta_status;

mt_beta_status mt_beta_realtime_start(
    const mt_beta_realtime_config* config,
    const mt_beta_realtime_callbacks* callbacks,
    mt_beta_realtime_session** session);

mt_beta_status mt_beta_realtime_request_stop(
    mt_beta_realtime_session* session);

mt_beta_status mt_beta_realtime_get_snapshot(
    mt_beta_realtime_session* session,
    mt_beta_realtime_snapshot* snapshot);

void mt_beta_realtime_release(
    mt_beta_realtime_session** session);
```

`start` validates and deep-copies public inputs, allocates the session, and
starts the serialized event thread. Preflight, capability discovery, DAG
construction, and runtime start continue asynchronously. The direct return
reports only immediate invalid arguments, allocation failure, or failure to
create the session/event thread. All later results are events.

`request_stop` is nonblocking, idempotent, and callable from the event callback.
It uses the existing runtime interruption and coordinated completion path.
`get_snapshot` copies one internally consistent POD snapshot and does not block
the DAG. `release` requests stop if needed, wakes waits, joins every worker and
the event thread, releases sockets/runtime/prepared input through RAII, frees
the session, and writes null to the caller's handle. It is forbidden from the
event callback because that would self-join. After `release` returns, no
callback may occur.

The callback's `user_data` must remain alive until `release` returns. The
session owns all sockets. Callers provide only bind and destination addresses
and ports; external socket handles, buffer sizes, and ownership callbacks are
not accepted.

The existing MPEG-TS/RTP path requires an SDP output path. The Beta layer
creates a session-unique file in the operating-system temporary directory,
passes that external artifact path through the existing request, reads the
completed description for `OUTPUT_READY`, and removes the exact file through
RAII during release. The public API never exposes a path, and the graph is not
changed for this adaptation.

## Events, Errors, and Snapshot

There is one serialized callback on one library-owned event thread:

```c
typedef enum mt_beta_realtime_state {
    MT_BETA_REALTIME_STARTING = 1,
    MT_BETA_REALTIME_RUNNING = 2,
    MT_BETA_REALTIME_STOPPING = 3,
    MT_BETA_REALTIME_COMPLETED = 4,
    MT_BETA_REALTIME_FAILED = 5
} mt_beta_realtime_state;

typedef enum mt_beta_error_code {
    MT_BETA_ERROR_NONE = 0,
    MT_BETA_ERROR_INVALID_ARGUMENT = 1,
    MT_BETA_ERROR_ALLOCATION_FAILED = 2,
    MT_BETA_ERROR_UNSUPPORTED = 3,
    MT_BETA_ERROR_HARDWARE_UNAVAILABLE = 4,
    MT_BETA_ERROR_IO_FAILURE = 5,
    MT_BETA_ERROR_FFMPEG_FAILURE = 6,
    MT_BETA_ERROR_CANCELLED = 7,
    MT_BETA_ERROR_INTERNAL = 8
} mt_beta_error_code;

typedef enum mt_beta_failure_stage {
    MT_BETA_FAILURE_NONE = 0,
    MT_BETA_FAILURE_INPUT_VALIDATION = 1,
    MT_BETA_FAILURE_SESSION_CREATION = 2,
    MT_BETA_FAILURE_PREFLIGHT = 3,
    MT_BETA_FAILURE_CAPABILITY = 4,
    MT_BETA_FAILURE_GRAPH_BUILD = 5,
    MT_BETA_FAILURE_RUNTIME_START = 6,
    MT_BETA_FAILURE_RUNTIME_EXECUTION = 7,
    MT_BETA_FAILURE_STOP = 8,
    MT_BETA_FAILURE_RELEASE = 9
} mt_beta_failure_stage;

typedef enum mt_beta_completion_reason {
    MT_BETA_COMPLETION_NONE = 0,
    MT_BETA_COMPLETION_SOURCE_COMPLETED = 1,
    MT_BETA_COMPLETION_REQUESTED_STOP = 2,
    MT_BETA_COMPLETION_SOURCE_LOSS = 3,
    MT_BETA_COMPLETION_STARTUP_FAILURE = 4,
    MT_BETA_COMPLETION_RUNTIME_FAILURE = 5
} mt_beta_completion_reason;

typedef struct mt_beta_realtime_event mt_beta_realtime_event;

typedef void (*mt_beta_realtime_event_callback)(
    void* user_data,
    const mt_beta_realtime_event* event);

struct mt_beta_realtime_callbacks {
    mt_beta_realtime_event_callback on_event;
    void* user_data;
};
```

No DAG/node lock is held while invoking it. The callback may call only
`request_stop` and `get_snapshot`. Event types are `STATE_CHANGED`,
`OUTPUT_READY`, `ERROR`, and `COMPLETED`. Event strings are borrowed until the
callback returns; callers copy any text they retain. `OUTPUT_READY` supplies
the generated MPEG-TS/RTP session description. `ERROR` carries stable Beta
error code, native code, failure stage, and diagnostic text. `COMPLETED`
carries the final reason. The C boundary never returns library-allocated text
that the caller must free.

The event is one simple discriminated POD. Fields irrelevant to its `type` are
zero or null:

```c
typedef enum mt_beta_realtime_event_type {
    MT_BETA_EVENT_STATE_CHANGED = 1,
    MT_BETA_EVENT_OUTPUT_READY = 2,
    MT_BETA_EVENT_ERROR = 3,
    MT_BETA_EVENT_COMPLETED = 4
} mt_beta_realtime_event_type;

struct mt_beta_realtime_event {
    mt_beta_realtime_event_type type;
    mt_beta_realtime_state state;
    mt_beta_error_code error_code;
    mt_beta_failure_stage failure_stage;
    mt_beta_completion_reason completion_reason;
    int32_t native_code;
    const char* detail;
    const char* output_description;
};
```

Stable enums distinguish validation, preflight, capability, graph build,
runtime start, runtime execution, stop, and release stages. Completion reasons
distinguish source completion, requested stop, source loss, runtime failure,
and startup failure; cancellation is never relabeled as success.

The state machine is:

```text
Starting -> Running -> Stopping -> Completed
    |           |          |          |
    +-----------+----------+--------> Failed
```

The snapshot mirrors facts already available from preflight and
`MediaGraphRuntimeReport`; the Beta layer does not add node telemetry or invent
missing values:

```c
typedef enum mt_beta_selected_backend {
    MT_BETA_BACKEND_UNKNOWN = 0,
    MT_BETA_BACKEND_NONE = 1,
    MT_BETA_BACKEND_D3D11VA = 2,
    MT_BETA_BACKEND_QSV = 3,
    MT_BETA_BACKEND_CUDA = 4,
    MT_BETA_BACKEND_VAAPI = 5,
    MT_BETA_BACKEND_RKMPP = 6,
    MT_BETA_BACKEND_VIDEOTOOLBOX = 7,
    MT_BETA_BACKEND_MEDIACODEC = 8
} mt_beta_selected_backend;

typedef enum mt_beta_selected_filter {
    MT_BETA_FILTER_UNKNOWN = 0,
    MT_BETA_FILTER_NONE = 1,
    MT_BETA_FILTER_RGA = 2,
    MT_BETA_FILTER_HARDWARE = 3
} mt_beta_selected_filter;

struct mt_beta_realtime_snapshot {
    mt_beta_realtime_state state;
    mt_beta_completion_reason completion_reason;
    mt_beta_selected_backend selected_backend;
    mt_beta_video_codec input_codec;
    mt_beta_video_codec output_codec;
    mt_beta_selected_filter selected_filter;
    uint8_t zero_copy_planned;
    uint64_t running_time_ms;
    uint64_t queued_buffers;
    uint64_t peak_queued_buffers;
    uint64_t worker_progress;
    uint64_t worker_process_calls;
    uint64_t worker_waits;
    uint64_t worker_wakeups;
    uint64_t worker_errors;
    uint64_t stalled_intervals;
    uint64_t total_pushed;
    uint64_t total_popped;
    uint64_t dropped_buffers;
    uint64_t encoded_packets_pushed;
    uint64_t encoded_packets_popped;
    uint64_t working_set_bytes;
    uint64_t peak_working_set_bytes;
    uint32_t logical_processor_count;
    double average_process_machine_cpu_percent;
    double peak_process_machine_cpu_percent;
    double average_process_single_core_cpu_percent;
    double peak_process_single_core_cpu_percent;
};
```

Backend and filter enums represent the existing selected planner product; they
do not let the caller choose it. Actual detailed DRM PRIME/upload/download and
buffer-identity evidence remains in existing graph diagnostics until the DAG
itself provides those facts through a shared report. The Beta API must not
scrape log text or report planned zero copy as observed zero copy.

No high-frequency frame/packet callback is exposed. This avoids turning the
test interface into a CPU-amplifying telemetry path. CPU fields have explicit
different units so task-manager machine percentage and single-core equivalent
cannot be compared as if they were the same metric.

## Build and Verification

CMake adds one static `media_transcode_beta` target and installs/exports no
dynamic-library symbols. The target links the existing core and the external
run controller. The existing realtime CLI is changed only above the graph to
use that same controller; its CLI parameters and observable route remain
available for regression testing.

Temporary C and C++ consumers prove header compatibility, start, snapshot,
stop, release, callback ordering, deep-copy behavior, and exception isolation.
They are used for RED/GREEN only and all source, targets, binaries, PDB/ILK,
and generated files are deleted before delivery.

Final verification uses strict Release builds. Windows exercises the existing
Auto selection; RKMPP builds after `ffenv on` with 8 parallel jobs and exercises
RKMPP-required selection. Real acceptance uses a single H.264 or HEVC RTP video
input, H.264/HEVC conversion in both directions, CBR and VBR, resolution
conversion, and MPEG-TS/RTP output. It records exact consumer/source/VLC
commands, PIDs, generated output description, picture quality, stalls, CPU in
both units, RSS, existing runtime counters, graph zero-copy diagnostics, and
truthful termination.
No source resolution, frame rate, bitrate, duration, or required transcode step
may be reduced to obtain a pass.

The frozen implementation is reviewed by two independent agents. Both must
report zero Critical and zero Important findings before the Beta library is
accepted.
