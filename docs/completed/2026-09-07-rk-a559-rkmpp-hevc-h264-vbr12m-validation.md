# RKMPP HEVC 2K30 到 H.264 1080p25 VBR 5/12/13 Mbps MPEGTS-RTP 验收

基线 `a5597326464140a319787d123ccc2ffef9c4e40b`，分支 `codex/rk-a559-external-rtp`，核心检查点 `3f10fb4d`。目标机二进制 SHA256：`96c5c71d3183cfe11846dba36ee48f4116983c775e7caa1b3eb5078188989b57`。本项未修改核心。

## 实际命令

输入为已有高规格 HEVC 素材经 RKMPP 硬解、硬编制作的 HEVC 2560x1440@30、约 11.6 Mbps、248.267 秒连续文件；源发送只做 copy，无软件解码、合成图案或循环。

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-local-run29
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"

/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-high-hevc2k30-h2641080p25-vbr12m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio

ffmpeg -hide_banner -nostdin -v info -c:v h264_rkmpp -i "$dir/output.ts" -map 0:v:0 -an -f null -
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run29\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:280 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run29\receiver.pcapng -q
```

执行顺序为先抓包和 VLC，再 CLI、源；停止由源自然结束驱动。

## 数据与判定

- 源时间：2026-09-07 12:36:26.636632021 至 12:40:34.988927211，北京时间，持续 248.352 秒。
- 精确 PID：CLI 3827432、源 3827436、输入/输出 tcpdump 3827414/3827415、运行脚本 3827407、Windows dumpcap 29688、VLC 26888。
- 发送 RTP 303041 包、RTCP 59 包，RTP 持续 248.199275 秒；发送序列与 TS sync/continuity/TEI/AFC 错误均为 0。
- 接收 RTP 303041 包，持续 248.199241 秒；缺失、重复、乱序均为 0。收发逐包 SHA256 一致：`2c05d63d01dcf213cdc5d8973b649961fec86519ee8a7d44b7bcd8770c3acf74`。
- 50 Mbps 服务曲线最大超额 1356 B，恰为一个最大 IP 数据报；deadline miss 为 0，最大 wire residence 59.546685 ms，低于 100 ms。
- 输出 6204 个 AU/PTS，PTS 持续按 40 ms 推进；目标机 `h264_rkmpp` 完整硬解 6204 帧，退出码 0。
- VLC 默认选中 NVIDIA RTX 4060 D3D11VA；运行中段 30 秒 GPU VideoDecode 采样均非零，平均 6.96%；源结束后 VLC 正常退出。日志无 buffer deadlock、超过 5 秒晚帧、picture-is-too-late 或 avcodec 解码错误。
- 保留可能迟显示的 debug 日志；PresentMon 的 ETW 采集有事件丢失，其显示统计无效，不作为通过依据。用户已观察无卡顿，并明确要求以正常持续解码及收发无丢包验证持续播放；不把短时 GPU 活动等同于逐帧显示证明。
- 源运行期间无 CLI 提前退出、worker error、丢弃、pressure failure 或 deadline miss；平均单核 CPU 20.365279%，RSS 峰值 56,967,168 B。VideoOnly 无 A/V 漂移项。
- 源结束后约 8 秒按 source-clock expiry 退出，CLI 退出码 1；源退出码 0。两路 tcpdump kernel drop 为 0。

**按用户最终明确的门禁判定 PASS。** 不扩展为逐帧零显示延迟、多小时稳定性或 Windows 转码通过。

证据位于目标机和本机各自的 `out/acceptance/rk-a559-local-run29/`。临时运行脚本内容归档为文本，实际脚本与分析器交付前删除；日志、抓包和诊断工具不入库。

