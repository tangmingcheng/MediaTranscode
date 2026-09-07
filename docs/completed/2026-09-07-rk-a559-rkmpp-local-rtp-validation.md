# RKMPP 本地高规格源验收完成记录

## 范围与版本

- 基线：`a5597326464140a319787d123ccc2ffef9c4e40b`。
- 分支：`codex/rk-a559-external-rtp`。
- 核心修复检查点：`3f10fb4d`。
- 目标机：`root@192.168.130.229`，目录 `/home/tang/MediaTranscode`。
- RKMPP 二进制 SHA256：`96c5c71d3183cfe11846dba36ee48f4116983c775e7caa1b3eb5078188989b57`。
- 本轮没有修改核心代码，也没有新增 CLI 或公共参数。

## 本地输入源

目标机没有单个超过三分钟的 H.264 文件。验收源只使用目标机已有的两份高规格视频制作，不使用 `testsrc` 或 `-stream_loop`：

```text
/home/tang/canonical-2k-hevc-aac-128s.mp4
  HEVC 2560x1440 30 fps，约 11.03 Mbps，128.267 s
/home/tang/test-continuous-120s-2k-hevc.mp4
  HEVC 2560x1440 30 fps，约 11.02 Mbps，120.000 s
```

制作命令：

```bash
printf "file '/home/tang/canonical-2k-hevc-aac-128s.mp4'\nfile '/home/tang/test-continuous-120s-2k-hevc.mp4'\n" > /home/tang/rk-highspec-source-list.txt
ffmpeg -hide_banner -nostdin -y -f concat -safe 0 -i /home/tang/rk-highspec-source-list.txt -map 0:v:0 -an -c:v h264_rkmpp -b:v 12M -maxrate 12M -bufsize 24M -g 60 -pix_fmt yuv420p -movflags +faststart /home/tang/rk-highspec-hevc2k30-to-h2642k30-248s.mp4
```

生成文件为 H.264、2560x1440、30 fps、约 11.57 Mbps、248.267 s，SHA256：
`0945cc9c1b5d65e842db6b9073b82d045d68c8198e3965ae109e35ec1ba101f9`。

## 实际执行命令

本地文件通过目标机 FFmpeg 按真实时钟发送到固定入口，未循环：

```bash
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

RKMPP CLI 参数保持用户指定值：

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-local-run22
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

Windows 接收抓包使用实际承载 `192.168.96.122` 的以太网接口 5：

```powershell
D:\Wireshark\dumpcap.exe -i 5 -f "udp and (port 6200 or port 6201)" -a duration:290 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run22\receiver.pcapng -q
```

VLC 直接打开 RTP URL。该 VLC 的 D3D11VA 路径在第 21 次触发 `not enough decoding slices in the texture (6/28)`、`buffer deadlock prevented` 和持续晚帧，本次禁用该播放器硬件解码器：

```powershell
D:\VideoLAN\VLC\vlc.exe --avcodec-hw=none --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run22\vlc.log rtp://@192.168.96.122:6200
```

## 验收结果

第 22 次源开始于 `2026-09-07 11:12:27.201 +08:00`，自然结束于 `11:16:35.304`，连续约 248.10 秒。CLI PID `3767593`，源 PID `3767596`，目标机输入/输出抓包 PID `3767574/3767575`，脚本 PID `3767565`；Windows dumpcap/VLC PID `17352/2884`。

- 输入 RTP 共 `262679` 包，序号缺失、乱序、重复均为 0；最大包间隔 `34.789 ms`，共 `7448` 个视频 marker，最大 marker 间隔 `57.212 ms`。
- 目标机发送 RTP `156268` 包、RTCP `56` 包；RTP 序号和 MPEG-TS sync、continuity、TEI 错误均为 0。
- 发送端 50 Mbps 服务曲线超额 `1356 B`，恰为一个最大数据报；`deadline_misses=0`，最大 wire residence `60.303236 ms`，满足 100 ms 硬约束。
- Windows 收到相同的 `156268` 个 RTP 包；发送端与接收端 RTP payload 序列 SHA256 同为 `bdd714f3878da1aa66c9952010272211f0c17eaac91f11855fed92de6492bc4c`，证明传输无丢包或重排。
- Windows 主机抓包的到达服务曲线超额为 `17387.5 B`，高于发送端；由于发送端已满足一个最大包边界且收发 payload 完全一致，该数值作为接收网卡/主机到达聚合风险保留，不把它改写成发送突发。
- 接收端还原流为 HEVC 1920x1080、25 fps、约 6.22 Mbps、248.16 秒。PTS 每 `40 ms` 连续推进，首个 PTS 比 PCR 提前 `180 ms`；全流 `6204/6204` 帧解码成功，FFmpeg 退出码 0。
- VLC 软件解码持续至源结束：`buffer deadlock=0`、超过 5 秒晚帧 `0`、picture late `0`、decoder error `0`。
- 源流期间 CLI 的 worker error、runtime error、丢弃、pressure failure 和 deadline miss 均为 0；平均单核 CPU 约 `16.28%`，峰值约 `57.14%`，RSS 峰值 `57073664 B`。
- 源自然结束后约 8 秒，CLI 按现有 RTP source-clock evidence expiry 失败退出，退出码 1；该退出发生在源停止后，不属于“源不停、转码停止”。
- 目标机两路 tcpdump 均为 `0 packets dropped by kernel`。

本轮满足：连续运行超过三分钟；源流存在期间转码不停止；目标机发出端无超过一个最大数据报的突发；接收端收到全部数据且全帧可解码；VLC URL 播放日志无卡顿、晚帧或解码错误。

## 失败根因闭环

- 外部源第 18 次：目标机入口在核心收包前已有约 2.48 秒无包窗口及 RTP 分片缺失，责任边界是外部发送端或中间传输，不修改核心。
- 本地源第 20 次：自行合成的 `testsrc2` 源不符合用户要求，立即作废并删除，未作为验收证据。
- 本地源第 21 次：核心输出和接收数据完整，但 VLC D3D11VA 报纹理解码 slice 数不足，随后 buffer deadlock 和持续晚帧；相同接收数据可完整解码 `6204` 帧。第 22 次只禁用 VLC 的故障硬件解码路径，核心与 CLI 参数不变，故障不再出现。

证据目录：目标机 `/home/tang/MediaTranscode/out/acceptance/rk-a559-local-run22/`；本机 `D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-local-run22\`。抓包和临时分析器不纳入版本库。
