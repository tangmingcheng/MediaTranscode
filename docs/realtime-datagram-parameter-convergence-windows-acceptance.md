# Windows Realtime Datagram 参数收口验收

## 高质量链路：PASS

- 日期：2026-08-31
- 输入：真实 120 秒 HEVC 2560x1440 30 fps，raw RTP。
- 输出：H.264 1920x1080 25 fps，VBR 5/12/13 Mbps，MPEG-TS/RTP。
- 部署事实：预留 egress 50 Mbps，最大 wire residence 100 ms。

CLI：

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id windows-review-high-2k30-hevc-to-1080p25-h264-vbr --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60240 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61240 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\review-high-passgate-20260831\output.sdp --video-codec h264 --rc vbr --width 1920 --height 1080 --fps 25 --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --gop 50 --no-audio
```

FFmpeg 源：

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60240?rtcpport=60241&pkt_size=1200"
```

VLC 接收：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\review-high-passgate-20260831\vlc.log rtp://@192.168.96.122:61240
```

结果：

- 抓包媒体跨度 119.968639 秒，RTP 113410 包，抓包丢包 0。
- RTP sequence break 0，reorder 0；MPEG-TS continuity error 0。
- 按 planner 速率 3989698 B/s 与 1356 B burst 逐前缀检查 GBRA/GCRA，最大超额 0 B。
- sender would-block、deadline miss、pressure、partial submit、ambiguous submit 均为 0；提交、发送和最终 sequence 全部为 113442。
- VLC 创建 1920x1080 D3D11 视频输出；black、corrupt、late picture、lost 和 discontinuity 日志均为 0。
- 单核 CPU 均值 26.404%，峰值 44.622%；RSS 增长 950272 B。CPU 按本轮明确范围仅记录，不作为 Datagram 发送控制修改项。
- 源结束后 CLI 以 RTP source-clock evidence expiry 失败退出，符合有限源生命周期语义。

## 最终平台能力探测回归门禁：PASS

- 日期：2026-09-01。
- 冻结代码：`55d485a9`。
- 输入：真实连续 120 秒 HEVC 2560x1440、30 fps、raw RTP。
- 输出：H.264 1920x1080、25 fps、VBR 5/12/13 Mbps、GOP 50、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms。

CLI：

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id win-final-sndbuf-rerun-hevc2k30-h2641080p25-vbr12m --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60592 --video-rtp-codec hevc --video-rtp-payload-type 98 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61592 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\win-final-sndbuf-rerun\output.sdp --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

FFmpeg 源：

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 98 "rtp://127.0.0.1:60592?rtcpport=60593&pkt_size=1200"
```

VLC：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --file-logging --logfile D:\Code\MyCode\MediaTranscode\out\acceptance\win-final-sndbuf-rerun\vlc.log --no-video-title-show rtp://@192.168.96.122:61592
```

结果：

- FFmpeg 完整发送 3600 帧；生产 DAG 输出 2998 个 access unit。sender 与 pcap 均为 113436 个 datagram，接口捕获丢包 0。
- RTP 113410 包，loss/reorder 0；MPEG-TS continuity drop、TEI、malformed 均为 0，媒体跨度 119.967640 秒。
- planner wire rate 为 3989698 B/s、单包 burst 为 1356 B；1/10/100 ms 最大 IP 字节为 4296/27120/172708 B，均低于对应 service envelope 加单包余量 5346/41253/400326 B。GCRA 最大 debt 为 1356.437 B，与单包余量差 0.437 B，处于抓包时间戳量化误差内，无追赶式 burst。
- sender would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0；最大 submit lateness 4.531 ms，最终 backlog 为 0。两个 endpoint 的 target/API/effective socket buffer 均为 309970 B，证明 Windows provider 按本次目标值精确回读。
- VLC 记录 `Received first picture`、`Stream buffering done`、1920x1080 D3D11VA 输出；picture too late、black、corrupt、lost、discontinuity、decoder error 均为 0。唯一一次 `might be displayed late (missing 14 ms)` 后立即是窗口主动关闭与 `exiting`，属于 teardown。
- CLI 平均单核 CPU 26.985%，峰值 68.750%，峰值 working set 229462016 B；CPU 按当前范围仅记录。源结束后如实以 RTP source-clock evidence expiry 终止，最终 dropped buffer、graph payload、backlog 均为 0。

## 随机高规格复审门禁：PASS

- 日期：2026-09-01。
- 输入：真实 120 秒 HEVC 2560x1440 30 fps，raw RTP。
- 输出：H.264 1920x1080 25 fps，VBR 5/12/13 Mbps，MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms。

CLI：

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id win-random-hevc2k30-h2641080p25-vbr12m-pass3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60560 --video-rtp-codec hevc --video-rtp-payload-type 98 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61560 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\random-windows-review-pass3\output.sdp --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

FFmpeg 源：

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 98 "rtp://127.0.0.1:60560?rtcpport=60561&pkt_size=1200"
```

VLC 接收：

```powershell
D:\VideoLAN\VLC\vlc.exe --verbose=2 --file-logging --logfile D:\Code\MyCode\MediaTranscode\out\acceptance\random-windows-review-pass3\vlc.log --no-video-title-show rtp://@127.0.0.1:61560
```

结果：

