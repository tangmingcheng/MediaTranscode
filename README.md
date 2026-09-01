# MediaTranscode

MediaTranscode is a C++20 and FFmpeg-backed media transcode framework centered on the graph DAG architecture.

The active code path is the graph runtime:

```text
src/internal/graph/        # graph model, planner, builder, runtime, and runtime nodes
tools/local_video_cli/     # local-file video transcode CLI
tools/realtime_video_cli/  # realtime video transcode CLI
include/media_transcode/
    Result.h               # shared Result<T> and ErrorInfo type
```

Only the graph architecture and `Result.h` are documented as active project surfaces.

## Build

Use the existing CMake flow:

```bash
cmake -S . -B out/build/x64-debug
cmake --build out/build/x64-debug --target media_transcode_core
cmake --build out/build/x64-debug --target media_transcode_local_video_cli
cmake --build out/build/x64-debug --target media_transcode_realtime_video_cli
```

Useful CMake options:

```text
MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS=ON
```

## Realtime DAG Path

The realtime CLI accepts URL/RTSP, separate H.264/AAC RTP, or MPEG-TS/UDP input. Input clock selection and output protocol selection are independent; every A/V input reaches the same canonical scheduler before exactly one output adapter is assembled.

```text
URL/RTSP, separate RTP, or MPEG-TS/UDP input
    -> planner-owned input clock
    -> shared A/V startup, drift, recovery, and scheduler
    -> separate RTP | MPEG-TS/UDP | MPEG-TS/RTP
```

`--output-layout` and the mandatory `--output-transport` select one of three exact modes:

```text
--output-layout separate --output-transport rtp  per-media RTP/RTCP plus SDP
--output-layout mpegts  --output-transport udp  Project MPEG-TS in UDP datagrams
--output-layout mpegts  --output-transport rtp  Project MPEG-TS over RTP/AVP plus SDP
```

`separate + udp` is rejected. MPEG-TS/RTP uses the static MP2T payload type 33, a 90 kHz RTP clock, adjacent RTP/RTCP ports, and one generated SDP media description. Open either RTP mode through its generated SDP:

For visible MPEG-TS acceptance, VLC opens the production URL directly:

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' 'rtp://@192.168.96.122:60000'
& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@192.168.96.122:60000'
```

Do not insert FFmpeg, an observer remux, or SDP playback between the production
MPEG-TS output and VLC. Separate dynamic-payload RTP still writes SDP as the
signaling artifact selected by the caller, but it is not the MPEG-TS/RTP receiver
URL.

`--max-duration SECONDS` is optional realtime CLI monitoring policy. When it is
present, it must be positive and stops a still-running CLI after that duration.
When it is absent, startup output, progress, and lifecycle failures retain their
existing checks, but the CLI has no duration deadline and reports
`max_duration=source_driven`. This value is not planned or passed into the
media graph runtime.

Raw RTP input requires explicit video and audio endpoint metadata. For H.264 or HEVC video only, omitting `--video-rtp-fmtp` enables preflight in-band parameter-set detection. Codec, payload type, 90 kHz clock rate, URL, and all timeout/capacity limits remain mandatory. The planner derives canonical fmtp only after complete, unambiguous evidence; the runtime receives the same bound UDP transport and the original pre-read RTP/RTCP queue. Supplying video fmtp keeps strict manual mode and performs no probe I/O. AAC always requires explicit fmtp; Opus keeps its existing no-fmtp contract.

The following VideoOnly example demonstrates automatic H.264 fmtp detection.
Add `--video-rtp-fmtp` only when authoritative signaling is available and manual
mode is required:

```powershell
$inputRtp = @(
    '--input-type','rtp',
    '--video-rtp-url','rtp://127.0.0.1:5004',
    '--video-rtp-codec','h264','--video-rtp-payload-type','96',
    '--video-rtp-clock-rate','90000',
    '--open-timeout-ms','30000','--read-timeout-ms','2000',
    '--analyze-duration-us','5000000','--probe-size','5000000'
)
$common = @(
    '--egress-capacity-bps','50000000','--maximum-wire-residence-ms','100',
    '--video-codec','hevc','--rc','cbr',
    '--width','1920','--height','1080','--fps','25',
    '--bitrate','6000','--gop','50','--no-audio'
)
$cli = 'out/build/x64-release/media_transcode_realtime_video_cli.exe'

& $cli @inputRtp @common --media-id rtp-to-tsudp `
    --output-layout mpegts --output-transport udp `
    --output 'udp://127.0.0.1:5010'

& $cli @inputRtp @common --media-id rtp-to-tsrtp `
    --output-layout mpegts --output-transport rtp `
    --rtp-host 127.0.0.1 --rtp-port 5020 `
    --sdp out/build/x64-debug/rtp-to-tsrtp.sdp
```

Probe limits have exact meanings: `--open-timeout-ms` is the target deadline for controllable startup work, `--analyze-duration-us` is the maximum interval for obtaining complete unambiguous facts after the first matching RTP packet, `--probe-size` caps all bytes received during preflight, and `--read-timeout-ms` bounds each socket wait. Detection ends when the required parameter sets are complete; conflicts observed before completion fail. FFmpeg/driver capability calls are checked for deadline overrun immediately after return because those APIs are not cancellable. Timeout, wrong PT, incomplete/conflicting parameter sets, unsupported packetization, or capacity exhaustion fails preflight before the DAG starts.

Add `--max-duration SECONDS` only when the caller wants the explicit CLI stop
gate; do not use `0`, a sentinel, or a large replacement duration.

Hardware backend and realtime low-latency behavior are planner-owned and have no
caller override; the highest-scoring capability-admitted chain is selected or the
request fails before runtime. `--quiet-graph` only controls diagnostics.
`udp://host:port` remains valid for UDP-carried RTP input in RTP-port mode;
MPEG-TS input uses `--input-type mpegts-udp` and requires an explicit
`--mpegts-max-pcr-gap-ms`. URL input uses `--input-type url`; `rtsp://` URLs
require `--rtsp-transport`, while non-RTSP URLs reject it.

## Production acceptance

Project acceptance is based on the real local and realtime CLIs, real media streams,
FFmpeg/VLC observation, runtime memory metrics, and continuous A/V drift telemetry.
The current absolute-path PowerShell commands and evidence are recorded under
`docs/completed/`.

Input termination is strict: only a true FFmpeg EOF completes successfully.
HTTP, UDP, and RTP source disappearance returns one preserved source-loss
failure; coordinated cancellation of the remaining workers does not replace
that primary error. EOF drains codec state and all sinks before the realtime
CLI performs its normal stop path.
