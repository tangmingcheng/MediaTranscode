# RKMPP H.264→HEVC 1080p25 CBR 6 Mbps run34 / run35：FAIL

版本：加入帧率缺口跳过的恢复版本，CLI SHA-256 `3a84a1980716aef25420302f9f1037571bc34b26d35b625a2b666fc1757cc66d`。目标机目录 `/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run34`；对应本机目录保存 VLC 和接收抓包。

## 实际命令

启用目标机既有 mtenv、ffenv 后，令 `dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run34`，运行：

```bash
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" 'rtp://192.168.130.229:61884?pkt_size=1400'
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run34\vlc.log rtp://@192.168.96.122:6200
```

源仍为已有 H.264 2560×1440@30 高规格有限视频；没有软件解码替代。CLI / 源 PID 为 3894561 / 3894582；输入/输出抓包 3894532 / 3894533；测试/注入脚本 3894501 / 3894502；VLC / Windows 抓包 15776 / 20996。

## 确认的根因

本文件各轮沿用以下输入丢包与恢复命令，作用于本地源实际经过的 lo，eth0 不注入丢包。计划为正常编码出流 20 秒后注入、30 秒后恢复；部分轮次核心提前失败，是否实际执行及执行时间以各轮结果为准，不把计划当成执行证明。

```bash
tc qdisc change dev lo root netem loss 20%
tc -s qdisc show dev lo
# 注入后 30 秒恢复，失败清理同样执行恢复命令
tc qdisc change dev lo root netem loss 0%
tc -s qdisc show dev lo
```

- 14:08:12.871 启动源；14:08:17.939 核心检测 `first_missing=8473 resumed=8474`，进入关键帧等待。
- 14:08:18.509 收到完整关键帧；14:08:18.540 帧率节点记录 `current_pts=1564692295 previous_pts=1564638295`，保留 54,000/90,000=0.6 秒缺口，没有补齐历史画面。
- 14:08:19.151，FileMuxNode 失败：`InvalidArgument: MPEG-TS mux session advance exceeded the maximum PCR gap`。workerErrors=1；61884 socket 各次采样 drops=0。源在此时仍运行，故是核心失败。
- 调用链为 ProjectMpegTsMuxSessionAdapter::write → MediaTsMuxSession::writeAccessUnit → advanceThroughAvailable。既有 adapter::poll 在 `nextTransportDeadline > latestAcceptedEmission` 时停止维护；没有媒体 AU 时 PCR/PSI 定时器因此不推进，下个 AU 的 0.6 秒时间跃迁触发 PCR 间隔守卫。守卫正确，停止定时维护与新增缺口保留行为的组合不完整。
- 计划的 20% lo 丢包直到 14:08:33.433 才开始，晚于核心失败约 14 秒；不能把退出归因于该注入。源随后被发送 SIGINT，于 14:09:20.045 结束。此轮没有通过持续运行或恢复验收。