- FFmpeg 完整发送 3600 帧/120 秒；生产输出 2998 AU。sender 提交 113437 个 datagram，dumpcap 同步捕获 113437 个，接口丢包 0。
- RTP 113410 包、loss/reorder 0；MPEG-TS continuity、TEI 与 fragment error 均为 0。
- 发送速率上界为 3989698 B/s；1/10/100 ms 最大 IP 字节为 4068/27120/172560 B，没有追赶式突发。
- sender would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0；最大 submit lateness 6.200624 ms，最终 backlog 为 0。
- Windows 两个 endpoint 的 planned/API/effective socket buffer 均为 309970 B，aggregate effective socket bytes 为 619940 B。
- VLC 创建 1920x1080 输出；picture too late、black、corrupt、lost、discontinuity、decoder error 均为 0。
- 单核 CPU 均值 26.051%，峰值 77.465%；峰值 RSS 229400576 B。该项为 2K HEVC 到 1080p H.264 软件转码规格，本轮按已确认范围仅记录，不扩展 CPU 优化。
- 源结束后 CLI 如实以 RTP source-clock evidence expiry 失败退出；最终 droppedBuffers、graph payload current 与 backlog 均为 0。

## 低质量链路：PASS

- 日期：2026-09-01
- 输入：真实 120 秒 H.264 1280x720 30 fps，raw RTP。
- 输出：HEVC 1920x1080 25 fps，CBR 6 Mbps，MPEG-TS/RTP。
- 部署事实：预留 egress 50 Mbps，最大 wire residence 100 ms。

CLI：

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id windows-review-low-720p30-h264-to-1080p25-hevc-cbr-retest --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60250 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61250 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\review-low-retest-20260901\output.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
```

FFmpeg 源：

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60250?rtcpport=60251&pkt_size=1200"
```

VLC 接收：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --extraintf=logger --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\review-low-retest-20260901\vlc.log rtp://@192.168.96.122:61250
```

结果：

- FFmpeg 完整发送 3600 帧/120 秒；输出 2998 AU，抓包媒体跨度 119.979612 秒。
- RTP 64486 包、RTCP 27 包，抓包丢包 0；RTP sequence break 0，reorder 0，MPEG-TS continuity error 0。
- 按 planner 速率 2006648 B/s 与 1356 B burst 逐前缀检查 GBRA/GCRA，最大超额 0 B。
- sender would-block、deadline miss、pressure、partial submit、ambiguous submit 均为 0；提交、发送和最终 sequence 全部为 64513。
- VLC 启动 HEVC 解码、收到首帧、完成 buffering 并创建 1920x1080 D3D11 输出；black、corrupt、late picture、lost、discontinuity、deadlock 和 decoder error 均为 0。
- `--extraintf=logger` 在当前 VLC 中报告该额外接口已移除；`--file-logging` 仍生成完整接收日志，主 GUI/D3D11 播放链路不受影响。
- 单核 CPU 均值 21.130%，峰值 32.450%；RSS 增长 2342912 B。CPU 按本轮明确范围仅记录，不作为 Datagram 发送控制修改项。
- 第一次同规格运行因 CLI 与 FFmpeg 被放入同一 PowerShell 进程连续异步创建，首个 IDR 早于 RTP socket bind，只输出 2948 AU/117.98 秒；本次严格以两条相邻直接命令先后启动，无检测或 sleep，完整门禁通过。
- 源结束后 CLI 以 RTP source-clock evidence expiry 失败退出，符合有限源生命周期语义。

## 地址族能力探测修复后高规格回归：PASS

- 日期：2026-09-01。
- 冻结代码：`581a78b5`。
- 输入：真实连续 120 秒 HEVC 2560x1440、30 fps、raw RTP。
- 输出：H.264 1920x1080、25 fps、VBR 5/12/13 Mbps、GOP 50、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms。

CLI：

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id win-family-final4-hevc2k30-h2641080p25-vbr12m --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60608 --video-rtp-codec hevc --video-rtp-payload-type 98 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61608 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\win-family-final4\output.sdp --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

FFmpeg 源：

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 98 "rtp://127.0.0.1:60608?rtcpport=60609&pkt_size=1200"
```

VLC 接收：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --file-logging --logfile D:\Code\MyCode\MediaTranscode\out\acceptance\win-family-final4\vlc.log --no-video-title-show rtp://@192.168.96.122:61608
```

结果：

- Npcap Loopback 捕获 255007 包，接口、pcap 与 dumpcap 丢包均为 0；按输出端口隔离后 RTP 113410 包，与 sender RTP endpoint 精确一致，RTP loss/reorder 为 0。
- sender 提交 RTCP 27 包；Npcap Loopback 对本机 RTCP 的发送与接收各记录一次，抓包为 54 条，按方向折算后与 sender 一致。
- planner wire rate 为 3989698 B/s；1/10/100 ms 最大 IP 字节为 4068/27120/170600 B，均低于“持续速率窗口加一个最大包”的 5346/41253/400326 B 上界。GCRA 最大 debt 为 1356.437 B，仅比单包 1356 B 多 0.437 B，属于 100 ns 抓包时间戳量化误差，无追赶式 burst。
- MPEG-TS continuity drop、TEI、malformed 均为 0。VLC 收到首帧并识别 1920x1080；late picture、black、corrupt、lost、discontinuity、decoder error 均为 0。
- sender would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0；最大 submit lateness 4.908324 ms，最终 backlog 为 0。
- 两个 endpoint 的 planner target、Windows API request 与 provider effective socket buffer 均为 309970 B，aggregate effective socket bytes 为 619940 B，证明按目标地址族选择的 provider 能力探测产品与运行时一致。
- CLI 平均单核 CPU 27.225%，峰值 77.465%，峰值 working set 229294080 B；CPU 按当前明确范围仅记录，不通过修改发送控制算法规避。源结束后 CLI 如实以 RTP source-clock evidence expiry 失败退出；最终 dropped buffer、graph payload current 与 sender backlog 均为 0。
- 前三次相同规格运行因验收抓包接口或 BPF 引号错误而缺少 wire 证据，均未判定 PASS；本结论只采用第四次完整证据。
