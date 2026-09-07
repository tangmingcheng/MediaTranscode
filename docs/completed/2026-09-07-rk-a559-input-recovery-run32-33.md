# RKMPP 输入丢包恢复 run32 / run33：完整门禁未通过

两个测试使用同一未提交恢复版本，CLI SHA-256：`3702de01c928a3994aa6caaa43645166cf64612b0ede908871a624a445cf3c23`。相对 `bdc15de9` 增加输入活跃时间、关键帧等待和 VideoOnly 时钟证据等待；不含随后针对 run33 的帧率补帧修改。两轮均不是成功验收提交。

## 真实命令

目标机 `source /opt/mt-tools/mtenv.sh; mtenv on; source /etc/profile.d/ffenv.sh; ffenv on`。`dir` 分别为 `/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run32` 和 `.../rk-a559-loss20-run33`。

```bash
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" 'rtp://192.168.130.229:61884?pkt_size=1400'
```

已有源为 H.264 2560×1440、30 fps、约 11.55 Mbps、248 秒有限文件。源端只复制已有压缩码流；核心使用 RKMPP，VLC 使用默认 D3D11VA。

播放器命令分别为：

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run32\vlc.log rtp://@192.168.96.122:6200
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run33\vlc.log rtp://@192.168.96.122:6200
```

输入注入：先 `tc qdisc change dev lo root netem loss 0%`，观察编码输出后等待 20 秒，执行 `tc qdisc change dev lo root netem loss 20%`，30 秒后执行 `tc qdisc change dev lo root netem loss 0%`。源到目标机本地地址经过 lo，出口经过 eth0 的原有 fq，无出口丢包注入。脚本启动与观察产生的实际时刻见下表，不把计划秒数替代实际证据。

## 逐次结果与根因

| 项目 | run32 | run33 |
|---|---|---|
| CLI / 源 PID | 3875424 / 3875649 | 3884765 / 3884768 |
| 输入 / 输出抓包 PID | 3875404 / 3875405 | 3884747 / 3884748 |
| 测试 / 注入脚本 PID | 3875389 / 3875971 | 3884740 / 3884836 |
| VLC / Windows 抓包 PID | 10808 / 29616 | 7192 / 26312 |
| 注入开始 / 撤销 | 13:48:46.447 / 13:49:16.461 | 13:51:59.972 / 13:52:29.986 |
| 实测注入期输入丢包 | 1,903 缺失，20.1546% | 6,339 缺失，20.0196% |
| 源结束 | 13:48:57.009，exit 155 | 13:55:40.484，exit 0 |
| 核心结果 | 输入停止后 ProgressTimeout，exit 1 | 源自然结束后 ProgressTimeout，exit 1 |

时间均为目标机 UTC+8。

**run32 根因：源发送失败，未完成恢复测试。** FFmpeg 在媒体时间 35.40 秒报 `Network is unreachable` 并退出；入口抓包最后 RTP 为 13:48:55.267793。核心此前于 13:48:59.292 记录 `waiting_for_decodable_input input_active=true`，最终于 13:49:07.319 开始终止，符合最后输入约 12 秒超时。核心 workerErrors=0、truncations=0、pressure_failures=0。网络不可达的更底层触发原因没有查实：事后本地路由正常，NetworkManager 对应时段无记录；不能把这一轮说成核心仍然在有输入时退出。没有因此修改核心。

**run33 根因：恢复时批量追补历史画面，反向阻塞接收。** 本轮源完整运行。13:52:12.582 进入等待，撤销丢包后 13:52:31.459 收到关键帧，13:52:31.626 恢复输出，同一核心会话存活。撤销后入口抓包 200,623 RTP、189.700735 秒、缺失 0；但核心在 13:52:37.903 报 `first_missing=1920 resumed=5332`，对应 3,412 包，运行中 `/proc/net/udp` 的 61884 socket drops 恰为 3,412。既有 VideoFrameRateNode::sendFrame 从旧 nextOutputIndex 循环补齐至当前 PTS，长缺口形成历史帧积压；最终 encodedPacketsPushed=18,612，对应 6,204 帧完整时间轴，并非损伤期间始终获得真实新画面。该补齐行为原来已存在，新增等待恢复路径使长时间缺口重新进入该路径，暴露了原实现未处理的恢复背压问题。

run33 输出 RTP 147,327，RTP 序号和 TS 连续性错误 0；但完整发送服务曲线超额 1,680.75 B，高于既有 1,356 B 单包基线，最大输出间隔 31.007763 秒。VLC 有恢复阶段晚帧丢弃日志。因此“会话保持并恢复输出”得到证据支持，完整无突发、无额外丢包与稳定播放门禁仍为 **FAIL**。CPU 平均单核 14.143354%，峰值 RSS 64,770,048 B；VideoOnly 不适用 A/V 漂移指标。

## 后续局部修复依据

参照 [GStreamer videorate 属性](https://gstreamer.freedesktop.org/documentation/videorate/index.html)及其 [gst_video_rate_do_max_duplicate 源码](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gst-plugins-base/gst/videorate/gstvideorate.c)：超过规划的最大可补帧间隔时，保留时间缺口并从新画面继续。当前拟由 VideoOnly planner 将权威源帧周期形成内部有理数契约，既有节点消费；不使用经验秒数、不新增对外参数。修改后的真实结果另行记录，不能用本报告证明修复通过。

证据位于上述目标机与本机同名目录，包含完整 CLI/源/VLC 日志、脚本内容、PID、输入分阶段序号分析、socket drops、发送时序和抓包统计；原始目标机 pcap 与本机 receiver.pcapng 保留。run32 的序号统计按展开的唯一序号集合计算，避免把乱序当成 65,535 包缺失。两个测试及路由监控 PID 3884460 均已退出；临时执行脚本已删除。Windows 抓包 run32 提前结束，不主张其完整统计门禁通过；run33 捕获 147,383 包，抓包工具丢包 0。
