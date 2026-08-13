# RKMPP Zero-Copy Completion Record

## Delivered

- Added strict `--hardware-backend rkmpp` planning for local and realtime CLIs. It conflicts with `--disable-hw` and fails before DAG startup when the complete RKMPP contract is unavailable.
- Modelled decode, optional RGA filter, and encode frame contracts as DRM PRIME hardware descriptors. Source-size chains omit the filter; resize chains use the target `scale_rkrga` filter.
- Validated DRM descriptors, format, planes, backing buffers, dimensions, lineage, and decode/filter/encode buffer identity at runtime. No software transfer fallback is inserted.
- Added H.264 and HEVC Annex-B scheduled RTP support, H.265 SDP parameter sets, POSIX RTP sockets/atomic SDP publication, Linux CPU/RSS sampling, and Linux DAG-worker thread names.
- Kept audio copy/transcode selection in the shared audio planner. Synchronized A/V requires frame transcode because startup trimming and drift correction operate on samples; its bitrate is now resolved from source/target facts rather than required as a CLI topology hint.

## Target acceptance

All target commands entered `/home/firefly/Downloads/MediaTranscode` and enabled the target FFmpeg environment before execution.

- Local source-size: H.264 to H.264, H.264 to HEVC, HEVC to H.264, and HEVC to HEVC completed with 481 frames, 2560x1440 output, full decode, matching decoder/encoder DRM identity, and zero software/download/upload counts.
- Local RGA: H.264 to H.264 and HEVC to HEVC completed at 1280x720. RGA replaced only the DRM hardware buffer; full decode and zero software/download/upload counts were observed.
- Realtime video: all four H.264/HEVC combinations produced decodable separate RTP output. HEVC routes published VPS/SPS/PPS in H.265 SDP.
- Realtime A/V: H.264 plus real source AAC produced H.264 2560x1440 and AAC 44100 Hz stereo output; full dual-stream decode returned success. Startup release was 16 video and 22 audio units, drift remained 0 ns/0 ppm, drops remained zero, and RSS stabilized near 60 MiB.
- Release video-only CLI CPU was approximately 20-24% in Linux single-core-percent convention. Full A/V was approximately 50-61%; thread sampling attributed about 38% to the software AAC encoder while RKMPP video decode/encode workers were about 2%/1%.

The target clean Release build used eight parallel jobs and completed all 491 targets. Both CLI executables linked against the FFmpeg libraries selected by `ffenv on`.

## Representative acceptance commands

The final target A/V run used the real source audio and did not pass `--audio-bitrate`:

```bash
ffmpeg -hide_banner -loglevel warning -re -i /home/firefly/Downloads/test16s.mp4 -map 0:v:0 -map 0:a:0 -c copy -f mpegts 'udp://127.0.0.1:24760?pkt_size=1316'

build-release/media_transcode_realtime_video_cli --media-id final-rkmpp-av --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:24760?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 3000 --analyze-duration-us 2000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 25760 --sdp /tmp/final-78685129.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --bitrate 2000 --gop 30 --audio-codec aac --hardware-backend rkmpp

ffmpeg -hide_banner -loglevel warning -analyzeduration 5000000 -probesize 5000000 -protocol_whitelist file,udp,rtp -i /tmp/final-78685129.sdp -map 0:v:0 -map 0:a:0 -c copy -y /tmp/final-78685129.mkv
ffprobe -v error -show_entries stream=index,codec_name,width,height,sample_rate,channels -show_entries format=duration -of default=noprint_wrappers=1 /tmp/final-78685129.mkv
ffmpeg -v error -i /tmp/final-78685129.mkv -map 0:v:0 -map 0:a:0 -f null -
```

The Windows default-planner local regression used the fixed 120-second source:

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_local_video_cli.exe --input D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4 --output D:\Code\MyCode\MediaTranscode\out\acceptance\rkmpp-windows-local-regression.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --bitrate 4000 --gop 30 --audio-codec aac
```

It selected the existing CUDA/NVENC auto chain, kept the compatible audio branch as packet copy, processed 3,600 video frames, reported zero drops/downloads/uploads, and exited normally. The Windows 120-second realtime A/V chain used FFmpeg `-re`, the realtime CLI without `--audio-bitrate`, and VLC against the published separate-RTP SDP; it held approximately 20.4 microseconds phase error with zero drops and stable RSS.

## Remaining risks

1. The installed FFmpeg RKMPP encoder closes its MPP context before clearing its four retained async imported frames. Source-loss shutdown therefore still prints four `invalid mem pool ptr` messages from the target FFmpeg fork even after application-side EOS drain. The application does not hide the messages or weaken RAII; resolving them requires an upstream FFmpeg RKMPP close-order fix, which is outside this application-only scope.
2. Finite MPEG-TS/UDP source loss remains a strict `IoFailure` (`native=-5`) rather than clean EOF, so the realtime CLI exits non-zero after the sender stops. This preserves the existing source-loss contract.
3. Full A/V CPU includes software AAC sample-domain trimming and drift correction. Packet copy remains available when the shared planner proves codec/output compatibility and the output contract does not require frame-domain correction.
4. Long-duration RKMPP/RGA soak, repeated source replacement, and thermal throttling remain unmeasured.
