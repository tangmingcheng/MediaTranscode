# RK Release 50 Mbps Datagram 发送控制门禁

日期：2026-08-31

链路：H.264 1280×720 30 fps raw RTP 输入，经 RKMPP 转码为 HEVC 1920×1080 25 fps、CBR 6 Mbps，输出 MPEG-TS/RTP。受管 egress capacity 为 50,000,000 bit/s，maximum sender service residence 为 100 ms。

## 实际命令

```text
/home/tang/task5-a14d4692/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-release-720p30-h264-to-1080p25-hevc-cbr6m-mpegts-rtp-queue-05 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --input-layout separate --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:58072 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 60072 --sdp /home/tang/task5-a14d4692-gate-rk-release-720p30-h264-to-1080p25-hevc-cbr6m-mpegts-rtp-queue-05/output.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio

/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:58072?rtcpport=58073&pkt_size=1200"

D:\VideoLAN\VLC\vlc.exe --no-one-instance --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-release-720p30-h264-to-1080p25-hevc-cbr6m-mpegts-rtp-queue-05\vlc.log --verbose=2 rtp://@:60072

/usr/sbin/tcpdump -i eth0 -U -n -s 0 -w /home/tang/task5-a14d4692-gate-rk-release-720p30-h264-to-1080p25-hevc-cbr6m-mpegts-rtp-queue-05/egress.pcap "(udp dst port 60072 or udp dst port 60073) and dst host 192.168.96.122"
```

## 结果

- 120 秒连续源完整发送 3600 帧，FFmpeg RC=0；CLI 在源结束后以真实 RTP source-clock expiry 退出，未超时且无进程残留。
- RK egress RTP 75,169 包，loss/order 为 0；Windows 接收 RTP 75,172 包，loss/order 为 0；两端 TS continuity error 均为 0。
- RK egress 1/5/10/20/100 ms 滑窗最大线速分别为 32.880/28.496/28.496/27.400/23.577 Mbps，均低于 50 Mbps 服务上限。sender 最大自适应 pacing rate 为 36.548 Mbps，deadline、pressure、would-block、partial 和 ambiguous failure 均为 0。
- VLC 使用 URL 接收，完成 HEVC 1920×1080 解码；corrupt、discontinuity、loss、late picture 和 decoder error 均为 0。
- RK CLI 单核 CPU 平均 12.318%，外部 1 秒采样峰值 18%；RSS 64.7–67.0 MiB，无持续增长。
- `backlog_max_residence=115.110 ms` 是 materialization 到 commit 的全阶段观测，包含 planner 的 32.517 ms protocol preparation lead；canonical release 到 immutable deadline 的部署合同仍为 100 ms，sender deadline miss 为 0。

判定：PASS。该提交仅标记已经通过的 `a14d4692` 生产字节，不包含随后进行的参数收口修改。