后续仅移除阻止既有 PCR/PSI 定时维护的 AU watermark 判断，保留现有时钟、deadline、InputOrDeadline 唤醒和最大 PCR 间隔校验。行业参照：[GStreamer 1.18 的实时聚合与输入停产处理](https://gstreamer.freedesktop.org/releases/1.18/)、[1.20 的 PCR 间隔保障](https://gstreamer.freedesktop.org/releases/1.20/)。本项目复用既有独立维护 API，不声称复制了 GStreamer 的 CBR 填充机制，也不新增空白视频或重启会话。修复结果由下一轮实测决定。

全部上述进程已退出，临时执行脚本已删除，lo 恢复 0% 注入。完整日志、原脚本内容、PID 和每 2 秒 socket 采样均保留；本轮失败后提前停止 Windows 抓包，不主张完整接收门禁通过。VideoOnly 的 A/V 漂移不适用。

## run35：维护持续，但恢复首包仍失败

版本增加上述 PCR/PSI 空闲维护修复，CLI SHA-256 `4ac6db36d0777443c81a03583103cc75abf8826bd9c9e328e7479ec9df702099`。执行同一 CLI 和 FFmpeg 命令，`dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run35`；播放器实际命令为：

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run35\vlc.log rtp://@192.168.96.122:6200
```

CLI / 源 PID 3901416 / 3901440，输入/输出抓包 3901397 / 3901398，测试/注入脚本 3901370 / 3901371，VLC / Windows 抓包 11028 / 18808。

- 14:19:25.196 注入 20%，14:19:55.205 撤销，实测 6,352 缺包、20.0221%；前后阶段输入 RTP 缺失均为 0，socket drops 所有采样均为 0。
- 14:19:37.733 进入等待恢复；PCR 持续输出 51.459168 秒，PTS 只推进 20.72 秒。这证明维护已能独立于可编码画面继续。输出 RTP 13,959，序号和 TS 连续性错误 0；发送服务曲线超额 1,356 B。仍不能把 PCR 输出当作真实画面恢复。
- 14:19:56.340 完整关键帧到达；14:19:56.371 跳过 31.166667 秒缺口；14:19:56.377 FileMuxNode 报 `MPEG-TS mux session emission time regressed`，workerErrors=1，CLI exit 1。测试脚本随后停止源，源于 14:19:58.376 结束、exit 255。
- 根因是 writeAccessUnit 仍直接用恢复 AU 的 emitOnMaster 调用维护推进，而独立 PCR 维护已经推进到更晚位置；维护时间和媒体时间共用了同一推进入参。修改只令维护推进取已推进位置与 AU 所需位置的较大值，AU 的 PTS、DTS、release 及独立媒体单调性校验不变，也不回退已经发送的 PCR。MediaTsOutputClockGenerator 本来就分别保存 PCR 和各流 DTS，复用该分工。

run35 完整恢复门禁仍为 **FAIL**。平均单核 CPU 8.524947%，峰值 RSS 57,774,080 B；输入 tcpdump 丢包 0。对应日志、分阶段抓包分析和 socket 采样已保存于目标机及本机同名目录。全部上述进程已退出，临时执行脚本已删除。后续修正等待 run36，不能以本轮证明通过。

## run36：恢复时包的发送窗口已过期

CLI SHA-256 `1635fdf8846c74ea228fbdc94665f1bf5f36419f0a61da0d0df687e8378cbcec`。沿用以上 CLI / FFmpeg 命令，目录改为 `rk-a559-loss20-run36`；VLC 命令相同，仅日志目录改为 run36。CLI / 源 PID 3916310 / 3916333，输入/输出抓包 3916293 / 3916294，执行/注入脚本 3916262 / 3916263，VLC / Windows 抓包 32012 / 27760。

- 14:29:08.869 检测 `first_missing=2768 resumed=2769`；14:29:10.328 收到完整关键帧；14:29:10.360 帧率节点保留 141,000/90,000=1.566667 秒缺口。
- 14:29:10.366 FileMuxNode 报 `MPEG-TS access unit has no transport service window`，CLI exit 1、workerErrors=1。发送调度所见包 DTS 为 42756，比首包 42747 仅增加 9/25=0.36 秒，已经落后于维护时钟。窗口拒绝正确，过期媒体仍向下游提交是未解决的问题。
- 这些数值尚不足以证明时间戳被改写：目标版 `scale_rkrga` 默认 `async_depth=2`，其异步队列在新输入到达后取出旧帧；RKMPP 编码器按提交帧 PTS 生成包 PTS/DTS。需用逐帧诊断区分“恢复新帧时间戳错误”和“先释放积压旧帧”，未确认前不修改时间戳或放宽窗口。
- 计划注入没有执行，失败发生在等待注入的 20 秒内。源由脚本在 CLI 失败后停止，于 14:29:11.362 结束；不能把本轮归因于 20% 注入。全部上述进程已退出，两个临时执行脚本已删除。

结论 **FAIL**，下一轮临时启用现有 Flow 日志，仅用于逐帧根因诊断。行业及平台依据：[RGA 异步 FIFO](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/d90e3a1/libavfilter/rkrga_common.c)、[RKMPP 编码时间戳传递](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/d90e3a1/libavcodec/rkmppenc.c)。

## run37：撤销输入丢包后重复窗口失败

CLI SHA-256 `91a9b2a05ae7547d650651d7ecc8f9e4755e9a42508901b85e4da3906de7498e`，同一 CLI / FFmpeg / VLC 命令，仅目录改为 `rk-a559-loss20-run37`。CLI / 源 PID 3925547 / 3925568，输入/输出抓包 3925528 / 3925529，执行/注入脚本 3925497 / 3925498，VLC / Windows 抓包 6404 / 31788。

- 14:45:54.331 开始注入，14:46:24.341 撤销。期间缺包 6,314，实测 19.9029%；注入前及恢复后缺包均为 0，socket drops 所有采样为 0。
- 14:46:07.064 输入活跃但没有可编码画面，继续等待；14:46:25.447 完整关键帧到达，14:46:25.480 保留 31.133333 秒缺口；14:46:25.486 再次报 `MPEG-TS access unit has no transport service window`，随后接收取消属于会话失败后的清理，不是新的独立根因。
- 首输入 PTS 2737166440，恢复前最后输入 2739044440（源相对时间 20.866667 秒），恢复输入 2741846440（52 秒）；失败包 DTS 760844，比首包 760324 只前进 520/25=20.8 秒，符合异步队列仍释放旧画面的行为。目标文件 `has_b_frames=0`，目标 RGA 广告 `async_depth` 范围 0–4、默认 2。
- 本轮试图通过修改诊断默认值开启 Flow，但 execution context 的 `setDiagnosticsEnabled` 又写回 State，因此逐帧日志未取得。不得把本轮称为逐帧诊断通过；后续临时诊断需覆盖这一赋值，交付前全部恢复。
- CLI exit 1；源被脚本在失败后停止，14:46:27.481 结束、exit 255。全部上述进程已退出，临时执行脚本已删除，lo 注入恢复为 0%。

结论 **FAIL**。下一步仅在已有低延迟 planner 中，根据滤镜广告的能力选择零帧等待，并由现有硬件滤镜 graph probe 验证可用性；DAG、节点线程、硬件缩放及外部参数均不变。零帧等待表示本帧 fence 完成即可输出，不再依赖后续输入填充队列；不放宽发送窗口或改写媒体时间。结果待 run38。

## run38：恢复编码成功，PCR API 跨度判断错误

CLI SHA-256 `3f79f22deaebbbc8ac84c57da0b06dc8c29d97abbe81505e4b2a569f0909936c`，同一 CLI / FFmpeg / VLC 命令，目录 `rk-a559-loss20-run38`。CLI / 源 PID 3940019 / 3940043，输入/输出抓包 3940002 / 3940003，执行/注入脚本 3939974 / 3939975，VLC / Windows 抓包 27544 / 31972。

- planner 与运行时滤镜日志均为 `scale_rkrga=w=1920:h=1080:format=nv12:async_depth=0`。本轮 Flow 逐帧诊断实际生效，非最终性能验收版本。
- 注入期间缺包 6,233，实测 19.6544%；前后输入缺包 0。14:56:31.386 恢复输入 PTS 4257402107；滤镜换算到 1/25 为 1182612，编码提交与编码包 PTS/DTS 均为 1182612，后续连续递增。原先旧帧窗口失败未出现，14:56:31.566 控制器记录 `output_resumed`。
- 14:56:31.963 FileMux 报 `MPEG-TS mux session advance exceeded the maximum PCR gap`。此时尚未把恢复 AU 发上线路：输出 PTS 仍仅 521 个、跨度 20.8 秒。编码恢复不等于播放恢复，完整门禁仍为 **FAIL**。
- 根因位于 `advanceThroughAvailable`：它在调用 `materializeMaintenanceThrough` 之前，把两次调用目标的跨度直接与 maximumPcrGap 比较。末 PCR 所在 master 时间约为 51.943 秒，恢复 AU emit 为 52.103 秒，调用跨越 160 ms；中间应生成两个相隔 80 ms 的 PCR，却在循环执行前被拒绝。这是调用跨度与相邻 PCR 间隔混淆，不能将该错误解释为 PCR 时间戳已经跳了 160 ms。
- 已有 `materializeMaintenanceThrough` 按各个 deadline 生成 PCR；`MediaTsOutputClockGenerator::preparePcr` 强制下一样本必须等于上一样本加 pcrInterval，且序列化还有独立校验。后续删除错位的调用跨度判断，保留这些逐样本校验及独立发送期限，不改变 pcrInterval、maximumPcrGap 或 CLI 参数。参照 FFmpeg MPEG-TS 编码器按 PCR 周期逐次插入维护包的实现。
- 抓包 PCR 650 个，所有 PCR 时间戳步长最大 80 ms；实际到达最大间隔 230.936 ms，不能声称实际 PCR 抖动达标。发送服务曲线超额 1,356 B，RTP 序号和 TS 连续性错误 0，仍不构成完整通过。源在 CLI 失败后被停止，14:56:33.387 结束。全部进程已退出，临时执行脚本已删除。

run39 恢复默认 State 日志与采样设置，使用完整高规格源重新验收。
