# RKMPP VideoOnly Batched Ingress Completion

## Delivered

- Capability selection and all receive/storage/reorder facts are planner-owned.
- The complete ingress product is serialized into the raw-RTP node and checked against the retained prepared socket binding before replay.
- Linux uses one bounded reusable `recvmmsg` adapter behind the shared ingress receiver; the existing parser, reorder buffer, depacketizer, clock tracker and DAG edges remain unchanged.
- Batch telemetry reports receive operations, datagrams, bytes and maximum batch geometry.

## Real CLI evidence

The RK Release build was produced under `ffenv on` with eight parallel jobs. The acceptance source remained 2560x1440 HEVC at 30 fps for 128 seconds. The realtime CLI selected `hevc_rkmpp -> scale_rkrga(1280x720) -> h264_rkmpp`, used DRM PRIME hardware frames, and emitted MPEG-TS over RTP to the visible Windows VLC receiver.

The run processed 122806 ingress datagrams in 15865 receive batches. Machine-normalized process CPU averaged 3.20%, single-core-normalized CPU averaged 25.61%, and RSS remained between 49.99 MB and 53.77 MB. Queues drained completely, dropped buffers were zero, 60798 output datagrams were accepted, and enqueue deadline misses were zero.

Commands used:

```text
/home/firefly/Downloads/MediaTranscode/out/build/rkmpp-release/media_transcode_realtime_video_cli --media-id rk-video-only-cpu --input-type rtp --input-layout separate --video-rtp-url rtp://127.0.0.1:52000 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --no-audio --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.197 --rtp-port 52500 --packet-size 1328 --sdp /tmp/rk-video-only-cpu.sdp --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --hardware-backend rkmpp --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100
/usr/local/bin/ffmpeg -hide_banner -loglevel warning -re -i /home/firefly/Downloads/canonical-2k-hevc-aac-128s.mp4 -map 0:v:0 -c:v copy -an -payload_type 96 -ssrc 52001 -f rtp rtp://127.0.0.1:52000?rtcpport=52001
& 'D:\VideoLAN\VLC\vlc.exe' 'rtp://@:52500'
```

## Remaining gates

- The bare RTP source provides no authoritative EOF/BYE contract. After the source stopped, the CLI correctly failed when sender-report evidence expired; this is not reported as successful completion.
- The installed RKMPP stack still emits teardown diagnostics and needs upstream close-order investigation.
- The Windows completion adapter, AudioVideo integration, cross-platform same-spec acceptance and long-duration soak remain incomplete.
