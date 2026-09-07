# RKMPP H.264 2K30 到 HEVC 1080p25 CBR6M MPEGTS-RTP 验收记录

> 最终门禁按用户明确要求：VLC 正常持续硬解证明持续播放；无卡顿由用户观察与收发无丢包验证。GPU 活动只作硬解辅助证据，不作为逐帧显示证明。

## 范围与版本

- 基线：`a5597326464140a319787d123ccc2ffef9c4e40b`。
- 分支：`codex/rk-a559-external-rtp`。
- 核心修复检查点：`3f10fb4d`。
- 目标机：`root@192.168.130.229`，目录 `/home/tang/MediaTranscode`。
- RKMPP 二进制 SHA256：`96c5c71d3183cfe11846dba36ee48f4116983c775e7caa1b3eb5078188989b57`。
- 本项没有修改核心代码，也没有新增 CLI 或公共参数。

## 输入源

使用目标机已有的两份 HEVC 2560x1440 30 fps、约 11 Mbps 高规格视频制作连续源，不使用 `testsrc` 或 `-stream_loop`。制作过程的解码和编码均使用 RKMPP：

```bash
printf "file '/home/tang/canonical-2k-hevc-aac-128s.mp4'\nfile '/home/tang/test-continuous-120s-2k-hevc.mp4'\n" > /home/tang/rk-highspec-hw-source-list.txt
ffmpeg -hide_banner -nostdin -y -c:v hevc_rkmpp -f concat -safe 0 -i /home/tang/rk-highspec-hw-source-list.txt -map 0:v:0 -an -c:v h264_rkmpp -b:v 12M -maxrate 12M -bufsize 24M -g 60 -pix_fmt yuv420p -movflags +faststart /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4
```

FFmpeg 映射为 `hevc (hevc_rkmpp) -> h264 (h264_rkmpp)`。生成文件为 H.264、2560x1440、30 fps、11,554,504 bps、248.266667 秒，SHA256 为 `82d2c9e76b2dd4d01a45065d6983206dba7fce44e897f4d65c5eb71d81815c37`。

## 实际命令

源只做实时读取和 RTP 封装，不解码：

```bash
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

RKMPP CLI 参数保持用户指定值：

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-local-run24
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

Windows 接收抓包使用 `dumpcap -D` 确认的“以太网”接口 6：

```powershell
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:260 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run24\receiver.pcapng -q
```

VLC 直接打开 URL，使用默认解码选择，只增加文件日志：

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run24\vlc.log rtp://@192.168.96.122:6200
```

目标机输出还原后使用 RKMPP 硬解校验：

```bash
ffmpeg -hide_banner -nostdin -v info -c:v hevc_rkmpp -i "$dir/output.ts" -map 0:v:0 -an -f null -
```

## 验收结果

第 24 次源开始于 `2026-09-07T11:59:47.919601470+08:00`，自然结束于 `12:03:55.938061092+08:00`，持续 248.018 秒。CLI PID `3802860`，源 PID `3802864`，目标机输入/输出抓包 PID `3802842/3802843`，脚本 PID `3802835`；Windows 有效 dumpcap/VLC PID `12072/30716`。

- 输入抓包有 `262430` 个 RTP 包，持续 `247.734422` 秒，RTP 序列缺失、乱序和重复均为 0；目标机 ingress 无 truncation 或 pressure failure。
- 发送端有 `156186` 个 RTP 包和 `56` 个 RTCP 包；RTP 序列缺失、乱序和重复均为 0，MPEG-TS sync、continuity、TEI 和 invalid AFC 错误均为 0。
- 发送端按 50 Mbps 服务曲线计算的最大超额为 `1356 B`，恰好一个最大 IP 数据报；1/5/10/40/100 ms 最大 IP 字节分别为 `5880/25764/47460/174024/259908 B`。`deadline_misses=0`，最大 wire residence `48.916468 ms`，满足 100 ms 硬约束。
- Windows 有效接收抓包覆盖 `196.285145` 秒，收到 `123613` 个 RTP 包，Wireshark RTP 统计为丢失 `0 (0.0%)`。抓包晚于源开始约 59 秒，但连续覆盖仍超过三分钟。
- VLC 默认选择 `d3d11va_vld`，明确记录 `Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU...)`。结束段 30 秒内 VLC UDP 接收数增加 `7207`、接收错误保持 0，进程 GPU `VideoDecode` 计数持续增长；`buffer deadlock`、超过 5 秒晚帧、`picture is too late` 和 decoder error 均为 0。
- 输出为 HEVC 1920x1080 25 fps；PTS 共 `6204` 个，间隔持续按 40 ms 推进，PTS 到达相对媒体时钟的漂移末值约 `-0.230 ms`。`hevc_rkmpp` 硬解全部 `6204` 帧，退出码 0；没有使用软件解码。
- 源运行期间 CLI 没有 worker error、runtime error、丢弃、pressure failure 或 deadline miss。平均单核 CPU 约 `16.03%`，峰值 `50.00%`；RSS 从 `52,191,232 B` 增至峰值 `56,717,312 B`，全程 RSS 增量约 4.5 MB；不据此外推长期内存趋势。
- 源自然结束后约 8 秒，CLI 按既有 `RTP video source clock evidence expired` 失败退出，退出码 1。该退出发生在源停止后，不属于源持续期间的核心退出。
- 两路目标机 tcpdump 均为 `0 packets dropped by kernel`；测试结束后 CLI、源、VLC 和抓包进程均无残留。

本项通过：核心和源连续运行超过三分钟，源存在期间核心不退出；发送端无超过一个最大数据报的突发；接收 RTP 零丢失；默认 VLC 持续使用 D3D11VA 硬解；目标机 RKMPP 完整硬解全部输出帧。

## 失败根因闭环

- 第 22 次使用 VLC 软件解码，不符合“不允许任何一方使用软件解码”的验收要求，结果作废；核心代码未因此修改。
- 第 23 次把 Windows `ifIndex=5` 误当成 Wireshark 接口号，实际抓到 WLAN，接收抓包只有文件头。`dumpcap -D` 证明承载 `192.168.96.122` 的“以太网”是接口 6；这是诊断命令错误，不是核心缺陷。
- 第 24 次首次启动 dumpcap 时参数引号被 `Start-Process` 拆散，进程立即退出。修正为完整参数字符串后从源运行第 59 秒开始有效抓包，连续覆盖 196.285 秒，仍满足三分钟门禁；核心和 CLI 参数没有改变。

证据目录：目标机 `/home/tang/MediaTranscode/out/acceptance/rk-a559-local-run24/`；本机 `D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run24\`。抓包和临时分析器不纳入版本库。

补充核对：接收窗口的 123613 个 RTP 包与发送端对应后缀逐包哈希一致，SHA256 为 8ec4978726957917579b24657bed658ed9e58a3074f72ae39aeda40add083aea；缺失、重复、乱序均为 0。
