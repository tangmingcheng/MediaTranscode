# RKMPP 外部 RTP 复验记录

基线 `a5597326464140a319787d123ccc2ffef9c4e40b`；工作分支 `codex/rk-a559-external-rtp`。直接使用当前目录，未建立独立工作树。目标机 `/home/tang/MediaTranscode`。

用户一小时时限：2026-09-04 15:18:18 至 16:18:18（北京时间）。参数不变，验收要求连续超过三分钟、源不停转码不停、线端无突发。

实际输入 H.264 1920x1080 25 fps，输出 HEVC 1920x1080 25 fps、CBR 6000 kbps、GOP 50、无音频、MPEG-TS/RTP。用户指定 media-id 中的 720p30 不作为源参数事实。入口 `192.168.130.229:61884`，出口 `192.168.96.122:6200`，受管容量 50000000 bps，maximum-wire-residence 100 ms。

## 失败与修复对照

| 次数 | 版本与结果 | 已确认原因 | 未确认部分及对应修改 |
|---|---|---|---|
| 1 | 基线，约 4.1 秒失败 | 平均排队时间软目标要求 6252528 B/s，超过 6250000 B/s 后直接退出 | 将软目标限于部署容量，保留逐包截止约束；后续实际触发过容量限制计数，未再由此分支退出 |
| 2 | 软目标修复，约 6.6 秒失败 | AU 95 到输出调度时已晚约 1.35 秒，继而违反发送 deadline | 上游解码迟交与补帧尚未区分，不宣称修复，不据此认定新增回归 |
| 3 | 同一修复，仅增加诊断，约 2.3 秒失败 | 视频提前就绪；低速发送状态下剩余提交窗口不足 | 50 Mbps 受管速率修正待验；逐帧诊断有扰动，不能替代正常版验收 |
| 4 | 同一诊断版，约 4.2 秒失败 | 当前服务速率 2010248 B/s；notBefore 比 notAfter 晚 113634 ns | 同上；输出捕获 2635 包，50 Mbps 服务曲线超额最多一包，短时无 RTP/TS 错误不代表通过 |
| 5 | 同一诊断版失败 | 实际提交不满足预约窗口 | 具体越界项证据不足，尚不能判定新增问题 |
| 6 | 受管速率修复，约 2.34 秒失败 | AU 到输出调度时晚约 1.20 秒 | 与第 2 次同类，上游原因未解决 |
| 7 | 编解码诊断，约 4.02 秒失败 | RKMPP receive EAGAIN 后等待新输入约 2.48 秒，随后输出旧帧 | planner 从源帧率给出接收轮询周期；未改变输入规格 |
| 8 | 解码接收修复，约 42 秒失败 | 提交不满足预约窗口；视频全部提前至少约 71 ms 就绪 | 同第 5 次；补充提交开始、结束、窗口诊断，未声称已解决 |
| 9 | 提交诊断，约 9.97 秒失败 | 编码 AU 晚约 2.27 秒 | 第 2、6、7 次问题仍有未覆盖部分 |
| 10 | 执行既有低延迟编码契约，约 9.34 秒失败 | 旧 AU 晚约 1.20 秒；相关输入先于输出约 2.5 秒到达 | LOW_DELAY 标志遗漏已修正，但不足以单独解决持续运行 |
| 11 | 核心 API 时间诊断，约 25.84 秒失败 | 解码轮询已排空，编码同次输出；后续输入间隔约 2.51 秒，历史发送 deadline 在数据报生成前已过期 | 区分媒体迟到与 wire queue residence；deadline 在首次物化时形成，重试不得延长 |
| 12 | 驻留起算修复，约 14.39 秒失败 | 恢复后的集中物化造成真实线端积压，驻留最高约 113.6 ms | 没有放宽 100 ms；planner 要求当前批次提交后再准入下一物化批次，复用全局 reservation 背压 |
| 13 | 背压改动后启动段错误，退出 139 | CMake 关闭头文件依赖；结构体变更后的增量构建仅重编 3 个 cpp，混入旧布局目标文件 | 本次新增构建问题；全量重建同一源码后复验，不改 CMake 扩大范围 |
| 14 | 全量重建，约 15 秒失败 | 启动崩溃消失；单个 reservation 可含约 473 KB，大批次内的发送耗时仍耗尽 100 ms | Windows 已使用高精度 timer；Linux timer slack 实测 50000 ns，满包额外间隔中位数约 96040 ns。Linux 等待精度修复另行验证 |

