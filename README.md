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

```powershell
ffplay -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-output.sdp
ffprobe -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-output.sdp
```

For visible local acceptance, open separate RTP through its generated SDP because
the H.264 and AAC dynamic payload mappings require signaling. Open MPEG-TS/RTP
directly as `rtp://@host:port` and MPEG-TS/UDP directly as `udp://@host:port`.
Do not insert an observer remux between the production output and VLC.

`--max-duration SECONDS` is optional realtime CLI monitoring policy. When it is
present, it must be positive and stops a still-running CLI after that duration.
When it is absent, startup output, progress, and lifecycle failures retain their
existing checks, but the CLI has no duration deadline and reports
`max_duration=source_driven`. This value is not planned or passed into the
media graph runtime.

Raw RTP input requires explicit video and audio endpoint metadata. For H.264 or HEVC video only, omitting `--video-rtp-fmtp` enables preflight in-band parameter-set detection. Codec, payload type, 90 kHz clock rate, URL, and all timeout/capacity limits remain mandatory. The planner derives canonical fmtp only after complete, unambiguous evidence; the runtime receives the same bound UDP transport and the original pre-read RTP/RTCP queue. Supplying video fmtp keeps strict manual mode and performs no probe I/O. AAC always requires explicit fmtp; Opus keeps its existing no-fmtp contract.

The following reusable input/options demonstrate automatic H.264 video fmtp with explicit AAC signaling. Add `--video-rtp-fmtp` back when authoritative signaling is available and manual mode is required:

```powershell
$inputRtp = @(
    '--input-type','rtp','--input-layout','separate',
    '--video-rtp-url','rtp://127.0.0.1:5004',
    '--video-rtp-codec','h264','--video-rtp-payload-type','96',
    '--video-rtp-clock-rate','90000',
    '--audio-rtp-url','rtp://127.0.0.1:5006',
    '--audio-rtp-codec','aac','--audio-rtp-payload-type','97',
    '--audio-rtp-clock-rate','44100','--audio-rtp-channels','2',
    '--audio-rtp-fmtp','profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210',
    '--open-timeout-ms','5000','--read-timeout-ms','2000',
    '--analyze-duration-us','5000000','--probe-size','5000000'
)
$common = @(
    '--metadata-queue','1','--packet-queue','256','--frame-queue','128','--mux-queue','256',
    '--startup-max-video-unit-bytes','4194304',
    '--startup-max-audio-unit-bytes','1048576','--startup-max-gap-ms','40',
    '--video-codec','h264','--width','1280','--height','720','--fps','30',
    '--bitrate','4000','--gop','30','--audio-codec','aac','--audio-bitrate','128',
    '--sample-rate','48000','--channels','2'
)
$cli = 'out/build/x64-debug/media_transcode_realtime_video_cli.exe'

& $cli @inputRtp @common --media-id rtp-to-tsudp `
    --output-layout mpegts --output-transport udp `
    --output 'udp://127.0.0.1:5010'

& $cli @inputRtp @common --media-id rtp-to-tsrtp `
    --output-layout mpegts --output-transport rtp `
    --rtp-host 127.0.0.1 --rtp-port 5020 --packet-size 1328 `
    --sdp out/build/x64-debug/rtp-to-tsrtp.sdp
```

Probe limits have exact meanings: `--open-timeout-ms` is the total startup deadline, `--analyze-duration-us` begins at the first matching RTP packet, `--probe-size` caps all bytes received during preflight, and `--read-timeout-ms` bounds each socket wait. Timeout, wrong PT, incomplete/conflicting parameter sets, unsupported packetization, or capacity exhaustion fails preflight before the DAG starts.

Add `--max-duration SECONDS` only when the caller wants the explicit CLI stop
gate; do not use `0`, a sentinel, or a large replacement duration.

Hardware planning, low-latency input, and graph diagnostics are enabled by default. Use `--disable-hw`, `--no-low-latency`, or `--quiet-graph` only for an explicit override. `udp://host:port` remains valid for UDP-carried RTP input in RTP-port mode; MPEG-TS input uses `--input-type mpegts-udp --input-layout mpegts` and requires an explicit `--mpegts-max-pcr-gap-ms`. A value of `1000` is the verified starting point for FFmpeg `-re` loopback sources; stricter values intentionally make PCR-gap generation reacquisition more sensitive.

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
