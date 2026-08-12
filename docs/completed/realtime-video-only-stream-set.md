# Realtime VideoOnly Stream Set

## Outcome

The explicit `MediaTranscodeStreamSet` contract is complete across local file, MPEG-TS/UDP, raw RTP, and real RTSP inputs. `VideoOnly` omits audio nodes, ports, queues, RTP media descriptions, PES and PMT entries. `AudioVideo` retains the canonical startup, generation, scheduler and drift authorities. Planner products own stream selection, signaling, queue and byte bounds, prepared-input handoff, protocol identity, timing and output shape; downstream components fail closed when a required product is missing or inconsistent.

The formal real-media matrix passed **56/56** with the canonical 120-second source:

| Input family | VideoOnly | AudioVideo | Result |
|---|---:|---:|---:|
| Local file | 2 | 0 | 2/2 |
| MPEG-TS/UDP | 6 | 3 | 9/9 |
| Raw RTP | 24 | 12 | 36/36 |
| Real RTSP/TCP wire | 6 | 3 | 9/9 |

The earlier nine HTTP-listen MPEG-TS session runs remain supplemental and are not counted as RTSP.

## Command contract

All processes were created hidden (`CreateNoWindow` or `-WindowStyle Hidden`). The source was always:

```text
D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4
```

Local VideoOnly used the production local CLI, once with the A/V source and once with a video-only projection:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_local_video_cli.exe' --input 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' --output '<absolute-output.mp4>' --no-audio
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -i '<absolute-output.mp4>' -map 0:v:0 -c copy -f null NUL
& 'D:\VideoLAN\VLC\vlc.exe' --intf dummy --no-one-instance --aout=dummy --video-filter=scene --scene-format=png --scene-ratio=30 --scene-path='<absolute-evidence-dir>' '<absolute-output.mp4>'
```

The realtime CLI shared these explicit bounds; routes that do not consume a field reject or ignore it through their typed request contract rather than a runtime fallback:

```text
--metadata-queue 1 --packet-queue 256 --frame-queue 256 --mux-queue 256
--startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40
--prepared-handoff-video-packets 256 --prepared-handoff-audio-packets 512
--prepared-handoff-video-bytes 1073741824 --prepared-handoff-audio-bytes 536870912
--progress-timeout-ms 15000 --first-output-timeout-ms 20000 --poll-interval-ms 500
```

Raw RTP used the fixed order CLI -> FFmpeg -> VLC. Manual H.264/HEVC supplied exact SPS/PPS or VPS/SPS/PPS; automatic routes omitted `--video-rtp-fmtp`, used `--probe-size 5000000`, and preserved the prepared same-socket transport. The three output substitutions were:

| Output | CLI arguments | VLC input |
|---|---|---|
| Separate RTP | `--output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port <V>` | generated SDP |
| MPEG-TS/UDP | `--output-layout mpegts --output-transport udp --output udp://127.0.0.1:<P>` | `udp://@127.0.0.1:<P>` |
| MPEG-TS/RTP | `--output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port <P> --sdp <file>` | `rtp://@127.0.0.1:<P>` |

The raw sender command family was:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -loglevel info -re -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' -map 0:v:0 -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:<video-port>?rtcpport=<video-rtcp-port>'
```

HEVC routes replaced stream copy with the fixed realtime encoder facts `hevc_nvenc -preset p4 -tune hq -rc vbr -b:v 8406k -maxrate 8406k -bufsize 16812k -g 30 -bf 0`. AudioVideo routes also mapped AAC to their explicit RTP/RTCP pair; VideoOnly source-semantics-one sent companion audio only to unused ports, while source-semantics-two used `-an`.

True RTSP used the user-approved order MediaMTX -> FFmpeg publisher -> CLI subscriber -> VLC. MediaMTX v1.19.3 was checksum-verified and configured as a TCP-only RTSP listener on `127.0.0.1:45100`; it is an acceptance tool under untracked `out/`, not a production dependency.

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\acceptance\tools\mediamtx\v1.19.3\mediamtx.exe' '<absolute-tcp-only-config.yml>'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -loglevel info -re -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' -map 0:v:0 -map 0:a:0 -c copy -rtsp_transport tcp -f rtsp 'rtsp://127.0.0.1:45100/<path>'
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id '<id>' --input-type rtsp --input-layout session --input 'rtsp://127.0.0.1:45100/<path>' --rtsp-transport tcp --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 5000000 --probe-size 5000000 <shared-bounds> <output-arguments> <video-arguments> [--no-audio]
& 'D:\VideoLAN\VLC\vlc.exe' --intf dummy --no-one-instance --aout=dummy --network-caching=1000 --video-filter=scene --scene-format=png --scene-ratio=30 --scene-path='<absolute-evidence-dir>' '<planned-output-or-sdp>'
```

MediaMTX logs prove publisher `ANNOUNCE/SETUP/RECORD` and CLI reader `DESCRIBE/SETUP/PLAY` for all nine RTSP routes.

## Verification evidence

- Every formal realtime source consumed all 3600 frames / 120 seconds without `-stream_loop` or a max-duration shortcut.
- VideoOnly SDP contains one video media description and no audio. VideoOnly Project MPEG-TS uses program 1 with PCR/H.264 PID 257 and no audio ES/PES. MPEG-TS/RTP uses PT 33 and `MP2T/90000`.
- AudioVideo Project MPEG-TS uses PCR/H.264 PID 257 and AAC PID 258. Continuous drift telemetry remained in the microsecond range with no recovery, discontinuity, duplicate or production drop in counted runs.
- Final true-RTSP A/V results were: separate RTP `33000/33000` encoded and 115 VLC frames; TS/UDP `42142/42142` and 112 frames; TS/RTP `42142/42142` and 116 frames. All ended with queued/workers/errors/drops equal to zero.
- Typical CLI working set was approximately 185-221 MiB after startup. CUDA initialization caused bounded transients on some TS/UDP routes; no sustained growth was observed during the 120-second runs.
- Exact launched PIDs and owned ports were checked after every counted chain. Final MediaMTX, FFmpeg, CLI, VLC, RTSP port 45100 and output-port residue was zero.
- VLC reported route-dependent UDP TS continuity, RTP loss and teardown/Direct3D warnings. These are retained as receiver-side risk evidence; production DAG telemetry and continuous decoded frames remained clean.

## Remaining risks

- Validation covers 120-second finite streams, not multi-hour soak, reconnect, hostile traffic or repeated RTSP publisher replacement.
- Prepared RTSP handoff packet and byte budgets are caller-owned facts. Undersized values fail closed; operators must size them for input rate and planning latency.
- Several realtime planner, builder and shape-validation translation units remain large and should be decomposed by responsibility without duplicating policy.
- Receiver-side UDP/RTP continuity warnings need packet-level analysis even though production queues report zero drops.
- PCMA, static PT 8 and G.711 remain intentionally outside this branch.