## 当前最小修改

修改集中于现有 planner、编解码契约与发送链路：软排队目标受部署容量约束；显式容量形成发送速率；RKMPP 按源帧率轮询异步输出；执行已有低延迟编码契约；wire deadline 从首次可发送的物化时刻起算；公共全局 reservation 在当前批次提交后唤醒下一批物化。未新增线程、队列或对外参数，未改变媒体时间戳、分辨率、帧率、码率和 RC。

wire residence 与端到端媒体延迟不同：`deadline = max(canonicalRelease, materializedAt) + maximumResidence`，数据报准入后不可更改。迟到仍在输出调度 telemetry 中报告。该修改不承诺消除上游停顿或播放器卡顿。

算法依据：[RFC 1363 最大速率漏桶](https://www.rfc-editor.org/rfc/rfc1363.html)；[WebRTC 平均队列软目标](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/pacing/pacing_controller.cc)。无突发需以接收抓包核对 50 Mbps 服务曲线加至多一个最大数据报的离散误差，不等同于匀速 6 Mbps。

目标后端依据：[ffmpeg-rockchip rkmppdec.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1/libavcodec/rkmppdec.c) 的非阻塞输出；[rkmppenc.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1/libavcodec/rkmppenc.c) 的 LOW_DELAY 分支。背压使用已有 FIFO reservation、WouldBlock、commit 唤醒和 RAII；不引入预测算法。当前版本必须在真实测试完成后重新交叉审查，早期四文件 PASS 不覆盖后续修改。

## 实际执行命令

RKMPP 每次使用下列原参数，仅 `dir` 随测试编号变化：

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-external-run14
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

Windows 使用同一命令参数，程序路径为 `D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe`，绑定 `rtp://192.168.96.122:61884`，SDP 为 `D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-external\windows-output.sdp`。源由用户指定的 startSendRtp/stopSendRtp API 控制，Windows 仅将目标地址改为本机，未另用 FFmpeg 生成或监控源。

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-external\windows-vlc.log --no-video-title-show rtp://@192.168.96.122:6200
```

Windows CLI PID `35516`，VLC PID `31180`。RKMPP 各次 CLI/capture/driver PID 保存在对应 `pids.txt`。VideoOnly 的 A/V 漂移不适用；记录 CPU/RSS、错误、排队与视频调度时间。

## Windows 真实链路通过记录

H.264 1920x1080 25 fps RTP → HEVC 1920x1080 25 fps CBR 6 Mbps MPEG-TS/RTP，完成全部重建后实跑。源开始 `16:05:52.333`，主动停止 `16:09:37.496`；接收抓包覆盖输出 `226.053533 s`、RTP `124286` 包、RTCP `50` 包，RTP 序号及 TS continuity/TEI 错误均为 0。输出最大包间隔 `36.661 ms`，50 Mbps 服务曲线最大超额 `1356 B`，不超过一个最大数据报。

运行期间 workerErrors/errors/droppedBuffers 均为 0；平均单核 CPU `18.50%`、峰值 `53.53%`，RSS 稳定约 `204.5 MB`、峰值 `217.1 MB`。VLC 播放窗口未记录 late/drop/discontinuity；启动有一次 `buffer deadlock prevented`，随后正常切换解码格式，另有窗口缩略图 API 报错，保留原始日志不隐去。源停止后以 `realtime runtime made no progress before timeout` 退出，未把无源判为成功 EOF。

Windows 二进制 SHA256：`08369a0ed5b3d4cc8dca68871226dcc2b92e3e0addc4e10759ce1fe12765fb11`。该通过记录不代表 RKMPP 已通过，也不覆盖后续 Linux 平台等待精度修改。

证据目录：本地 `out/acceptance/rk-a559-external/`，目标机 `out/acceptance/rk-a559-external-run*/`。临时诊断不纳入版本库，交付前恢复并全量重建、删除临时脚本及诊断备份，检查测试进程残留。
