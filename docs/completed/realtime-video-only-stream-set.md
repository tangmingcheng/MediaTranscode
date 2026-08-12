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

Local VideoOnly used the production local CLI, once with the A/V source and once with the concrete video-only projection `canonical-video-only.mp4`:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_local_video_cli.exe' --input 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' --output 'D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\L01\output.mp4' --no-audio --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_local_video_cli.exe' --input 'D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\canonical-video-only.mp4' --output 'D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\L02\output.mp4' --no-audio --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30
& 'D:\VideoLAN\VLC\vlc.exe' --intf dummy --no-one-instance --aout=dummy --video-filter=scene --scene-format=png --scene-ratio=30 --scene-path='D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\L02' 'D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\L02\output.mp4'
```

Every realtime route used these queue, video-startup and supervisor bounds:

```text
--metadata-queue 1 --packet-queue 256 --frame-queue 256 --mux-queue 256
--startup-max-video-unit-bytes 4194304
--progress-timeout-ms 15000 --first-output-timeout-ms 20000 --poll-interval-ms 500
```

AudioVideo routes additionally used `--startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40`. Only generic AudioVideo session input accepts the four prepared-handoff controls; that route used `--prepared-handoff-video-packets 256 --prepared-handoff-audio-packets 512 --prepared-handoff-video-bytes 1073741824 --prepared-handoff-audio-bytes 536870912`. VideoOnly, raw RTP and MPEG-TS/UDP input reject those route-inapplicable controls.

Raw RTP used the fixed order CLI -> FFmpeg -> VLC. Manual H.264/HEVC supplied exact SPS/PPS or VPS/SPS/PPS; automatic routes omitted `--video-rtp-fmtp`, used `--probe-size 5000000`, and preserved the prepared same-socket transport. The three output substitutions were:

| Output | CLI arguments | VLC input |
|---|---|---|
| Separate RTP | `--output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port <V>` | generated SDP |
| MPEG-TS/UDP | `--output-layout mpegts --output-transport udp --output udp://127.0.0.1:<P>` | `udp://@127.0.0.1:<P>` |
| MPEG-TS/RTP | `--output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port <P> --sdp <file>` | `rtp://@127.0.0.1:<P>` |

For example, counted RHC01 used concrete input RTP/RTCP 43800/43801, unused companion audio 43802/43803, output 43820/43821, and this exact source command:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -loglevel info -re -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' -map 0:v:0 -c:v hevc_nvenc -preset p4 -tune hq -rc vbr -b:v 8406k -maxrate 8406k -bufsize 16812k -g 30 -bf 0 -an -payload_type 96 -ssrc 43801 -f rtp 'rtp://127.0.0.1:43800?rtcpport=43801' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 43803 -f rtp 'rtp://127.0.0.1:43802?rtcpport=43803'
```

Its production CLI and VLC observer commands were:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id rhc01-canonical-vo-hevc-manual-separate-audio-present --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:43800' --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'sprop-vps=QAEMAf//IWAAAAMAkAAAAwAAAwB4lwJA;sprop-sps=QgEBIWAAAAMAkAAAAwAAAwB4oAKAgC4fE5ZdKQhGRf0MBAQAAAMABAAAAwB5gBd5RAABAIeAAAQCH4Q=;sprop-pps=RAHArfAzJA==' --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 43820 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\RHC01-canonical-vo-hevc-manual-separate-audio-present\output.sdp' --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 256 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 8406 --gop 30 --rc auto --progress-timeout-ms 15000 --first-output-timeout-ms 20000 --poll-interval-ms 500 --no-audio
& 'D:\VideoLAN\VLC\vlc.exe' --intf dummy --no-one-instance --aout=dummy --no-video-title-show --network-caching=1000 --verbose 2 --file-logging --logfile='D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\RHC01-canonical-vo-hevc-manual-separate-audio-present\vlc-debug.log' --video-filter=scene --scene-format=png --scene-ratio=30 --scene-path='D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\RHC01-canonical-vo-hevc-manual-separate-audio-present' --scene-prefix=vlc-frame --no-audio 'D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\RHC01-canonical-vo-hevc-manual-separate-audio-present\output.sdp'
```

HEVC routes replaced stream copy with the fixed realtime encoder facts `hevc_nvenc -preset p4 -tune hq -rc vbr -b:v 8406k -maxrate 8406k -bufsize 16812k -g 30 -bf 0`. AudioVideo routes also mapped AAC to their explicit RTP/RTCP pair; VideoOnly source-semantics-one sent companion audio only to unused ports, while source-semantics-two used `-an`.

True RTSP used the user-approved order MediaMTX -> FFmpeg publisher -> CLI subscriber -> VLC. MediaMTX v1.19.3 was checksum-verified and configured as a TCP-only RTSP listener on `127.0.0.1:45100`; it is an acceptance tool under untracked `out/`, not a production dependency. The final AudioVideo routes used paths `rw07handofffinal`, `rw08handofffinal`, and `rw09handofffinal`; outputs were separate RTP 45800-45803, TS/UDP 45810, and TS/RTP 45820-45821 respectively.

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\acceptance\tools\mediamtx\v1.19.3\mediamtx.exe' 'D:\Code\MyCode\MediaTranscode\out\acceptance\tools\mediamtx\v1.19.3\mediamtx-tcp-only.yml'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -nostdin -loglevel info -re -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' -map 0:v:0 -map 0:a:0 -c copy -rtsp_transport tcp -f rtsp 'rtsp://127.0.0.1:45100/rw09handofffinal'
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id rw09-handoff-final --input-type rtsp --input-layout session --input 'rtsp://127.0.0.1:45100/rw09handofffinal' --rtsp-transport tcp --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 45820 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\rtsp-wire-final\RW09-handoff-final\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 256 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --prepared-handoff-video-packets 256 --prepared-handoff-audio-packets 512 --prepared-handoff-video-bytes 1073741824 --prepared-handoff-audio-bytes 536870912 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 8406 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 15000 --first-output-timeout-ms 20000 --poll-interval-ms 500
& 'D:\VideoLAN\VLC\vlc.exe' --intf dummy --no-one-instance --aout=dummy --network-caching=1000 --video-filter=scene --scene-format=png --scene-ratio=30 --scene-path='D:\Code\MyCode\MediaTranscode\out\acceptance\video-only-stream-set\rtsp-wire-final\RW09-handoff-final' 'rtp://@127.0.0.1:45820'
```

MediaMTX logs prove publisher `ANNOUNCE/SETUP/RECORD` and CLI reader `DESCRIBE/SETUP/PLAY` for all nine RTSP routes. The final server PID `5936` was persisted and stopped exactly. The final RW07-RW09 source/CLI/VLC PID numbers were used by the hidden orchestrator for residue checks but were not persisted into the route artifacts; this report therefore does not invent them. Earlier counted route JSON files retain their concrete launched PIDs, and every final route retains the exact commands, ports, logs and zero-residue result.

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
