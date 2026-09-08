# RKMPP HEVC 2K30 到 H.264 1080p25 CBR6M MPEGTS-RTP 验收记录

> 证据复核：本记录证明核心持续运行、网络连续性及硬件解码；此前用短时 GPU 活动和日志推断整段画面无卡顿，证据不足。用户随后明确以 VLC 正常持续硬解、收发无丢包和已完成的画面观察为最终门禁，本项按此通过。完整 VLC 日志还包含 `picture might be displayed late`，不应把仅搜索严重晚帧所得的零计数表述为所有晚帧均为零。

## 版本与输入

- 基线 `a5597326464140a319787d123ccc2ffef9c4e40b`，核心修复检查点 `3f10fb4d`，验收分支 `codex/rk-a559-external-rtp`。
- 目标机 `/home/tang/MediaTranscode`，二进制 SHA256 为 `96c5c71d3183cfe11846dba36ee48f4116983c775e7caa1b3eb5078188989b57`。
- 输入文件 `/home/tang/rk-highspec-hw-hevc2k30-248s.mp4` 为 HEVC 2560x1440 30 fps、11,577,100 bps、248.266667 秒，SHA256 为 `a04142981e30d21c8d2d6d81054ed3de038d50d43b9db756c41a79769d8a11d7`。
- 输入文件由目标机已有两份 2K30 HEVC 高规格视频经 `hevc_rkmpp -> hevc_rkmpp` 制作；没有软件解码、合成源或循环播放。

## 实际命令

源发送只做实时读取和 RTP 封装：

```bash
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

RKMPP CLI：

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-local-run26
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-high-hevc2k30-h2641080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec h264 --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

Windows 接收和播放：

```powershell
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run26\receiver.pcapng -q
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run26\vlc.log rtp://@192.168.96.122:6200
```

目标机输出硬解：

```bash
ffmpeg -hide_banner -nostdin -v info -c:v h264_rkmpp -i "$dir/output.ts" -map 0:v:0 -an -f null -
```

## 验收结果

run26 从 `2026-09-07T12:16:04.405800232+08:00` 开始，源在 `12:20:12.692928887+08:00` 自然结束，持续 248.287 秒。CLI PID `3809859`，源 PID `3809862`，目标机输入/输出抓包 PID `3809840/3809841`，脚本 PID `3809600`；Windows dumpcap/VLC PID `23028/9932`。

- 输入有 `263076` 个 RTP 包，持续 `247.735141` 秒，序列缺失、乱序和重复为 0；ingress 无 truncation 或 pressure failure。
- 发送端有 `156176` 个 RTP 包和 `57` 个 RTCP 包，RTP 序列及 MPEG-TS sync、continuity、TEI、invalid AFC 错误均为 0。
- 50 Mbps 服务曲线最大超额 `1356 B`；1/5/10/40/100 ms 最大 IP 字节为 `5880/25764/47728/172212/267856 B`。`deadline_misses=0`，最大 wire residence `56.722286 ms`，满足 100 ms 约束。
- Windows 收到完整的 `156176` 个 RTP 包，持续 `248.192021` 秒，Wireshark RTP 丢失为 `0 (0.0%)`。
- VLC 默认使用 NVIDIA D3D11VA；106 秒节点连续 5 秒 `VideoDecode` 利用率为 6.58% 至 7.00%。日志中 buffer deadlock、超过 5 秒晚帧、picture late 和 decoder error 均为 0。
- 输出 PTS 共 `6204` 个，媒体时长 `248.12` 秒，40 ms 步长连续，PTS 到达相对媒体时钟的漂移末值约 `-0.199 ms`。`h264_rkmpp` 硬解全部 6204 帧并以 0 退出。
- 源运行期间 CLI 没有 worker error、runtime error、丢弃、pressure failure 或 deadline miss；平均单核 CPU `15.33%`，峰值 `47.06%`，RSS 峰值 `53,161,984 B`。
- 源自然结束约 8 秒后，CLI 按既有 source-clock expiry 以 1 退出。目标机两路 tcpdump 均为 0 kernel drop；所有测试进程无残留。

本项通过：连续运行超过三分钟，源运行期间核心不退出；发送无突发，收端 RTP 零丢失；VLC 默认硬解持续工作；目标机 RKMPP 全帧硬解输出。

## 失败根因

run25 在核心启动前失败：临时脚本启用 `set -u` 后加载 `/etc/profile.d/ffenv.sh`，该环境脚本读取未定义的 `PKG_CONFIG_PATH` 并退出。run26 仅删除临时脚本的 `set -u`，没有修改核心、CLI 参数或媒体链路。

证据目录：目标机 `/home/tang/MediaTranscode/out/acceptance/rk-a559-local-run26/`；本机 `D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run26\`。抓包和临时分析器不纳入版本库。

补充核对：收发 156176 个 RTP 包逐包哈希一致，SHA256 为 c1f525e5b93be34e4910a0f82a167c5e3a1aa6b8aa9188c38f446c0652639c52；缺失、重复、乱序均为 0。
