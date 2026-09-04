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
| 14 | 全量重建，约 15 秒失败 | 启动崩溃消失；单个 reservation 可含约 473 KB，大批次内的发送耗时仍耗尽 100 ms | 已有 planner 批次上限仅在整批物化后分区，未限制同一驻留窗口内准入量；下一次按既有上限逐批物化 |

| 15 | Linux 等待精度诊断版本，约 6 秒失败 | 同一提交 deadline 超限；单次 reservation 高水位仍为 346 包、468424 B | 等待精度改动没有解决大批次准入问题，已撤销；只保留按 planner 批次上限物化的待验修复 |

一小时期限已于 16:18:18 到期，未在期限内取得 RKMPP 通过版本。按用户后续要求继续只做 RKMPP 修复与验收，不再运行 Windows 测试。

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

Windows 二进制 SHA256：`08369a0ed5b3d4cc8dca68871226dcc2b92e3e0addc4e10759ce1fe12765fb11`。该通过记录不代表 RKMPP 已通过，也不覆盖后续批次物化修改。

证据目录：本地 `out/acceptance/rk-a559-external/`，目标机 `out/acceptance/rk-a559-external-run*/`。临时诊断不纳入版本库，交付前恢复并全量重建、删除临时脚本及诊断备份，检查测试进程残留。

## 第 16 次 RKMPP 实测：持续运行达标，完整验收未通过

原参数，二进制 SHA256 `96c5c71d3183cfe11846dba36ee48f4116983c775e7caa1b3eb5078188989b57`；23 个生产文件 SHA256 与本地全部一致。CLI PID `1230402`、目标机抓包 PID `1230389`、driver PID `1230386`、VLC PID `30456`。脚本内容归档在本地证据目录 `run-script-content.txt`，本次实际执行 `bash /home/tang/rk-a559-run.sh run16`。

- 输入抓包持续 `224.604606 s`；输出持续 `223.113921 s`。停止源流 API 于 `16:30:44.680` 调用，CLI 随后于 `16:30:55.908` 因原配置的无进度超时退出，退出码 1；源运行期间未退出，workerErrors/errors/droppedBuffers 均为 0。
- 输出 RTP `139060` 包、RTCP `50` 包；收发端包内容与顺序 SHA256 全部相同，无 RTP 序号或 TS continuity/TEI 错误。聚合内容哈希 `c5d6bc219dc4dfc48bbc197d6aeb162e67f024db90daae922eb02779df0c5dce`。收发捕获丢弃均为 0。
- planner 批次上限在物化前生效，backlog 高水位从 349 包降到 `49` 包、`66444 B`；最大驻留 `48.770734 ms`，deadline_misses `0`。原大批次驻留超限在本次未再发生。
- 平均单核 CPU `13.55%`、峰值 `87.5%`；RSS 从约 `65.98 MB` 到 `72.23 MB`，后段稳定。VideoOnly，A/V 漂移不适用。
- 目标机发送抓包的 50 Mbps 服务曲线超额 `1356 B`，恰为一个最大包；接收抓包为 `8191.25 B`，超过该边界，故不能将整体验收记作 PASS。峰值对应相同 10 包在发送端跨 `2.836 ms`、逐包间隔 `278–341 us`，在接收端跨 `0.859 ms` 且多次间隔 `0–1 us`。网络压缩与接收时间戳误差尚未区分，第 17 次仅将抓包时间戳改用工具已支持的高精度时钟验证，不改 CLI 参数或核心代码。
- 输入捕获确认缺少 `35` 个 RTP 序号，最长到包间隔 `2512.329 ms`；输出最长间隔 `2458.347 ms`。VLC 有 `91` 条 picture-too-late、`169` 条 late-frame-dropping，以及 PCR 迟到、硬件画面分配失败日志。部分输出空档与输入停顿重合，不能将全部播放告警归因于源，也不能宣称播放连续无异常。

第 16 次接收播放命令：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-external\vlc-run16.log --no-video-title-show rtp://@192.168.96.122:6200
```

第 17 次使用同一 CLI 命令，仅 `dir` 为 `rk-a559-external-run17`；VLC 日志路径相应为 `vlc-run17.log`。接收抓包命令：

```powershell
D:\Wireshark\dumpcap.exe -i 7 --time-stamp-type host_hiprec_unsynced -f "host 192.168.130.229 and (udp port 6200 or udp port 6201)" -a duration:270 -q -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-external\receiver-run17.pcapng
```

## 第 17 次 RKMPP 实测：复现接收侧证据不达标

未修改核心与 CLI 参数，仅接收抓包切换高精度单调时钟。CLI PID `1234117`、目标机抓包 PID `1234100`、driver PID `1234097`、VLC PID `25628`；实际执行 `bash /home/tang/rk-a559-run.sh run17`。

- 输出 `224.885541 s`、RTP `141015` 包与 RTCP `50` 包；源码与二进制同第 16 次。源停前核心错误、丢弃、deadline_misses 均为 0；最大驻留 `61.715733 ms`，backlog 高水位仍为 49 包、66444 B。
- 收发内容及顺序全部一致，聚合哈希 `dab87ce11bf9b7ed49deab93a6e0962e3b2307d4798ff7977c75974597ec3cacd`，RTP/TS 错误及捕获丢弃均为 0。目标机发送服务曲线超额 `1356 B`；接收仍为 `11479 B`，故时钟精度不足的假设未获验证，完整链路仍为未通过。
- 输入实际缺少 16 个 RTP 包，最大到包间隔 `2511.404 ms`；输出最大间隔 `3875.483 ms`。最长输出空档附近存在两处输入序号缺口及 `2375.517 ms` 的输入空档，尚不足以把全部输出迟到归因于源。
- 平均单核 CPU `13.09%`、峰值 `78.35%`，RSS 从 `43.26 MB` 到 `51.05 MB`，后段稳定。源停止后于 `16:39:26.876` 按原无进度超时退出，退出码 1；不将退出码解释为正常 EOF。
- VLC 记录 33 条 picture-too-late、PCR 迟到及一次硬件画面分配失败。第 16 次 TS 中实际包含 VPS/SPS/PPS 各 113 次、IDR 112 次，不支持“输出缺少重复参数集”的推断，未据此修改编码器。

接收时间戳的限制依据：[Npcap 时间戳说明](https://npcap.com/guide/wpcap/pcap-tstamp.html)指出，主机时间戳可能受中断批处理和队列延迟影响。该说明仅解释测量边界，不证明本次异常一定来自抓包，也不作为放宽门禁的理由。

当前判定：RKMPP 核心持续运行及原 100 ms 驻留约束连续两轮达标；完整“持续转码、无突发”链路证据仍不达标，禁止标记整体验收成功。独立审查仅针对已实测的退出修复，不代替完整验收。

## 源码交叉审查与清理

两位未参与实现的独立审查者均重新检查全部 23 个生产文件，明确源码 PASS；没有运行 Windows 测试，且两者均指出完整实流验收未通过。专项评分 82/100，见 `QUALITY_SCORE.md`。源码冻结后二进制哈希未变。

目标机本轮 CLI/capture/driver 六个精确 PID 均已退出；VLC 两个精确 PID 已退出。临时运行脚本和 Python 分析器已从目标机删除，内容以文本保留在本地证据目录，未纳入仓库。未新增诊断库。提交仅作为持续运行修复检查点，不标记完整验收成功；PR 保持草稿。
