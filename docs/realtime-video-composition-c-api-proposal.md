# C Beta 固定实时合屏入口提案

状态：2026-10-09用户明确允许按提案实现；包含逐源input_latency_ms（此前已批准）。旧ABI及本文音频输出字段边界保持；实现、双独立源码审查和Release全量构建已完成，真实验收尚未完成。

## 现状与 ABI

原计划要求 2～4 路 RTP/RTCP、固定 grid、指定音源，并同时提供 CLI 和 C API。已批准独立 C 类型、start 入口、音频输入协议字段、逐源 input_latency_ms 与异构 snapshot UNKNOWN；另已批准唯一音频输出目标字段 audio_bitrate_bps。

现有 `mt_beta_realtime_config/input/video_output/snapshot` 无 struct_size/version，不可追加成员或扩展 input union 后仍声称保持 ABI。全部已有结构布局、入口签名和枚举数值保持不变；新增独立类型及 `mt_beta_realtime_start_composition(config, callbacks, session)`，返回同一 opaque session。复用原 stop/release、事件、snapshot 和输出管理入口；不新增媒体执行链。

## 拟新增类型与精确字段清单

| 类型 | 字段 |
|---|---|
| mt_beta_rtp_audio_input | const char* bind_address；uint16_t port；const char* codec_name；uint8_t payload_type；uint32_t clock_rate；uint32_t channels；const char* fmtp |
| mt_beta_composition_input | mt_beta_rtp_video_input video；const mt_beta_rtp_audio_input* audio；uint32_t input_latency_ms |
| mt_beta_composition_grid | 独立枚举 FIXED=1，对应既有内部 Fixed；不改已有枚举 |
| mt_beta_realtime_composition_config | const char* media_id；const mt_beta_composition_input* inputs；size_t input_count；mt_beta_composition_grid grid；size_t audio_source；uint64_t audio_bitrate_bps；uint64_t preparation_storage_limit_bytes；mt_beta_video_output output；mt_beta_realtime_deployment deployment |

audio=NULL 明确表示源无音频；非空必须完整验证，部分字段不能退化为纯视频。audio_source 为零基索引，选中源必须存在真实音频。codec_name 复用既有 RTP registry 支持范围，避免新发明音频 codec 枚举。fmtp 的必需性由实际 codec/session registry 决定。仅支持本次已批准 RTP 独立流输入；不扩 URL、MPEG-TS 输入矩阵。

除已授权的 preparation_storage_limit_bytes 外，不添加 struct_size/version、源标签、容量、时序、除已批准 audio_bitrate_bps 外的音频输出编码配置或逐源 telemetry 字段；若后续需要须独立确认。

## 音频输出合同：已有推导与尚需证据

现有公开 output 虽名为 video_output，但其协议字段仍可用于唯一合屏输出。内部 MediaRealtimeAudioPlannerOptionsResolver 对 muxed 输出形成 AAC-LC requirement，composition 强制 frame transcode；MediaResolvedAudioTargetDecision 从选中源真实事实解析采样率、声道/布局和可用源码率，随后 opened encoder readback 形成 emission、frameSize、实际格式及总资源产品。不能在 Beta mapper 填固定 AAC 码率、采样率、声道或帧长。

### 已确认的缺事实路径

原有 raw RTP 音频路径不能在零音频输出配置下完成准备，源码证据如下：

1. `src/internal/graph/planner/realtime/MediaRealtimeInputPlanner.cpp` 的 `planRawRtp` 填入 codec、采样率、声道等协议事实，但不填 `bitrateBitsPerSecond`，该字段保持 0。
2. `src/internal/graph/planner/audio/MediaResolvedAudioTargetDecision.cpp` 仅在源 `bitrateBitsPerSecond > 0` 时继承未指定的目标码率，因此该路径的目标码率仍缺失。
3. `src/internal/graph/planner/audio/capability/MediaAudioEncoderCapabilityProvider.cpp` 的 `verifyCandidate` 在 `avcodec_open2` 前调用 `rateBits`，缺失或非正码率返回 `NotInitialized`：`audio encoder requires authoritative target bitrate`。

新增显式目标字段已补足这项缺事实；其他准备校验仍可能在发布异步 session 后明确失败，不能将声明完整或 start 返回成功当作媒体能力已完成。现有 CLI 显式音频配置的成功不能替代本入口验收；不得使用有限采样平均码率或 FFmpeg 默认值掩盖缺事实。

### 已批准的最小音频输出目标

用户已批准为新 `mt_beta_realtime_composition_config` 增加 `uint64_t audio_bitrate_bps`：调用者要求的音频编码目标码率，必须为正且可由现有整数 kbps 合同精确表示。该字段现已接入 mapper，复用现有整数 kbps 精确转换并填入 audio.bitrateKbps；无默认值。

为补足上述缺失，不必同时增加 codec、sampleRate、channels：muxed resolver 已形成 AAC-LC requirement，选中源提供采样率及声道事实。该字段可沿现有 target/capability planner 和 encoder readback 形成当前码率及资源产品；它不等价于 CBR 保证。当前 `MediaResolvedAudioTargetDecision` 保留 `Auto`，capability provider 未按 CBR/VBR 分支；`AudioCodecResolverNode` 应用数值码率选项而没有消费该 RC 枚举的策略分支。若要求显式 CBR/VBR 合同，需要另行调查并授权相应生产核心变更，不能仅新增 RC 字段声称已保证。

## 映射、所有权与失败语义

新 mapper 将数组映射至同一 MediaRealtimeRtpTranscodeRequest.composition，输出 execution 为 AudioVideo；每源成员由既有 CompositionInputPlanner 从实际音频声明验证，不能全局强制所有源 AV。复用现有 video output/deployment mapper 与既有 planner，进入同一 MediaRealtimeBetaSession → MediaRealtimeVideoRunController。

start 返回前深拷贝数组、所有 video/audio 地址和 fmtp、codec_name、media_id、output 字符串；不保留 caller 指针。callbacks.user_data 仍由 caller 管理，沿现有 callback/release 同步约束。校验或分配失败不得发布 session；准备后失败、取消和资源归还沿原 session/controller 的首错、事件、RAII、stop token 流程，不新建线程模型。源断流遵循合屏持续输出策略，不等同 session 立即 completed；全部可信结束或显式 stop 才结束。

## snapshot.input_codec 的最小语义

保持字段及枚举 ABI。旧单源行为不变；新合屏入口仅在全部实际视频源 codec 相同时返回该 codec，异构或未确定时返回现有 UNKNOWN。不将首源 codec 冒充全体，不新增 MIXED 枚举，不新增逐源 snapshot。backend 同样保留现有报告含义，不能声称代表每源后端。该语义已获批准。STARTING 阶段保持 UNKNOWN；仅在全部实际源完成准备、收到原生命周期 prepared 回调后回填同构 codec，不能提前用声明代替准备结果。

## 授权状态与交付门禁

1. 独立 C 类型/入口、音频输入协议事实字段及逐源 input_latency_ms 已获批准；逐源等待值必填正值，无默认。
2. 异构合屏 snapshot.input_codec 使用现有 UNKNOWN 的语义已获批准。
3. audio_bitrate_bps 已获明确批准并接线；不新增 codec、RC、采样率或声道输出目标字段，不声称 CBR。

冻结后双独立源码审查、Release 全量构建、真实 C API 生命周期与原规格 realtime 输出证据、旧单源 ABI/行为回归，随后按 Windows→RKMPP 顺序验收。本文没有新增测试体系、默认值或外部 FFmpeg 修改范围。

## 工业实践依据

[GStreamer ABI 约定](https://gstreamer.freedesktop.org/documentation/frequently-asked-questions/developing.html)以主版本区分不兼容 API/ABI；本仓库旧结构没有版本或大小协商，因此保留旧布局，新增独立入口与类型。[GLib 字符串复制合同](https://docs.gtk.org/glib/func.strdup.html)明确复制和释放责任；本入口使用既有 `std::string`/`std::vector` 所有权，在发布异步 session 前复制调用者数据，并复用原生命周期与错误传递。

## 2026-10-09 实施门禁进度

独立C入口、逐源音频协议/等待和已授权audio_bitrate_bps接线完成；旧ABI保持。两个未参与实现的独立审查者均明确源码PASS（A首次因额度中断不计通过，恢复后完成复审）。Release clean-first/all-target configure/build exit0，realtime CLI及Beta库产物分别10:32:53/10:32:52；git diff --check成功。尚未执行真实C调用方生命周期、释放配置后运行及旧单源回归，不代表C API完整验收。r134三源输出调度FAIL仍独立保留。

2026-10-10 两名新的独立审查者复核当前增量均明确源码PASS：正值精确kbps、深拷贝、所有权发布、stop/release、旧ABI及prepared后snapshot语义。未执行真实C调用方验证，不替代运行门禁。

## 2026-10-10 真实调用方验证准备

当前Beta产物为静态库，realtime CLI仍直接调用application层，不能拿CLI成功替代C入口执行。下一轮使用临时原生C调用方链接已全量构建的Beta/application/core及相同FFmpeg import libs，测试参数校验、start返回后覆盖并释放调用方配置、prepared后snapshot、回调、准备期取消和运行期stop/release；旧单源入口另保留回归。临时调用方及对象/可执行文件只在out/acceptance，本项结束按清单清除，不纳入版本库或永久测试体系。

已核验MediaRealtimeBetaTemporaryDescription用GetTempPathW/GetTempFileNameW创建并删除会话描述。调用方进程的TEMP/TMP须定向到项目内本轮目录，避免本轮验证生成或删除项目外文件；不增加公共配置字段、不修改生产临时文件实现。callbacks.user_data存活至release结束，release在callback之外且与其他API调用同步。C入口音频码率只有目标值，不能将CLI的audio-rc cbr验收声明直接转嫁到该入口。

## 2026-10-10 r146 真实 C 调用方：参数/启动前取消与 prepared 身份失败

临时原生 C 调用方链接 10:25:07 的既有 Release Beta/application/core 库，单独编译 exit0；未改生产源码。配置为三路 H264 1280×720/30，RTP 61030/61040/61020，逐源等待100/150/200ms；第三路AAC 44100Hz双声道61022，输出HEVC 1280×720/30、视频CBR 8000000bps、GOP60，音频目标192000bps（不宣称音频CBR），MPEGTS/RTP62020，出口50000000bps/100ms。start返回后清零释放全部632字节配置及字符串，另分配同尺寸poison；后者不保证地址复用。callback上下文保留至release。

参数验证实际命令（PATH前置项目Release，TEMP/TMP均为项目out/acceptance/composition-c-api-r146-temp）：
```text
D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-caller.exe invalid-bitrate
D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-caller.exe invalid-latency
D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-caller.exe inexact-bitrate
D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-caller.exe cancel
```

前三项分别audio_bitrate_bps=0、源1等待0、audio_bitrate_bps=192001，均INVALID_ARGUMENT、session未发布、callback0、exit0。cancel初次exit6来自临时断言错误：错误要求COMPLETED状态和无ERROR。实际既有controller返回Cancelled/CallerStop，Beta映射ERROR_CANCELLED(stage STOP)、FAILED状态、一次COMPLETED事件(reason REQUESTED_STOP)。双独立审查确认无需改生产；临时断言按此契约修正后复跑exit0，callbacks4/errors1/completions1/running_seen0、release NULL，原文为realtime video run stop was requested before preflight。只证明preflight前取消，不证明prepare内部中断或正常运行停止。

三源实流调用方PID36016自然exit6，11:01:36.013前后首错：prepared output identity does not match the owned description；ERROR_INTERNAL / RUNTIME_START / STARTUP_FAILURE，最终FAILED，callbacks4/errors1/completions1/running0，release NULL。STARTING snapshot codec/backend UNKNOWN，未产生输出，VLC录制0B。未取得运行期CPU/内存趋势与A/V漂移，不能判媒体或配置释放后运行通过。调用方在后台监控挂接前即失败，后续复验需从启动即采集进程指标。

代码证据：CompositionPreparer生成report但不填outputId；RunController仅非合屏VideoOnly分支registerInitialOutput，Beta handlePrepared拒绝空ID。还须调查输出ready、快照、retirement与描述文件所有权的完整生命周期，不能只填常量ID绕过。旧间歇期限与此为不同问题，均未关闭。

抓包PID16768 Ctrl-C exit0，103522包/drop0；VLC4644 RC quit exit0。三源25348/34856/7460在判失败后按精确PID强制清理，exit-1，未完成120秒、不算源自然结束。所有启动命令如下（含日志、捕获、VLC实际完整选项）：

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$env:PATH='D:/Code/MyCode/MediaTranscode/out/build/x64-release;'+$env:PATH; $env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-temp'; $env:TMP=$env:TEMP; $p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-caller.exe' -ArgumentList 'composition' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-caller.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-caller-error.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-c-api-r146- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r146-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


```

本轮结论：参数拒绝及preflight前取消有限验证通过；真实合屏C入口FAIL，旧ABI、正常运行/停止、完整Windows与RKMPP验收仍开放。按文件清单清理本轮临时调用方源码/obj/exe、日志、pcapng和0B录制；不保留临时测试代码。

清理核验：16件本轮文件及空temp目录已逐项删除，前缀扫描无残留；宿主CIM无本轮媒体进程，git diff --check成功。原120秒源和正式Release保留。

双独立根因审查一致：除初始ID外，合屏未接入输出Preparing→WaitingForRandomAccess→Running/OUTPUT_READY→Retired生命周期，不能仅registerInitialOutput补丁修复。下一项抽取复用既有状态发布和就绪判据，固定输出仍用原DAG，动态add/remove保持当前限制。权威就绪事实为ScheduledDatagramSender成功commitSubmittedPrefix后的VideoRandomAccessUnitEnd evidence，按现有controller核对activation/generation；不能拿编码包数、SDP存在或prepared代替。该证据也不单独证明AAC到达或接收端可播放。退役通知与SDP释放须在runtime停止、join和资源回收后；异常和取消提前返回同样必须收敛。尚未实现或验证此修复，完整任务维持FAIL。
## r147 输出生命周期修复设计（实施中）

依据GStreamer官方States设计（https://gstreamer.freedesktop.org/documentation/additional/design/states.html），异步启动受理、媒体实际就绪、线程停用与资源释放是不同阶段；本仓库复用既有状态机及发送完成证据，不照搬GStreamer状态名或preroll。先抽取RuntimeBranch已有sender证据收集器供固定runtime共用，保留branch发布锁和退役前快照。collector只返回已提交随机访问单元的不可变值，不选择策略、不增线程/媒体引用/队列；遍历沿现有scheduler节点序列，生命周期owner须防止并发拓扑销毁。MPEGTS已有TsMuxSession→PendingEmission→ProtocolDatagramBatch→materializer传递VideoRandomAccessUnitEnd，无需新造媒体判定。

后续统一初始输出身份，抽取共享发布逻辑；固定AV和合屏均绑定现有runtime，动态单视频保留既有分支管理。就绪期限须来自实际planner产品，activation/generation匹配sender证据，原始失败优先。运行结束先保存最终报告，再stop/join/reset完整释放后通知Retired；仅stop不足以证明节点资源销毁。prepared/compile/register/start异常早退也纳入同一收敛，回调异常不得掩盖首错。对外类型及外部依赖不变；生产接线、双审、Release和C入口实流复验完成前不算修复通过。
r147现场代码核验补充：MediaRealtimeRuntimeCompletion只stop，RuntimeReset在reset异常后仅abort，不足以保证节点析构；计划在runtime完整作用域退出后显式终结固定输出，禁止在noexcept析构调用observer。Beta Retired回调先于handleOutcome，回调异常可能抢先占用emergency首错槽；修复需保持生产首错优先和SDP删除可达。

已实施第一步：MediaVideoOutputReadyCollector.h抽取原RuntimeBranch sender遍历，原branch委托共享helper，锁及退役证据缓存不变。尚未接固定输出、尚未构建/实流复验，不代表r146修复。ContinuousOutput同步plan没有maximumWaitNs；源启动期限不能替代输出随机访问首包期限。实际代际应来自protocol authority当前activation；不得硬编码1或套用单视频startup产品。下一步完成输出就绪期限的权威规划与共享生命周期接线，再集中全量Release和真实C调用方验证。
期限设计复核：可显式沿用现有policy.firstOutputTimeout作为操作watchdog，用同一runtime起点形成绝对期限；这将既有编码首包观测加强到线端就绪观测，不宣称等价重构或媒体硬期限。Ready仍取sender提交证据与当前activation代际；应比较证据submittedAt同一steady时钟时间，不能因轮询晚到误拒先前已就绪。编码计数不再替代该判据，也不重启计时。取消/原始失败优先，旧固定AV同规格回归。无需新增公共参数或经验常量；若要求媒体硬期限，当前事实不足，不能以policy冒充planner服务保证。两名独立审查已明确通过2文件collector等价抽取；Release全量重建进行中。
r147构建结果：双独立源码PASS后，VS2026 Release clean-first/all-target configure/build均exit0；realtime产物2026-10-10 11:09:50、5792768字节。未执行本轮媒体链路，未关闭r146失败。未新增测试文件；原120秒源124427809字节保留，git diff --check通过。

## r148 固定输出生命周期接线（已双审，待运行）

新增FixedOutputLifecycle与共享OutputPublisher；固定AV/合屏在prepared前注册初始输出，原动态VideoOnly继续原分支控制。collector使用r147共享实现。start前在协议master域记录操作期限，poll仅根据已提交随机访问视频证据、当前activation代际及同域时间发Running/READY；不把编码计数或音频到达当证明。此处修正r147设计中可能混淆的“同一steady时间”：submittedAt实际为protocol master域，不能直接比较steady epoch。

runtime执行封装在内层作用域，最终报告先保存；runtime及其节点析构后由外层发Failed/Retired。owner先构造路径再注册ID，绑定ID无分配；快照发布使用const引用，Retired直接erase；错误文本复制失败也先尝试退役，原outcome优先。Beta退役清理覆盖回调异常，已有controller failureSignal时不让callback异常占用首错槽，原错误全文由handleOutcome保留。Waiting/READY回调后重新检查stop。无新公共字段、线程、媒体队列或平台分支，动态add/remove仍未为合屏开启。

首轮双审FAIL的注册孤儿、错误文本复制跳过退役、通知后继续启动worker三项已修；七文件双独立复审明确源码PASS，git diff --check通过。全量Release重建进行中。C调用方实流、输出快照/SDP寿命、正常停止、旧固定AV/动态回归及完整Windows后RKMPP仍未通过；不把本片源码PASS当完整验收。
## r148 真实 C 入口复验结果（2026-10-10，有限场景）
七文件双独立源码复审PASS后，VS2026 Release clean-first/all-target configure/build exit0，realtime产物11:20:01、5801472字节；临时C调用方链接同批Beta/application/core，编译exit0。生产未再改动。
三源原规格H264 1280×720/30、约8Mbps；源0/1纯视频61030/61040，源2视频61020+AAC44100双声道61022，逐源等待100/150/200ms。HEVC1280×720/30 CBR8000000bps/GOP60，AAC目标192000bps，MPEGTS/RTP62020，出口50000000bps/100ms；不声明音频CBR。start返回后清零释放调用方配置和全部字符串；事件上下文存活至release。

调用方PID17448（session6862）正常请求停止exit0；三源AV19304、V61030 28492、V61040 16556均120秒/3600帧自然exit0。VLC30960 RC quit exit0；dumpcap35620 Ctrl-C exit0。全部源结束后持续输出黑场，未声称C会话自然EOF。确认原r146身份错误不再出现，prepared后codec=H264/backend=CUDA，输出ID1从WaitingForRandomAccess到Running/READY。READY/Retired/COMPLETED各一次，errors0，最终session COMPLETED/REQUESTED_STOP、输出集合0；READY时SDP存在，COMPLETED回调内SDP已删除，release后仍不存在且句柄NULL。仅证明这次正常生命周期；准备取消、回调异常、旧ABI、旧固定AV/动态和realtime CLI回归未由本轮覆盖。

调用方每秒采样142次，整机归一CPU均值2.8191204577%、峰4.828146%，WS峰259133440B、private峰865853440B；终态WS91987968B、private326332416B。运行workerErrors0，encoded47209/47208。内部AV漂移115条、绝对最大22631ns，仅同步内部证据，不代表内容AV漂移。aggregate视频4225次，源real/black为3483/742、3568/657、3504/721；音频6065次，real5151744+silence1058816=6210560=6065×1024。最终contribution_generation0对应失源黑场。

用户恢复桌面后实际查看VLC截图：live/live-full受150% DPI坐标影响裁切，显示三路局部画面和右下黑格；不冒称完整窗口。SetThreadDpiAwarenessContext(-4)后live-dpi成功取得完整窗口，此时源已结束，确认为黑场。另实际查看原始出口n1800完整1280×720抽帧，三路画面/右下黑格；离线抽帧不替代完整现场时序。

抓包411879/drop0；出口86537 RTP、SSRC1166AD2A、序号异常0，已识别跳过1个工具文字footer，提取TS101417728B。ffprobe视频4223/音频6062包，DTS非递增0；完整AV decode exit0，warning日志0B。sender86572次提交=86537RTP+35RTCP，deadline/pressure/partial/ambiguous失败0，但delivery_evidence=not_proven。停止时reservation wait被取消，sender尾部backlog1包/228B、mux pending188B、AAC队列2帧；正常请求停止不代表完整drain。

aggregate同一cursor1778/audio2614272最大process54.6622ms、video54.5997ms、canvas54.5286ms；audio最大0.8405ms，built事件最大间隔64.2918ms。between_calls82.22ms位于启动cursor0等待。未复现r143绝对期限失败，不代表旧问题修复，也不能将wall-clock归因GPU。

实际启动命令（TEMP/TMP项目内；PATH前置本批Release DLL；VLC详细选项以此为准）：
```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-capture.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$env:PATH='D:/Code/MyCode/MediaTranscode/out/build/x64-release;'+$env:PATH; $env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-temp'; $env:TMP=$env:TEMP; $p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-caller.exe' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-caller.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-caller-error.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-c-api-r148- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


```
离线核验命令：
```powershell
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-tshark.log'; exit $LASTEXITCODE
$ErrorActionPreference='Stop'; $count=0L; $anomalies=0L; $footer=0; $last=@{}; $stream=[IO.File]::Create('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire.ts'); try { foreach($line in [IO.File]::ReadLines('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-output-payloads.txt')) { if($line -eq 'No matching RTP/H.264 payload dumped.') {$footer++; continue}; if(!$line){continue}; $b=[Convert]::FromHexString($line.Replace(':','')); if($b.Length -lt 12 -or ($b[0]-shr 6)-ne 2){throw 'Invalid RTP'}; $seq=[int]$b[2]*256+$b[3]; $ssrc=[Convert]::ToHexString($b[8..11]); $off=12+4*($b[0]-band 15); if($b[0]-band 16){$off+=4+4*([int]$b[$off+2]*256+$b[$off+3])}; $end=$b.Length; if($b[0]-band 32){$end-=$b[-1]}; if($off -gt $end -or ($end-$off)%188 -ne 0){throw 'Invalid TS payload'}; if($last.ContainsKey($ssrc) -and $seq -ne (($last[$ssrc]+1)%65536)){$anomalies++}; $last[$ssrc]=$seq; $stream.Write($b,$off,$end-$off); $count++ } } finally {$stream.Dispose()}; [pscustomobject]@{Packets=$count;SequenceAnomalies=$anomalies;TextFooters=$footer;SSRC=($last.Keys -join ',');Bytes=(Get-Item 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire.ts').Length} | ConvertTo-Json

$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress

& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire-decode.log'; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v error -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire.ts' -vf 'select=eq(n\,1800)' -frames:v 1 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r148-wire-frame.png'; exit $LASTEXITCODE
```
停止调用方使用已核对PID17448/程序名/本轮命令行的独立控制台Ctrl-C，调用方在主线程request_stop/release。其他进程亦按上述精确PID停止，无强杀C调用方。清理前保存下列原始终态及关键日志；随后按本轮清单删除源码、obj/exe、日志、pcapng、TS、PNG、payload文本与空temp目录，保留原测试源及正式Release。完整合屏仍75/FAIL。

原始记录：
```text
EVENT type=1 state=1 error=0 stage=0 reason=0 output=0 output_state=0 detail= path= exists=0
EVENT type=5 state=0 error=0 stage=0 reason=0 output=1 output_state=2 detail= path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r148-temp\mtbF50D.tmp exists=1
EVENT type=1 state=2 error=0 stage=0 reason=0 output=0 output_state=0 detail= path= ex[2026-10-10 11:21:44.475] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=75
EVENT type=5 state=0 error=0 stage=0 reason=0 output=1 output_state=3 detail= path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r148-temp\mtbF50D.tmp exists=1
EVENT type=2 state=0 error=0 stage=0 reason=0 output=1 output_state=3 detail= path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r148-temp\mtbF50D.tmp exists=1
[2026-10-10 11:24:05.312] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=15225 committed_datagrams=86572 committed_payload_bytes=102458552 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=86573 pacing_cancelled=0 pacing_submitted=86572 pacing_maximum_submit_lateness_ns=22771220 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=1 backlog_current_wire_bytes=228 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=55540300 last_materialized_sequence=86573 last_scheduled_sequence=86573 last_submitted_sequence=86572 last_committed_sequence=86572 shared_scope_charged_datagrams=86572 shared_scope_charged_wire_bytes=104882568 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=86537 endpoint_1_committed_payload_bytes=102456172 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=35 endpoint_2_committed_payload_bytes=2380
[2026-10-10 11:24:05.312] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=188 peak_pending_bytes=92872 pressure_failures=0 access_units=10286 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=2814 maximum_mux_input_after_emit_ns=24612924 maximum_mux_input_after_dispatch_ns=-75387076 worst_mux_input_ready_ns=63245329900 worst_mux_input_emit_ns=63220716976 worst_mux_input_dispatch_ns=63320716976 maximum_protocol_batch_produced_after_release_ns=24632124 worst_protocol_batch_produced_ns=63245349100 worst_protocol_batch_release_ns=63220716976 worst_protocol_batch_deadline_ns=63320716976 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-10 11:24:05.314] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=44151 maximum_ns=54662200 begin_steady_ns=9628665877400 end_steady_ns=9628720539600 video_cursor=1778 audio_cursor=2614272 next_video_cursor=1778 next_audio_cursor=2614272 error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-10 11:24:05.314] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=4226 maximum_ns=54599700 begin_steady_ns=9628665903000 end_steady_ns=9628720502700 video_cursor=1778 audio_cursor=2614272 next_video_cursor=1778 next_audio_cursor=2614272 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 11:24:05.314] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=4226 maximum_ns=54528600 begin_steady_ns=9628665944600 end_steady_ns=9628720473200 video_cursor=1778 audio_cursor=2614272 next_video_cursor=1778 next_audio_cursor=2614272 error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-10 11:24:05.314] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-c-api-r148.output output_generation=1 video_commits=4225 audio_commits=6065 real_samples=5151744 silence_samples=1058816 source[0].group=composition-c-api-r148.source.0 source[0].real_frames=3483 source[0].black_frames=742 source[0].contribution_generation=0 source[1].group=composition-c-api-r148.source.1 source[1].real_frames=3568 source[1].black_frames=657 source[1].contribution_generation=0 source[2].group=composition-c-api-r148.source.2 source[2].real_frames=3504 source[2].black_frames=721 source[2].contribution_generation=0
EVENT type=5 state=0 error=0 stage=0 reason=0 output=1 output_state=5 detail= path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r148-temp\mtbF50D.tmp exists=1
EVENT type=1 state=3 error=0 stage=0 reason=0 output=0 output_state=0 detail= path= exists=0
EVENT type=1 state=4 error=0 stage=0 reason=0 output=0 output_state=0 detail= path= exists=0
EVENT type=4 state=0 error=0 stage=0 reason=2 output=0 output_state=0 detail= path= exists=0
RELEASE null=1 errors=0 ready=1 retired=1 completed=1 sdp_remains=0
ASSERT ready_id=1 running_id=1 final_count=0 ready_exists=1 completed_path_gone=1
```

双独立运行证据复核均明确：仅r148 C入口/输出生命周期场景PASS，完整合屏FAIL。两者实际查看wire完整三格与live-dpi黑场；现场活动三格只由主线程查看的DPI裁切截图支持局部观察，不判完整现场连续播放通过。原退出码/RTP序号/DTS来自主线程实际工具命令。

清理完成：24件列明本轮文件和空temp目录已逐项删除，前缀扫描无残留，宿主媒体/分析进程为空；原120秒源和正式Release保留，git diff --check通过。

## r149 回调异常验证未触发（2026-10-10）
生产与Release沿用r148，未修改。临时C++公共C入口消费者新增发布事件同步：start前创建事件，start返回后原子发布session并signal；Waiting回调先等待，分别request_stop或throw。成功断言检查stop OK/CANCELLED/STOP、快照归零、SDP回收；throw后回调禁用预期不计完成事件。独立源码复核PASS，临时编译exit0。

实际没有到达Waiting回调，两个场景均FAIL/未验证：stop PID30112于12:24:08报告raw RTP video signaling probe reached open timeout before complete parameter sets; matching_rtp_packets=0 matching_datagram_bytes=0 parameter_sets=none；throw PID34248于12:26:30同错。均自然exit7、triggered0、worker_progress0、ERROR_NOT_INITIALIZED/PREFLIGHT、FAILED/STARTUP_FAILURE、输出集合0、release NULL。不能将此结果当作回调语义或worker不启动验证通过。

首个VLC启动自动审批超时，没有进程创建，重试成功PID27284；三源启动明显错开，第二次调用前两路已经结束仍误启动，是本轮编排错误，不是新生产缺陷。三源PID13140/23952/34448分别完整120秒3600帧自然exit0；VLC RC quit exit0。无媒体输出，VLC录制0B；无A/V漂移数据。两个caller逐秒记录CPU/内存，仅预检活动，非运行期性能验收。下一轮应在单次已批准的宿主启动操作中直接启动全部源/消费者，避免每个启动分别等待工具审批；不增时长/降规格，不把超时改成通过。

本轮实际命令：
```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-c-api-r149- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-source0.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-source1.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-source2.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode


$env:PATH='D:/Code/MyCode/MediaTranscode/out/build/x64-release;'+$env:PATH; $env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-temp'; $env:TMP=$env:TEMP; $p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-caller.exe' -ArgumentList 'stop' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-stop.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-stop-error.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
$env:PATH='D:/Code/MyCode/MediaTranscode/out/build/x64-release;'+$env:PATH; $env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-temp'; $env:TMP=$env:TEMP; $p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-caller.exe' -ArgumentList 'throw' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-throw.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r149-throw-error.log' -PassThru; Write-Output "PID=$($p.Id)"; $p.WaitForExit(); Write-Output "EXIT=$($p.ExitCode)"; exit $p.ExitCode
```
本轮结束按清单清理临时cpp/obj/exe、stop/throw与源/VLC日志、录制和空temp目录；原120秒源及Release保留，完整目标保持未完成。

stop samples=31 cpu_mean=0.202919870967742 cpu_max=0.629758 ws_max=91607040 private_max=424091648
throw samples=31 cpu_mean=0.0850088064516129 cpu_max=0.419426 ws_max=53194752 private_max=378699776

## r150 Waiting 回调停止/异常复验（2026-10-10，辅助门禁）
沿用r148冻结Release，无生产修改。临时消费者仅修改轮次名，发布同步与断言经独立复审PASS。一次宿主调用内直接执行各绝对路径启动命令，避免r149分次工具启动错位；仍用原三路120秒源，不增timeout、不降低媒体参数。三个FFmpeg源PID18956/11876/8372，VLC10864；stop调用方16576、throw调用方35956。

stop：目标Waiting回调1次，request_stop status0，ERROR_CANCELLED/STOP，snapshot FAILED/REQUESTED_STOP；Retired与COMPLETED各1，READY0，最终输出列表0，COMPLETED前SDP删除，release NULL，exit0。
throw：Waiting回调主动抛出C++异常，后续用户回调禁用；snapshot FAILED/STARTUP_FAILURE，READY/Retired/Completed回调均0，终态列表0，release NULL且SDP不存在，exit0。该场景只在release后检查文件，不能直接证明release前已删除；无后续回调是既有保护语义，不是声称运行成功。
两者progress/in/out=0，日志无runtime start.begin/start.done；结合回调后停止检查源码，支持未启动媒体workers。没有媒体输出或A/V漂移，VLC接收只是完整连接端点，不作为播放通过。

双独立证据复核均限定PASS，stage差异如实保留：output Failed/Retired事件stage6描述输出仍处Publication/RuntimeStart；session ERROR stage8描述因调用方停止终止，不是替换首错。仍未覆盖既有运行错误后回调再抛异常、READY后活动停止、旧ABI、旧固定AV/动态回归及完整矩阵；r149两次未触发失败不改记通过。

实际命令（直接宿主执行，未写入测试脚本）：
```powershell
$ErrorActionPreference='Stop'
$r150Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-c-api-r150- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'r150Vlc='; Write-Output $r150Vlc.Id
$r150Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-source0.log' -PassThru; Write-Output 'r150Source0='; Write-Output $r150Source0.Id
$r150Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-source1.log' -PassThru; Write-Output 'r150Source1='; Write-Output $r150Source1.Id
$r150Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-source2.log' -PassThru; Write-Output 'r150Source2='; Write-Output $r150Source2.Id
$env:PATH='D:/Code/MyCode/MediaTranscode/out/build/x64-release;'+$env:PATH; $env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-temp'; $env:TMP=$env:TEMP; $r150Stop=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-caller.exe' -ArgumentList 'stop' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-stop.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-stop-error.log' -PassThru; Write-Output 'r150Stop='; Write-Output $r150Stop.Id
$r150Stop.WaitForExit(); Write-Output "STOP_EXIT=$($r150Stop.ExitCode)"
$env:PATH='D:/Code/MyCode/MediaTranscode/out/build/x64-release;'+$env:PATH; $env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-temp'; $env:TMP=$env:TEMP; $r150Throw=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-caller.exe' -ArgumentList 'throw' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-throw.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r150-throw-error.log' -PassThru; Write-Output 'r150Throw='; Write-Output $r150Throw.Id
$r150Throw.WaitForExit(); Write-Output "THROW_EXIT=$($r150Throw.ExitCode)"
$r150Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r150Source0.ExitCode)"
$r150Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r150Source1.ExitCode)"
$r150Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r150Source2.ExitCode)"
if($r150Stop.ExitCode -ne 0 -or $r150Throw.ExitCode -ne 0){exit 7}
```

stop samples=7 cpu_mean=0.348351 cpu_max=0.963487 ws_max=214294528 private_max=735330304
```text
EVENT type=1 state=1 error=0 stage=0 reason=0 output=0 output_state=0 detail= path= exists=0
CALLBACK_STOP waiting id=1 status=0
EVENT type=5 state=0 error=0 stage=0 reason=0 output=1 output_state=2 detail= path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r150-temp\mtb283A.tmp exists=1
EVENT type=5 state=0 error=7 stage=6 reason=0 output=1 output_state=6 detail=realtime video run stop was requested during output notification path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r150-temp\mtb283A.tmp exists=1
EVENT type=5 state=0 error=7 stage=6 reason=0 output=1 output_state=5 detail=realtime video run stop was requested during output notification path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r150-temp\mtb283A.tmp exists=1
EVENT type=3 state=0 error=7 stage=8 reason=0 output=0 output_state=0 detail=realtime video run stop was requested during output notification path= exists=0
EVENT type=1 state=5 error=0 stage=0 reason=0 output=0 output_state=0 detail= path= exists=0
EVENT type=4 state=0 error=0 stage=0 reason=2 output=0 output_state=0 detail= path= exists=0
RELEASE null=1 errors=1 ready=0 retired=1 completed=1 sdp_remains=0
ASSERT ready_id=0 running_id=0 final_count=0 ready_exists=0 completed_path_gone=1
CALLBACK_ASSERT mode=0 triggered=1 worker_progress=0
```

throw samples=5 cpu_mean=0.43795 cpu_max=1.258277 ws_max=150937600 private_max=573067264
```text
EVENT type=1 state=1 error=0 stage=0 reason=0 output=0 output_state=0 detail= path= exists=0
CALLBACK_THROW waiting id=1 path=D:\Code\MyCode\MediaTranscode\out\acceptance\composition-c-api-r150-temp\mtb417E.tmp
RELEASE null=1 errors=0 ready=0 retired=0 completed=0 sdp_remains=0
ASSERT ready_id=0 running_id=0 final_count=0 ready_exists=0 completed_path_gone=0
CALLBACK_ASSERT mode=1 triggered=1 worker_progress=0
```

最终：同一宿主句柄93409退出0，两个caller退出0、三个source退出0；源尾日志均120秒3600帧。VLC RC quit后CIM确认进程消失，未单独取得VLC exitcode；录制0B。r149/r150每轮15件临时文件及各自空temp目录按清单清理，原源/Release保留。

## r151 历史头文件单视频入口回归（2026-10-10）

旧头来自commit 6dc12d8549fd198acbf3a90ef5ef1dce34dfab5f:include/media_transcode_beta/realtime.h，独立复核内容一致（忽略换行）。临时C消费者仅包含旧头，链接r148 Release，编译exit0；sizeof config/snapshot/event=144/184/56。源H2641280×720/30约8Mbps，输出HEVC1280×720/30 CBR8Mbps、GOP60、MPEG-TS/RTP，无音频。

PID：VLC27248、dumpcap31124、源33676、caller36572；同一宿主session95644取得SOURCE_EXIT=0/CALLER_EXIT=7。源完整120秒3600帧。源尽后旧VideoOnly路径按原无进度超时退出：ERROR10/stage7，FAILED/reason5，原文realtime runtime made no progress before timeout。没有主动停止；消费者沿用COMPLETED/REQUESTED_STOP且errors0断言，故exit7。本轮整体场景未通过，不更改原结果；源码独立复核该超时分支与HEAD一致，不属于r148固定AV生命周期。

有限旧入口/回调/清理事实：READY/Retired/Completed各1，ready_id=running_id=1；final_count0，ready_exists1，completed_path_gone1，release NULL，SDP无残留。只证明本次历史头编译调用路径，不证明全部旧二进制ABI兼容或主动取消。未取得活动现场截图，不判现场播放通过。

129个CPU/内存样本：CPU均值1.841544248%、峰值2.411053%；WS峰190042112B、private峰441442304B，终态WS49426432/private40280064B。无音频，A/V漂移不适用。抓包188078/drop0；输出81382RTP、SSRC7B6C0788、序号异常0，提取TS100000208B（跳过工具已知文本footer1）。3558视频包DTS非递增0；完整视频解码exit0、日志0B。sender81411=81382RTP+29RTCP，deadline/pressure/partial/ambiguous0，backlog0、mux pending0；delivery_evidence=not_proven，不能宣称端到端交付或全部排空。

VLC通过RC quit、capture通过核验PID后的Ctrl-C关闭；未独立取得二者退出码。实际启动命令：
```powershell
$ErrorActionPreference='Stop'
$r151Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-c-api-r151- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'r151Vlc='; Write-Output $r151Vlc.Id
$r151Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-capture.log' -PassThru; Write-Output 'r151Capture='; Write-Output $r151Capture.Id
$r151Source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-source1.log' -PassThru; Write-Output 'r151Source='; Write-Output $r151Source.Id
$env:PATH='D:/Code/MyCode/MediaTranscode/out/build/x64-release;'+$env:PATH; $env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-temp'; $env:TMP=$env:TEMP; $r151Caller=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-caller.exe' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-caller.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-c-api-r151-caller-error.log' -PassThru; Write-Output 'r151Caller='; Write-Output $r151Caller.Id
$r151Source.WaitForExit(); Write-Output "SOURCE_EXIT=$($r151Source.ExitCode)"
$r151Caller.WaitForExit(); Write-Output "CALLER_EXIT=$($r151Caller.ExitCode)"; exit $r151Caller.ExitCode
```

r151清理：17件列明文件及空temp目录已删除，宿主四个PID均不存在。审查A实际读取原日志/源码，有限旧入口及清理PASS；审查B在清理后仅复核本节归档（此前已核验旧头/调用方源码），有限归档PASS，不能声称双人重算原始运行数据。整体场景FAIL保留。

## r152 realtime CLI 现场复验（2026-10-10，运行中）
沿用r148 Release，无生产修改。PID VLC4396/capture36912/源7868、37564、13392/CLI20812；后台逐秒CPU、WS、private记录。现场截图API返回句柄无效，随后OpenInputDesktop/GetUserObjectInformation实测Screen-saver，画面项未验证。实际命令：
```powershell
$ErrorActionPreference='Stop'
$r152Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r152- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r152Vlc.Id
$r152Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r152Capture.Id
$r152Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r152Source0.Id
$r152Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r152Source1.Id
$r152Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r152Source2.Id
$r152Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r152 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r152Cli.Id
$r152Clock=[Diagnostics.Stopwatch]::StartNew(); $r152LastTime=0.0; $r152LastCpu=0.0; $r152Cores=[Environment]::ProcessorCount
while(-not $r152Cli.WaitForExit(1000)) {$r152Cli.Refresh(); $r152Time=$r152Clock.Elapsed.TotalSeconds; $r152Cpu=$r152Cli.TotalProcessorTime.TotalSeconds; $r152Usage=100*($r152Cpu-$r152LastCpu)/(($r152Time-$r152LastTime)*$r152Cores); $r152Line="seconds=$r152Time cpu=$r152Usage ws=$($r152Cli.WorkingSet64) private=$($r152Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-monitor.log' -Value $r152Line; $r152LastTime=$r152Time; $r152LastCpu=$r152Cpu }
Write-Output "CLI_EXIT=$($r152Cli.ExitCode)"
$r152Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r152Source0.ExitCode)"
$r152Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r152Source1.ExitCode)"
$r152Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r152Source2.ExitCode)"
exit $r152Cli.ExitCode
```

r152最终结果：同一启动/监控session65507取得CLI_EXIT=0与三源EXIT=0；三源各完整120秒3600帧，源停止后继续合屏输出，精确PID20812 Ctrl-C请求caller_stop，不能记作源尽自然退出。VLC RC quit与capture精确PID36912 Ctrl-C关闭，未独立取得二者exitcode。prepared_output_id1，waiting→running→retired；最终queued/workers/payload字节/objects均0，reservation/release101701/101701，workerErrors/errors0，pressureFailures30300（不称无背压）。
137个后台样本CPU均值2.091415074%、峰3.861775142%，WS峰269295616B/private峰879681536B；CLI内部426样本CPU均值2.150699%、峰5.858586%、WS峰269287424B，两套采样口径不同。113条内部drift绝对峰13606ns，非内容A/V证明。
capture409139/drop0；输出83797RTP/SSRC29BBD16F/序号异常0，TS98273804B，已知文本footer跳过1。ffprobe视频4072/音频5845、DTS非递增均0，完整AVdecodeexit0/日志0B。第1800帧实际查看三格内容+右下黑场；这是离线wire证据，不代替失败的现场截图。
未完整drain：encoded45520/45519、AAC关闭2帧、sender backlog1包/228B、mux pending188B。旧r143间歇超期仍未关闭。
新增关联：音频built最大间隔104.0526ms（14420868099100..14420972151700，样本4075520→4076544），同cursor2773/4076544的process86.3934ms/buildVideo86.3211ms/canvas86.2322ms嵌套位于其中；canvas结束14420971922300后0.2294ms即下一audio built。定位到该调用内墙钟时间，尚不能区分GPU执行与OS抢占，不能推断旧150ms故障同因。下一步结合平台可用权威GPU/线程trace继续调查，禁止擅自增加deadline或扩外部FFmpeg修改范围。
线端分析实际命令：
```powershell
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-tshark.log'; exit $LASTEXITCODE
$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress

& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-wire-decode.log'; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v error -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-wire.ts' -vf 'select=eq(n\,1800)' -frames:v 1 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r152-wire-frame.png'; exit $LASTEXITCODE
```

双独立审查者均读取原始终态日志并实际查看wire图，明确有限PASS，完整验收FAIL。归档后20件本轮临时产物按清单删除，前缀扫描无残留，六个宿主PID均不存在；原120秒源124427809B及Release保留。本轮无RKMPP测试。

## r153 CUDA诊断调查与失败轮（2026-10-10）

沿用r148 Release，无生产修改。r152的86.2322ms仅canvas调用墙钟。源码MediaVideoCanvasProducer::compose先复制黑底再逐tile复制；CudaCanvasAdapter::transfer每次同步source stream、提交逐plane cuMemcpy2DAsync、同步destination stream。不能未测量就把全部等待归因GPU计算或删除同步（它同时保证源/目标生命周期）。
官方依据：[CUDA Stream API](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__STREAM.html)说明同步等待stream任务完成；[Nsight Systems](https://docs.nvidia.com/nsight-systems/UserGuide/index.html)提供CUDA与线程调度采集；[Microsoft CPU Analysis](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis)区分CPU执行/等待分析。已核对本机Nsight2024.6.2.225及profile帮助，不凭新版本文档假设本机参数。WindowsToolkit也已安装。

实际诊断PID：VLC34284、源35772/33012/33672、Nsight25064、CLI35664。--kill=false，无媒体max-duration。Nsight因当前令牌非管理员明确关闭CPU context switch和CPU sampling，CUDA采集可用。工具require_escalated不等于Windows管理员令牌。
CLI约启动60ms后进入stop，首个可见Cancelled在stop.begin之后；最终报graph payload credit waiters were cancelled，encoded0、workerErrors3/errors4，payload329/329归零。Nsight报告仅准备/短暂启动阶段CUDA，不能解释r152稳定运行canvas延迟。原CLI stdout/stderr存于SQLite ProcessStreams关联StringIds18/20，主线程提取并查阅；控制台只显示profiler统计，不能据此说没有CLI日志。
同一session5047取得三源EXIT0，profilerEXIT1。源均完整120秒；CLI进程通过Get-Process取得的ExitCode空，未独立取得CLI退出码，不能冒称CLI1。VLC录制0B、无输出/内容AV/现场画面证据。VLC RC quit后精确PID核验清理。

独立源码调查确认：MediaRealtimeRuntimeCompletion::complete先runtime.stop再优先返回worker primaryFailure，可能覆盖此前waitStatus；makeCompletedRunOutcome也可能优先completionPrimaryFailure。r153日志stop.begin先于Cancelled支持它是次生错误，原wait failure尚无日志证据。stdin EOF只停止控制输入，不停止媒体；不得归因EOF或Nsight主动发信号。acceptanceCollector.sample在notifyProgress之前可返回失败，当前cpuSamples0只是调查候选，不是已证实根因。
下一步先修正首错保留/观测链，重新取得原始退出原因，再继续CUDA/线程定位；不得更改媒体deadline或跳过采样错误以让测试通过。完整目标仍75/FAIL，本轮诊断FAIL。

实际命令（宿主直接执行，未写入测试脚本）：
```powershell
$env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-temp'; $env:TMP=$env:TEMP; [IO.Directory]::CreateDirectory($env:TEMP) | Out-Null
$ErrorActionPreference='Stop'
$r153Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r153- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r153Vlc.Id
$r153Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r153Source0.Id
$r153Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r153Source1.Id
$r153Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r153Source2.Id
$r153Profiler=Start-Process -FilePath 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' -ArgumentList 'profile --kill=false --trace=cuda --sample=process-tree --cpuctxsw=process-tree --stats=true --output=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-profile D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r153 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-cli-error.log' -PassThru; Write-Output 'PROFILER_PID='; Write-Output $r153Profiler.Id
$r153Child=$null; while(-not $r153Profiler.HasExited -and -not $r153Child){$r153Child=Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object {$_.CommandLine -like '*composition-wiring-r153*'}; if(-not $r153Child){$r153Profiler.WaitForExit(1000) | Out-Null}}; if(-not $r153Child){Write-Output "PROFILER_EXIT=$($r153Profiler.ExitCode)"; exit 8}; $r153Cli=Get-Process -Id $r153Child.ProcessId; Write-Output "CLI_PID=$($r153Cli.Id)"
$r153Clock=[Diagnostics.Stopwatch]::StartNew(); $r153LastTime=0.0; $r153LastCpu=0.0; $r153Cores=[Environment]::ProcessorCount
while(-not $r153Cli.WaitForExit(1000)) {$r153Cli.Refresh(); $r153Time=$r153Clock.Elapsed.TotalSeconds; $r153Cpu=$r153Cli.TotalProcessorTime.TotalSeconds; $r153Usage=100*($r153Cpu-$r153LastCpu)/(($r153Time-$r153LastTime)*$r153Cores); $r153Line="seconds=$r153Time cpu=$r153Usage ws=$($r153Cli.WorkingSet64) private=$($r153Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r153-monitor.log' -Value $r153Line; $r153LastTime=$r153Time; $r153LastCpu=$r153Cpu }
Write-Output "CLI_EXIT=$($r153Cli.ExitCode)"
$r153Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r153Source0.ExitCode)"
$r153Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r153Source1.ExitCode)"
$r153Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r153Source2.ExitCode)"
$r153Profiler.WaitForExit(); Write-Output "PROFILER_EXIT=$($r153Profiler.ExitCode)"; exit $r153Profiler.ExitCode
```

双独立审查均实际读取提取日志：失败解释PASS、运行FAIL。15件顶层产物及temp内2件已列明lock文件、空目录逐项清理，前缀扫描无残留；原源/Release保留。未修改生产代码。

## r154 首错保留修复设计（2026-10-10）

按官方Go errgroup先记录失败、取消并等待所有任务、返回已记录失败的顺序（https://go.dev/src/cmd/vendor/golang.org/x/sync/errgroup/errgroup.go），在现有C++生命周期映射为：完整执行runtime.stop清理，但已有waitStatus失败优先保留；没有已有失败才采用worker/stop错误。controller投影同样优先保留waitOutcome错误及RuntimeProgress阶段，完成期仅在返回错误与worker记录一致时标WorkerFailure。不新增线程/队列/公共参数，不改变媒体策略；二次worker错误仍在既有日志/metrics。共享Windows/RKMPP控制器均受影响，先Windows真实Nsight失败场景及无分析器同规格链路验证。不能宣称与Go调度器等价，只复用首错保留与等待清理顺序。

r154修改完成：两个生产文件首错传播顺序已冻结，双独立源码PASS；Release全量configure/build0，699构建项，realtime exe2026-10-10 12:55:27/5801472B。
复验PID VLC36500/source25140、26584、37200/profiler14620/CLI31224；同一session33742本轮提前持有CLI Handle，实得CLI1/三源0/profiler1。三源各完整120秒3600帧；profiler结束未杀CLI（kill=false）。最终原始stderr明确IoFailure: Thread32Next failed，stop.begin后的worker Cancelled不再覆盖它，双独立真实证据有限PASS。Nsight中的stdout捕获截断于node_abort，不能宣称最终payload归零；无有效输出、录制0B，不作为合屏成功。
sampler在Thread32Next FALSE后立即GetLastError并保存，再CloseHandle；依据[Microsoft Thread32Next](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-thread32next)仅ERROR_NO_MORE_FILES作为正常结束。当前最终消息未明确打印原生码，不据此断言具体profiler/OS根因；禁止忽略未知错误。CPU/上下文trace仍因非管理员关闭。本轮VLC RC quit，精确六PID均已不存在。
实际复验命令：
```powershell
$env:TEMP='D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-temp'; $env:TMP=$env:TEMP; [IO.Directory]::CreateDirectory($env:TEMP) | Out-Null
$ErrorActionPreference='Stop'
$r154Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r154- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r154Vlc.Id
$r154Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r154Source0.Id
$r154Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r154Source1.Id
$r154Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r154Source2.Id
$r154Profiler=Start-Process -FilePath 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' -ArgumentList 'profile --kill=false --trace=cuda --sample=process-tree --cpuctxsw=process-tree --stats=true --output=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-profile D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-wiring-r154 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-cli-error.log' -PassThru; Write-Output 'PROFILER_PID='; Write-Output $r154Profiler.Id
$r154Child=$null; while(-not $r154Profiler.HasExited -and -not $r154Child){$r154Child=Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object {$_.CommandLine -like '*composition-wiring-r154*'}; if(-not $r154Child){$r154Profiler.WaitForExit(1000) | Out-Null}}; if(-not $r154Child){Write-Output "PROFILER_EXIT=$($r154Profiler.ExitCode)"; exit 8}; $r154Cli=Get-Process -Id $r154Child.ProcessId; $r154Cli.Handle | Out-Null; Write-Output "CLI_PID=$($r154Cli.Id)"
$r154Clock=[Diagnostics.Stopwatch]::StartNew(); $r154LastTime=0.0; $r154LastCpu=0.0; $r154Cores=[Environment]::ProcessorCount
while(-not $r154Cli.WaitForExit(1000)) {$r154Cli.Refresh(); $r154Time=$r154Clock.Elapsed.TotalSeconds; $r154Cpu=$r154Cli.TotalProcessorTime.TotalSeconds; $r154Usage=100*($r154Cpu-$r154LastCpu)/(($r154Time-$r154LastTime)*$r154Cores); $r154Line="seconds=$r154Time cpu=$r154Usage ws=$($r154Cli.WorkingSet64) private=$($r154Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r154-monitor.log' -Value $r154Line; $r154LastTime=$r154Time; $r154LastCpu=$r154Cpu }
Write-Output "CLI_EXIT=$($r154Cli.ExitCode)"
$r154Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r154Source0.ExitCode)"
$r154Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r154Source1.ExitCode)"
$r154Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r154Source2.ExitCode)"
$r154Profiler.WaitForExit(); Write-Output "PROFILER_EXIT=$($r154Profiler.ExitCode)"; exit $r154Profiler.ExitCode
```

r154清理完成：15件顶层产物及2件临时lock和空目录逐项删除，原源及新Release保留。r155开始无分析器同规格三源CLI回归（PID2672，VLC36804/capture7424/源8080、2180、32424）；现场截图API仍返回句柄无效，暂不判现场画面通过。

r155实际命令（无分析器、完整原规格回归）：
```powershell
$ErrorActionPreference='Stop'
$r155Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r155- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r155Vlc.Id
$r155Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r155Capture.Id
$r155Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r155Source0.Id
$r155Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r155Source1.Id
$r155Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r155Source2.Id
$r155Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r155 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r155Cli.Id
$r155Clock=[Diagnostics.Stopwatch]::StartNew(); $r155LastTime=0.0; $r155LastCpu=0.0; $r155Cores=[Environment]::ProcessorCount
while(-not $r155Cli.WaitForExit(1000)) {$r155Cli.Refresh(); $r155Time=$r155Clock.Elapsed.TotalSeconds; $r155Cpu=$r155Cli.TotalProcessorTime.TotalSeconds; $r155Usage=100*($r155Cpu-$r155LastCpu)/(($r155Time-$r155LastTime)*$r155Cores); $r155Line="seconds=$r155Time cpu=$r155Usage ws=$($r155Cli.WorkingSet64) private=$($r155Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r155-monitor.log' -Value $r155Line; $r155LastTime=$r155Time; $r155LastCpu=$r155Cpu }
Write-Output "CLI_EXIT=$($r155Cli.ExitCode)"
$r155Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r155Source0.ExitCode)"
$r155Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r155Source1.ExitCode)"
$r155Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r155Source2.ExitCode)"
exit $r155Cli.ExitCode
```

r155结果FAIL：同一session32737实得CLI_EXIT1、三源EXIT0（各完整120秒3600帧），未发CLI停止信号。约64.8秒失败，原错完整保留：
```text
InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=64756080925 dispatch_ns=64856080925 available_ns=64863020988 last_committed_kind=maintenance service_start_ns=64863020988 actual_now_ns=64884347000 remaining_ns=-6940063 payload_bytes=564 wire_bytes=576
```
sender45849=45836RTP+13RTCP，deadline/pressure/partial/ambiguous0、backlog0；mux pending564B、AAC关闭2帧、encoded21549/21548，不能宣称完整drain。最终queued/workers/payload bytes/objects0，reservations/releases54453/54453，workerErrors1/errors1，压力17101。CPU后台63样本均2.357153792%、峰3.525808367%、WS峰247967744B/private峰862273536B；内部201样本CPU均2.346079%、峰4.470588%、WS峰247517184B。64条内部drift绝对峰118ns不是内容AV。
capture371178/drop0；停止采集指令身份核验失败，因为该PID已不存在（后续CIM确认），未发送信号或终止复用PID；本轮capture设有370秒guard，未单独取得exitcode，不冒称Ctrl-C成功。VLC RC quit后六PID均不存在；现场截图API失败，本轮未做完整线端解码/现场播放成功认定。

关键同次关联：video_cursor1928/audio_cursor2834432的process144.7867ms、build_video144.7473ms、canvas144.6856ms嵌套；aggregate_built间隔144.9279ms（15695023207900..15695168135800）完全包含canvas（15695023266500..15695167952100），返回后0.1837ms生成下一音频块。encoder_packet/scheduler_input最大间隔结束于2832384..2833408，失败AU canonical dispatch64226394557ns对应2832384/44100s，scheduler取得时晚27.719075ms。可把本次真实期限失败与canvas调用内墙钟停顿关联，不等于已证明GPU执行或OS调度根因。
本轮验证首错传播不会吞原worker失败；正常完整合屏回归FAIL，禁止调大预算或降低源规格来关门禁。下一步对canvas内source同步、plane提交、destination同步的有界诊断分解与线程CPU时间做官方API能力核验，以绕开当前Nsight注入阻塞而不改变媒体语义。

双审均实际读取原始日志，证据定位PASS/运行FAIL。时间口径核对：head_arrived_ns64883800000比dispatch64856080925晚27.719075ms；late_audio_head的now64883800900晚27.719975ms，两者相差0.9us并非矛盾。AU2767从scheduled_created15695169120900到session_ready15695169336700为0.2158ms。

r155共15件本轮产物按清单清理，前缀扫描无残留，六PID不存在；原源与Release保留。

## r156 CUDA调用阶段诊断设计（2026-10-10）

依据Microsoft GetThreadTimes官方文档（https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes）与CUDA Stream API，复用MediaCurrentThreadCpuClock及MediaSteadyClock，对真实CUDA adapter五种调用阶段保留每阶段最长一次的固定记录：context_push/source_wait/plane_copy/destination_wait/context_pop。每条含同次wall/CPU、begin/end、操作种类与返回码；诊断关闭不采样，无新增线程/队列/帧所有权，析构后汇总且格式化失败不影响媒体。CPU计数失败标unavailable，时钟差不当GPU时间。保持CUDA调用/同步/失败返回原顺序；RGA没有这些CUDA API，不能伪造同阶段，只沿用共享aggregate墙钟和线程CPU组件；本轮只Windows运行，不修改RGA。先双审/全量Release再原规格realtime，诊断开销可能扰动现场，有限定位不等修复验收。

r156双独立源码审查PASS。保留解释边界：wall包含CPU采样开销，CPU量化且0不等没有执行，plane_copy为单plane极值；各阶段最大记录可能不属于同次transfer，必须按steady时间区间关联，不得相加。没有adapterID时用运行时段及外层canvas区间限定归属。

r156双审后Release全量session61312 configure/build0，realtime exe13:30:12/5805568B。实流PID VLC34512/capture33240/source36368、34828、31436/CLI37820。实际命令：
```powershell
$ErrorActionPreference='Stop'
$r156Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r156- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r156Vlc.Id
$r156Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r156Capture.Id
$r156Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r156Source0.Id
$r156Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r156Source1.Id
$r156Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r156Source2.Id
$r156Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r156 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r156Cli.Id
$r156Clock=[Diagnostics.Stopwatch]::StartNew(); $r156LastTime=0.0; $r156LastCpu=0.0; $r156Cores=[Environment]::ProcessorCount
while(-not $r156Cli.WaitForExit(1000)) {$r156Cli.Refresh(); $r156Time=$r156Clock.Elapsed.TotalSeconds; $r156Cpu=$r156Cli.TotalProcessorTime.TotalSeconds; $r156Usage=100*($r156Cpu-$r156LastCpu)/(($r156Time-$r156LastTime)*$r156Cores); $r156Line="seconds=$r156Time cpu=$r156Usage ws=$($r156Cli.WorkingSet64) private=$($r156Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-monitor.log' -Value $r156Line; $r156LastTime=$r156Time; $r156LastCpu=$r156Cpu }
Write-Output "CLI_EXIT=$($r156Cli.ExitCode)"
$r156Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r156Source0.ExitCode)"
$r156Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r156Source1.ExitCode)"
$r156Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r156Source2.ExitCode)"
exit $r156Cli.ExitCode
```

r156最终：session42937 CLI_EXIT0/三源EXIT0（各120秒3600帧），源尽后继续黑/静音，精确PID37820 Ctrl-C caller_stop；capture33240 Ctrl-C、VLC34512 RC quit，不冒称二者独立exitcode。最终118514/118514 payload预约/释放，bytes/objects/queued/workers0，errors/workerErrors0，压力30325。encoded71106/71105，AAC2帧、senderbacklog1包228B、muxpending188B，仍非完整drain。
后台211样本CPU均1.710919091%、峰4.077269262%、WS峰271941632B/private峰878116864B；包含较长失源后黑/静音段，不与更短前轮均值作性能改善比较。内部671样本CPU均1.735217%、峰7.259953%；113条drift峰118ns仅内部时钟。capture417873/drop0；输出92531RTP序号连续SSRC47022B73，TS100481676B，工具footer跳过1。ffprobe6361video/9131audio DTS非递增均0，完整AVdecode0且日志0B。未新增现场画面证据。
双独立原始日志复核有限PASS：canvas70.1879ms(17406764969800..17406835157700)包含destination_wait41.7732ms(17406790037200..17406831810400)，同cursor1986/2919424位于builtgap82.3309ms(17406752942400..17406835273300)。source_wait最大36.6752ms在17406565394200..17406602069400，属另一调用，不能相加。context_push0.1737ms/plane_copy0.8707ms/context_pop0.5489ms各为不同单次极值；全部native0、threadCPU0，量化观测不证明GPU耗时或排除主机抢占。本轮未复现r155144ms超期，不能记修复；仅定位本轮长canvas区间内存在目标stream同步等待。后续按同次transfer完整阶段记录与CUDA stream依赖/完成契约调查异步方案，不能直接删同步释放帧。
线端实际核验命令：
```powershell
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-tshark.log'; exit $LASTEXITCODE
$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress

& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r156-wire-decode.log'; exit $LASTEXITCODE
```

r156记录后19件本轮产物逐项清理，前缀扫描无残留，六PID不存在；原120秒源和Release保留。

## r157：桌面恢复后现场三格有限PASS，发送期限运行FAIL（2026-10-10）

沿用r156 Release（13:30:12，5805568B），本轮未修改生产代码。输入三路H264 1280×720/30约8Mbps，其中一路AAC；输出HEVC 1280×720/30 CBR8Mbps、AAC目标192kbps、MPEG-TS/RTP。三源均使用原120秒连续源，无循环或降规格。

VLC PID36344、dumpcap12188、AV源20588、视频源34576/10044、CLI32460。宿主session97041确认CLI自然exit1，三源各3600帧/120秒exit0。未发送CLI停止信号。错误完整保留为 `IoFailure: scheduled datagram submit exceeded its original deadline`，发生于源活动阶段，不能完成本轮失源黑场门禁。

两张无遮挡VLC现场截图已由主智能体及两名独立审查实际查看：三格内容、右下黑格；两个时刻内容变化。首张截图被其他窗口遮挡，不用于完整布局判定。现场有限PASS，不证明持续无卡顿、音质或内容A/V同步。

同游标video2155/audio3168256：process111.6515ms、canvas111.5223ms（18028671469500..18028782991800）；audio built间隔112.8455ms（18028670309600..18028783155100）包含该canvas；source_wait65.5392ms（18028716941300..18028782480500）位于canvas内，native0/thread_cpu0仅量化记录。destination最大57.1631ms属于另一次调用，不得相加。失败datagram缺精确媒体身份及deadline关联，不能据此断定canvas/GPU是sender超期唯一原因。

sender deadline_misses1、would_block0，51258提交（51241 RTP+17 RTCP），最后提交sequence51258，剩余5包/6404B，delivery_evidence=not_proven；AAC关闭仍2帧。最终payload预约/释放60887/60887、0B/0对象，queued/workers0，errors/workerErrors2，pressure19125；资源回收不是完整drain。抓包376583/drop0，本失败轮未做完整线端解码。后台70个CPU样本均值2.463574893%、峰3.821524883%，WS峰248610816B、private峰859336704B；内部223样本均值2.453462%、峰5.567929%。72条内部drift绝对值最大111ns，不是内容A/V同步证据。

两名独立原始证据复核均判现场有限PASS/运行FAIL。整体75/FAIL；继续定位发送期限与canvas串行等待，保留尾部排空、内容AV、完整矩阵及Windows后RKMPP门禁。本轮不提交推送合并部署。

### 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$r157Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r157- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r157Vlc.Id
$r157Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r157Capture.Id
$r157Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r157Source0.Id
$r157Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r157Source1.Id
$r157Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r157Source2.Id
$r157Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r157 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r157Cli.Id
$r157Clock=[Diagnostics.Stopwatch]::StartNew(); $r157LastTime=0.0; $r157LastCpu=0.0; $r157Cores=[Environment]::ProcessorCount
while(-not $r157Cli.WaitForExit(1000)) {$r157Cli.Refresh(); $r157Time=$r157Clock.Elapsed.TotalSeconds; $r157Cpu=$r157Cli.TotalProcessorTime.TotalSeconds; $r157Usage=100*($r157Cpu-$r157LastCpu)/(($r157Time-$r157LastTime)*$r157Cores); $r157Line="seconds=$r157Time cpu=$r157Usage ws=$($r157Cli.WorkingSet64) private=$($r157Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-monitor.log' -Value $r157Line; $r157LastTime=$r157Time; $r157LastCpu=$r157Cpu }
Write-Output "CLI_EXIT=$($r157Cli.ExitCode)"
$r157Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r157Source0.ExitCode)"
$r157Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r157Source1.ExitCode)"
$r157Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r157Source2.ExitCode)"
exit $r157Cli.ExitCode
```

### 现场截图及结束操作

```powershell
$r157Vlc=Get-CimInstance Win32_Process -Filter 'ProcessId=36344'; if($r157Vlc.Name -ne 'vlc.exe' -or $r157Vlc.CommandLine -notlike '*composition-wiring-r157*'){throw 'Identity mismatch'}
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public static class R157Dpi { [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr value); [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int height,uint flags); }';
[R157Dpi]::SetThreadDpiAwarenessContext([IntPtr](-4)) | Out-Null;
$r157Window=(Get-Process -Id 36344).MainWindowHandle;
[R157Dpi]::SetWindowPos($r157Window,[IntPtr](-1),20,20,1000,700,0) | Out-Null;
& 'C:/Users/ROG/.codex/skills/screenshot/scripts/take_screenshot.ps1' -WindowHandle $r157Window -Path 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r157-live-clear.png'
```

第二张将文件名live-clear改为live-later。抓包以核验PID/命令行及console成员后的CtrlC结束；VLC核验身份后通过127.0.0.1:63020 RC quit结束。未单独取得两者退出码，不冒称自然退出。

清理清单（18件，删除前归档）：
composition-wiring-r157-capture-stdout.log
composition-wiring-r157-capture.log
composition-wiring-r157-cli-error.log
composition-wiring-r157-cli.log
composition-wiring-r157-live-clear.png
composition-wiring-r157-live-later.png
composition-wiring-r157-live.png
composition-wiring-r157-monitor.log
composition-wiring-r157-received.ts
composition-wiring-r157-source0-stdout.log
composition-wiring-r157-source0.log
composition-wiring-r157-source1-stdout.log
composition-wiring-r157-source1.log
composition-wiring-r157-source2-stdout.log
composition-wiring-r157-source2.log
composition-wiring-r157-vlc.log
composition-wiring-r157-wire.pcapng
composition-wiring-r157.sdp

清理核验：18件全部删除，r157残留0；六PID均不存在，原源124427809B保留。

## r160 实流执行命令（进行中）

双owner最终源码两独立复审PASS；Release全量configure/build0，session96110。仅realtime实流，原三路120秒源，无循环、降规格或CLI时长限制。精确PID：VLC39972、dumpcap39052、AV源38752、V源20588/38836、CLI36744；启动及监控session71923。现场首张无遮挡截图三格视频与右下黑格，尚非完整验收。

```powershell
$ErrorActionPreference='Stop'
$r160Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r160- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r160Vlc.Id
$r160Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r160Capture.Id
$r160Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r160Source0.Id
$r160Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r160Source1.Id
$r160Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r160Source2.Id
$r160Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r160 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r160Cli.Id
$r160Clock=[Diagnostics.Stopwatch]::StartNew(); $r160LastTime=0.0; $r160LastCpu=0.0; $r160Cores=[Environment]::ProcessorCount
while(-not $r160Cli.WaitForExit(1000)) {$r160Cli.Refresh(); $r160Time=$r160Clock.Elapsed.TotalSeconds; $r160Cpu=$r160Cli.TotalProcessorTime.TotalSeconds; $r160Usage=100*($r160Cpu-$r160LastCpu)/(($r160Time-$r160LastTime)*$r160Cores); $r160Line="seconds=$r160Time cpu=$r160Usage ws=$($r160Cli.WorkingSet64) private=$($r160Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-monitor.log' -Value $r160Line; $r160LastTime=$r160Time; $r160LastCpu=$r160Cpu }
Write-Output "CLI_EXIT=$($r160Cli.ExitCode)"
$r160Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r160Source0.ExitCode)"
$r160Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r160Source1.ExitCode)"
$r160Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r160Source2.ExitCode)"
exit $r160Cli.ExitCode
```

### r160 所选AV源重入命令

首组三源自然结束，CLI按continuous场景持续黑屏后，重启所选AV源，PID27284、session40943；仍完整120秒，不循环。VLC现场左下格恢复，其余格黑色。

```powershell
$r160Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-reentry.log' -PassThru; Write-Output 'Reentry_PID='; Write-Output $r160Reentry.Id
$r160Reentry.WaitForExit(); Write-Output "REENTRY_EXIT=$($r160Reentry.ExitCode)"; exit $r160Reentry.ExitCode
```

### r160 结果与证据边界

- 输入：三路H.264 1280×720/30fps、约8Mbps RTP，其中所选源含AAC44.1kHz双声道；原三源及所选AV重入均完整120秒/3600帧，四进程exit0。输出：HEVC1280×720/30fps CBR target8000kbps＋AAC target192kbps → MPEG-TS/RTP，50Mbps出口/100ms预算未放宽。
- CLI36744在源结束后持续输出黑/静音，所选源27284重入generation2恢复左下格，其余黑；四张现场截图实际查看并独立复核有限PASS。再次失源后调用方正常CtrlC，session71923 CLI_EXIT0及三原源0，session40943重入0。不是CLI自然EOF。
- 最终queued/workers/errors/dropped均0；payload184686/184686、bytes/objects0，峰18798832B/668对象；pressure47579，不称零背压。CPU外部303样本/311.3941秒，平均1.97907%/峰3.61870%，WS峰267919360B/private峰877228032B；内部959样本平均2.000738%/峰5.982906%，WS峰267497472B。230条内部drift最大绝对15938ns，非内容AV证明。
- 视频canvas最大100.767ms（steady21028906941200..21029007708200，cursor5867），音频built最大26.6717ms/committed26.6949ms，双独立复核支持本轮聚合执行隔离有限PASS。scheduler_input最大90.6241ms（21028922434500..21029013058600）与canvas区间重叠，端到端仍有等待，不把重叠断言唯一因果。
- sender deadline/would_block均0，submitted176588=RTP176512＋RTCP76；delivery_evidence仍not_proven。停止尾部1包228B、mux188B、AAC2帧、encoded计数差1，非完整drain；sender stage=failed为停止终态，不能隐藏。
- dumpcap613804/drop0；线端RTP176512，序号异常0，TS204250532B；ffprobe视频9189包/音频13190包，DTS不递增各0；完整AV解码exit0且日志0B。VLC存在playback/picture late和up-sampling告警，退出日志read error/SetThumbNailClip，不能称全程播放无抖动。FFmpeg输入range unknown→tv告警仍存在。
- 源码双独立最终PASS、实流停止/资源/现场/聚合隔离有限双审PASS；线端补充已交两者最终复核。完整目标仍75/FAIL：端到端deadline可靠性、尾部排空、内容AV、2～4源完整矩阵与Windows后RKMPP仍开放。没有改外部FFmpeg，没有提交/推送/部署。

线端审计实际命令（实流结束后执行，不增实时监控FFmpeg）：

```powershell
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-tshark.log'; exit $LASTEXITCODE
$ErrorActionPreference='Stop'; $count=0L; $anomalies=0L; $footer=0; $last=@{}; $stream=[IO.File]::Create('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-wire.ts'); try { foreach($line in [IO.File]::ReadLines('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-output-payloads.txt')) { if($line -eq 'No matching RTP/H.264 payload dumped.') {$footer++; continue}; if(!$line){continue}; $b=[Convert]::FromHexString($line.Replace(':','')); if($b.Length -lt 12 -or ($b[0]-shr 6)-ne 2){throw 'Invalid RTP'}; $seq=[int]$b[2]*256+$b[3]; $ssrc=[Convert]::ToHexString($b[8..11]); $off=12+4*($b[0]-band 15); if($b[0]-band 16){$off+=4+4*([int]$b[$off+2]*256+$b[$off+3])}; $end=$b.Length; if($b[0]-band 32){$end-=$b[-1]}; if($off -gt $end -or ($end-$off)%188 -ne 0){throw 'Invalid TS payload'}; if($last.ContainsKey($ssrc) -and $seq -ne (($last[$ssrc]+1)%65536)){$anomalies++}; $last[$ssrc]=$seq; $stream.Write($b,$off,$end-$off); $count++ } } finally {$stream.Dispose()}; [pscustomobject]@{Packets=$count;SequenceAnomalies=$anomalies;TextFooters=$footer;SSRC=($last.Keys -join ',');Bytes=(Get-Item 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-wire.ts').Length} | ConvertTo-Json

$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress

& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r160-wire-decode.log'; exit $LASTEXITCODE
```

### r160清理与最终复核

两独立智能体最终有限PASS，整体75/FAIL；VLC另有音频buffer丢弃告警，播放零抖动未证明。以下本轮逐项清单在上述命令、指标、失败边界记录后删除，保留指定120秒源与Release正式构建产物：

- composition-r160-build-final.log
- composition-r160-build-retry.log
- composition-r160-build.log
- composition-wiring-r160-capture-stdout.log
- composition-wiring-r160-capture.log
- composition-wiring-r160-cli-error.log
- composition-wiring-r160-cli.log
- composition-wiring-r160-live-clear.png
- composition-wiring-r160-live-ended.png
- composition-wiring-r160-live-reentry.png
- composition-wiring-r160-live-second.png
- composition-wiring-r160-monitor.log
- composition-wiring-r160-output-payloads.txt
- composition-wiring-r160-received.ts
- composition-wiring-r160-reentry-stdout.log
- composition-wiring-r160-reentry.log
- composition-wiring-r160-source0-stdout.log
- composition-wiring-r160-source0.log
- composition-wiring-r160-source1-stdout.log
- composition-wiring-r160-source1.log
- composition-wiring-r160-source2-stdout.log
- composition-wiring-r160-source2.log
- composition-wiring-r160-tshark.log
- composition-wiring-r160-vlc.log
- composition-wiring-r160-wire-decode.log
- composition-wiring-r160-wire.pcapng
- composition-wiring-r160-wire.ts
- composition-wiring-r160.sdp

清理实测：上述28件全部删除，本轮前缀文件0残留；七个精确PID及带本轮标识的媒体进程0残留。指定源124427809B保留，最终realtime Release5838336B/14:29:05保留；git diff --check通过。本轮未接入RKMPP，无远程产物。

## r162：有序输出排空实流（进行中）

最终Release全量configure/build0（session93204，realtime5846016B/15:05:31），两独立源码复审PASS。原规格启动session31042：VLC34044/capture40088/AV源27268/V源38656与38096/CLI37956。仅realtime，连续120秒源、HEVC720p30 CBR8000kbps＋AAC192kbps，未降规格。

```powershell
$ErrorActionPreference='Stop'
$r162Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r162- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r162Vlc.Id
$r162Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r162Capture.Id
$r162Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r162Source0.Id
$r162Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r162Source1.Id
$r162Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r162Source2.Id
$r162Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r162 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r162Cli.Id
$r162Clock=[Diagnostics.Stopwatch]::StartNew(); $r162LastTime=0.0; $r162LastCpu=0.0; $r162Cores=[Environment]::ProcessorCount
while(-not $r162Cli.WaitForExit(1000)) {$r162Cli.Refresh(); $r162Time=$r162Clock.Elapsed.TotalSeconds; $r162Cpu=$r162Cli.TotalProcessorTime.TotalSeconds; $r162Usage=100*($r162Cpu-$r162LastCpu)/(($r162Time-$r162LastTime)*$r162Cores); $r162Line="seconds=$r162Time cpu=$r162Usage ws=$($r162Cli.WorkingSet64) private=$($r162Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-monitor.log' -Value $r162Line; $r162LastTime=$r162Time; $r162LastCpu=$r162Cpu }
Write-Output "CLI_EXIT=$($r162Cli.ExitCode)"
$r162Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r162Source0.ExitCode)"
$r162Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r162Source1.ExitCode)"
$r162Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r162Source2.ExitCode)"
exit $r162Cli.ExitCode
```

### r162所选AV源重入

PID38420，session46494，原规格完整120秒；generation2已locked。现场截图CopyFromScreen返回句柄无效，OpenInputDesktop实际返回Screen-saver；本轮现场项未验证，不归因合屏失败。

```powershell
$r162Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r162-reentry.log' -PassThru; Write-Output 'Reentry_PID='; Write-Output $r162Reentry.Id
$r162Reentry.WaitForExit(); Write-Output "REENTRY_EXIT=$($r162Reentry.ExitCode)"; exit $r162Reentry.ExitCode
```

## r162 最终结果：FAIL（2026-10-10）

最终全量 Release 重建退出 0；真实 CLI 退出 1。三路原源及所选音频源重入均使用完整 120 秒源并自然退出 0。正常停止请求触发 continuous_output_drain，但节点 77 报原始错误 `Protocol terminal requires its exact active generation`；不得认定排空通过。两 aggregate 使用无 generation 的 makeEof，终端经编码器、scheduler、mux 原样传递，与 materializer 的精确输出代合同冲突。修复应在生产者使用已激活输出 epoch 的 generation，不使用重入源 generation，也不放宽下游校验。

最终 payload 180113/180113、占用 0，峰值 18896056 字节/669 对象；workerErrors=1。sender 提交 174242（RTP 174165、RTCP 77），deadline/wouldblock=0，但剩余 1 包/228 字节，故不是尾部排空成功。抓包 611457 包、丢包 0；未完成本轮 wire 全量分析。AAC 未再报关闭残留 2 帧，仅为局部证据。截图因 Screen-saver 桌面失败，无本轮现场画面 PASS；用户随后已恢复桌面。

外部监控 283 次：CPU 平均 2.2632%、峰值 4.8209%；WS 峰值 267218944 字节，private 峰值 878125056 字节。

已核验本轮 PID 34044、40088、27268、38656、38096、37956、38420 均不存在。按本轮文件清单清理临时产物，保留连续源。整体合屏仍 75/FAIL，RKMPP 尚未开始。

## r163 实流命令（进行中）

冻结后全量 Release 重建退出 0。以下为实际启动及后台监控命令，结果待验。

```powershell
$ErrorActionPreference='Stop'
$r163Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r163- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r163Vlc.Id
$r163Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r163Capture.Id
$r163Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r163Source0.Id
$r163Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r163Source1.Id
$r163Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r163Source2.Id
$r163Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r163 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r163Cli.Id
$r163Clock=[Diagnostics.Stopwatch]::StartNew(); $r163LastTime=0.0; $r163LastCpu=0.0; $r163Cores=[Environment]::ProcessorCount
while(-not $r163Cli.WaitForExit(1000)) {$r163Cli.Refresh(); $r163Time=$r163Clock.Elapsed.TotalSeconds; $r163Cpu=$r163Cli.TotalProcessorTime.TotalSeconds; $r163Usage=100*($r163Cpu-$r163LastCpu)/(($r163Time-$r163LastTime)*$r163Cores); $r163Line="seconds=$r163Time cpu=$r163Usage ws=$($r163Cli.WorkingSet64) private=$($r163Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r163-monitor.log' -Value $r163Line; $r163LastTime=$r163Time; $r163LastCpu=$r163Cpu }
Write-Output "CLI_EXIT=$($r163Cli.ExitCode)"
$r163Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r163Source0.ExitCode)"
$r163Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r163Source1.ExitCode)"
$r163Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r163Source2.ExitCode)"
exit $r163Cli.ExitCode
```

## r163 最终结果：FAIL（2026-10-10）

冻结后全量 Release 成功，两位独立源码复审 PASS。真实 CLI PID 22432 在约 85 秒时退出 1，错误为 `protocol datagram exceeded its absolute emission deadline before materialization`，发生于正常运行而非 drain。原源 PID 35024/38156/39520 均完成 120 秒、3600 帧并退出 0；未启动重入，未验证正常停止排空。VLC PID 36480 截图成功但拍摄时 CLI 已退出，画面为三格内容加右下黑格的保留帧，不能作为现场连续播放 PASS。dumpcap PID 13940 已 CtrlC，VLC 已 RC quit。

视频 canvas 最长 136.1539 ms，与 scheduler 音频消费最大间隔 121.7653 ms 重叠；audio aggregate 最长事件间隔 31.9497 ms，canonicalizer 36.1992 ms。事实说明独立音频生产仍运行，下游音视频排序等待与视频延迟相关；尚不能据此宣布 canvas 内部根因已证明。不得调大预算或降低规格掩盖。sender finished/零 backlog 为错误终止中的局部事实，不代表正常 drain。AAC 仍报 2 帧残留，符合失败路径未 drain，不构成 generation 修复验证。

外部监控 83 次，CPU 平均 2.3305%、峰值 4.2321%；WS 峰值 267489280 字节。

详细诊断见 realtime-composition-output-drain.md。

## r164 真实链路计划命令（待最终重建后执行）

```powershell
$ErrorActionPreference='Stop'
$r164Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r164- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r164Vlc.Id
$r164Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -a duration:370 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r164Capture.Id
$r164Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r164Source0.Id
$r164Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r164Source1.Id
$r164Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r164Source2.Id
$r164Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r164 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r164Cli.Id
$r164Clock=[Diagnostics.Stopwatch]::StartNew(); $r164LastTime=0.0; $r164LastCpu=0.0; $r164Cores=[Environment]::ProcessorCount
while(-not $r164Cli.WaitForExit(1000)) {$r164Cli.Refresh(); $r164Time=$r164Clock.Elapsed.TotalSeconds; $r164Cpu=$r164Cli.TotalProcessorTime.TotalSeconds; $r164Usage=100*($r164Cpu-$r164LastCpu)/(($r164Time-$r164LastTime)*$r164Cores); $r164Line="seconds=$r164Time cpu=$r164Usage ws=$($r164Cli.WorkingSet64) private=$($r164Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-monitor.log' -Value $r164Line; $r164LastTime=$r164Time; $r164LastCpu=$r164Cpu }
Write-Output "CLI_EXIT=$($r164Cli.ExitCode)"
$r164Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r164Source0.ExitCode)"
$r164Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r164Source1.ExitCode)"
$r164Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r164Source2.ExitCode)"
exit $r164Cli.ExitCode
```

r164 启动命令已实际执行，最终构建退出0。三路原源自然结束后实际执行重入：
```powershell
$r164Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-reentry.log' -PassThru; Write-Output 'Reentry_PID='; Write-Output $r164Reentry.Id
$r164Reentry.WaitForExit(); Write-Output "REENTRY_EXIT=$($r164Reentry.ExitCode)"; exit $r164Reentry.ExitCode
```

## r164 实流结果：运行时排空有限 PASS，完整验收未通过

冻结后全量 Release 退出0。真实三源120秒与所选AV源完整120秒重入均自然退出0（PID37336/41792/23832/39560）。CLI40312全程无worker错误，全部源结束后持续黑场；经核验精确进程身份与console成员后发送CtrlC正常停止，退出0。15:48:11.009请求drain，15:48:11.263完成，约254ms。sender finished，186194包全部提交，backlog0，AAC无关闭残留帧；payload204113/204113归还，bytes/objects0。

VLC19248实际截图观察到三格内容、全源结束黑场、所选源重入左下恢复；截图时宿主CIM证实CLI仍运行。窗口截图与CLI计数是有限观测，不等于全部内容级AV同步证明。dumpcap13276在370秒上限自然结束，早于最终CLI停止，620063包/丢包0；wire仅覆盖该区间，182771输出RTP包、序号异常0，TS205396580字节；视频10933包/音频15695包均无DTS非递增，最终尾部wire证据缺失。

外部监控390次，CPU平均1.7619%/峰值4.0988%，WS峰值268587008字节。 原始相位绝对最大值20217ns，仅时钟telemetry，不当作内容AV结论。

最慢compose尝试69.6723ms，其中tile0 source synchronize63.239ms，CPU计时0需考虑计时量化；本轮不是r163的136ms失败复现。scheduler最长缺视频观测58.7074ms，结束dispatch余量46.398733ms；该记录与最慢compose并非同一次时间窗口，不可拼接因果。r163 deadline仍未解决。

mux最终pending_bytes188是陈旧诊断：MediaTsPendingEmission::materialize设置m_materialized后pendingBytes为0，但未刷新diagnostics；MediaTsMuxSession完成AU后reset，finish若仍pending会失败。应同步更新实际清空时的诊断，不修改媒体策略。最终PID40312已被系统复用为conhost.exe，不得按旧PID误杀；其余本轮媒体进程已不存在。


r164 已实际执行的停止及离线分析命令（抓包区间全量音视频解码退出0）：
```powershell
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R164CliConsole {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint ev, uint group);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList([Out] uint[] ids, uint size);
}
'@
$r164Target=40312
$r164Process=Get-CimInstance Win32_Process -Filter "ProcessId=$r164Target"
if($r164Process.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r164Process.CommandLine -notlike '*composition-wiring-r164*'){throw 'PID identity mismatch'}
[R164CliConsole]::FreeConsole() | Out-Null
if(-not [R164CliConsole]::AttachConsole($r164Target)){throw 'AttachConsole failed'}
try {
if(-not [R164CliConsole]::SetConsoleCtrlHandler([IntPtr]::Zero,$true)){throw 'Cannot ignore helper CtrlC'}
$r164Ids=[uint32[]]::new(32)
$r164Count=[R164CliConsole]::GetConsoleProcessList($r164Ids,32)
if($r164Count -ne 2 -or $r164Ids[0..($r164Count-1)] -notcontains $r164Target -or $r164Ids[0..($r164Count-1)] -notcontains $PID){throw 'Unexpected console members'}
if(-not [R164CliConsole]::GenerateConsoleCtrlEvent(0,0)){throw 'GenerateConsoleCtrlEvent failed'}
} finally { [R164CliConsole]::FreeConsole() | Out-Null }
Write-Output 'CTRL_C_EVENT delivered to verified target console'

& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-output-payloads.txt' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-tshark.log'; exit $LASTEXITCODE
$ErrorActionPreference='Stop'; $count=0L; $anomalies=0L; $footer=0; $last=@{}; $stream=[IO.File]::Create('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-wire.ts'); try { foreach($line in [IO.File]::ReadLines('D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-output-payloads.txt')) { if($line -eq 'No matching RTP/H.264 payload dumped.') {$footer++; continue}; if(!$line){continue}; $b=[Convert]::FromHexString($line.Replace(':','')); if($b.Length -lt 12 -or ($b[0]-shr 6)-ne 2){throw 'Invalid RTP'}; $seq=[int]$b[2]*256+$b[3]; $ssrc=[Convert]::ToHexString($b[8..11]); $off=12+4*($b[0]-band 15); if($b[0]-band 16){$off+=4+4*([int]$b[$off+2]*256+$b[$off+3])}; $end=$b.Length; if($b[0]-band 32){$end-=$b[-1]}; if($off -gt $end -or ($end-$off)%188 -ne 0){throw 'Invalid TS payload'}; if($last.ContainsKey($ssrc) -and $seq -ne (($last[$ssrc]+1)%65536)){$anomalies++}; $last[$ssrc]=$seq; $stream.Write($b,$off,$end-$off); $count++ } } finally {$stream.Dispose()}; [pscustomobject]@{Packets=$count;SequenceAnomalies=$anomalies;TextFooters=$footer;SSRC=($last.Keys -join ',');Bytes=(Get-Item 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-wire.ts').Length} | ConvertTo-Json

$ErrorActionPreference='Stop'; $last=@{}; $counts=@{}; $regressions=@{}; & 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-wire.ts' | ForEach-Object { $f=$_.Split(','); if($f.Length -ge 3 -and $f[0] -match '^\d+$' -and $f[2] -match '^-?\d+$'){$key=$f[0]; $dts=[long]$f[2]; if(!$counts.ContainsKey($key)){$counts[$key]=0; $regressions[$key]=0}; if($last.ContainsKey($key) -and $dts -le $last[$key]){$regressions[$key]++}; $last[$key]=$dts; $counts[$key]++}}; if($LASTEXITCODE -ne 0){throw 'ffprobe failed'}; [pscustomobject]@{Packets=$counts;NonIncreasingDts=$regressions} | ConvertTo-Json -Compress

& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r164-wire-decode.log'; exit $LASTEXITCODE
```

## r165 实流命令（待构建后执行）

```powershell
$ErrorActionPreference='Stop'
$r165Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r165- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r165Vlc.Id
$r165Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r165Capture.Id
$r165Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r165Source0.Id
$r165Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r165Source1.Id
$r165Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r165Source2.Id
$r165Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r165 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r165Cli.Id
$r165Clock=[Diagnostics.Stopwatch]::StartNew(); $r165LastTime=0.0; $r165LastCpu=0.0; $r165Cores=[Environment]::ProcessorCount
while(-not $r165Cli.WaitForExit(1000)) {$r165Cli.Refresh(); $r165Time=$r165Clock.Elapsed.TotalSeconds; $r165Cpu=$r165Cli.TotalProcessorTime.TotalSeconds; $r165Usage=100*($r165Cpu-$r165LastCpu)/(($r165Time-$r165LastTime)*$r165Cores); $r165Line="seconds=$r165Time cpu=$r165Usage ws=$($r165Cli.WorkingSet64) private=$($r165Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-monitor.log' -Value $r165Line; $r165LastTime=$r165Time; $r165LastCpu=$r165Cpu }
Write-Output "CLI_EXIT=$($r165Cli.ExitCode)"
$r165Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r165Source0.ExitCode)"
$r165Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r165Source1.ExitCode)"
$r165Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r165Source2.ExitCode)"
exit $r165Cli.ExitCode
```

r165最终构建退出0，启动及监控命令已实际执行；以下为源自然结束后重入的实际命令：
```powershell
$r165Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r165-reentry.log' -PassThru; Write-Output 'Reentry_PID='; Write-Output $r165Reentry.Id
$r165Reentry.WaitForExit(); Write-Output "REENTRY_EXIT=$($r165Reentry.ExitCode)"; exit $r165Reentry.ExitCode
```

## r165 实流结果：FAIL（重入音频候选样本边界）

冻结后双审PASS、Release全量构建0。原三源PID40380/24072/17412完整120秒自然0。所选AV重入PID41860进入generation2后约1秒，CLI37496在16:00:44.580报首错 `Aggregate audio candidates exceed their planner sample bound`，退出1；其他Cancelled为失败清理后果。重入源随后也完整120秒自然0。尝试CtrlC重入进程时身份检查因命令行不含日志前缀拒绝执行，没有误杀；随后检查其真实身份并观察到自然完成。不能判正常排空或重入通过。

VLC38492截图因桌面句柄无效失败；抓包29648在CLI结束后精确PID CtrlC，493755包、丢包0。本轮保留原错误，无需将失败流当成功再做解码验收。payload107740/107740回收归零，workerErrors1；canvas最慢15.4536ms，不支持将本次错误归因于canvas迟延。日志缺少候选数量、已有样本和新块样本的具体值，不能据此直接判planner容量应增大。

外部监控147次，CPU平均2.0205%、峰值3.4241%；WS峰值248795136字节。


## r166 实流计划命令（待构建后执行）

```powershell
$ErrorActionPreference='Stop'
$r166Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r166- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-received.ts}}" --sout-keep rtp://@127.0.0.1:62020' -PassThru; Write-Output 'Vlc_PID='; Write-Output $r166Vlc.Id
$r166Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-capture.log' -PassThru; Write-Output 'Capture_PID='; Write-Output $r166Capture.Id
$r166Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-source0.log' -PassThru; Write-Output 'Source0_PID='; Write-Output $r166Source0.Id
$r166Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-source1.log' -PassThru; Write-Output 'Source1_PID='; Write-Output $r166Source1.Id
$r166Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-source2.log' -PassThru; Write-Output 'Source2_PID='; Write-Output $r166Source2.Id
$r166Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r166 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-cli-error.log' -PassThru; Write-Output 'Cli_PID='; Write-Output $r166Cli.Id
$r166Clock=[Diagnostics.Stopwatch]::StartNew(); $r166LastTime=0.0; $r166LastCpu=0.0; $r166Cores=[Environment]::ProcessorCount
while(-not $r166Cli.WaitForExit(1000)) {$r166Cli.Refresh(); $r166Time=$r166Clock.Elapsed.TotalSeconds; $r166Cpu=$r166Cli.TotalProcessorTime.TotalSeconds; $r166Usage=100*($r166Cpu-$r166LastCpu)/(($r166Time-$r166LastTime)*$r166Cores); $r166Line="seconds=$r166Time cpu=$r166Usage ws=$($r166Cli.WorkingSet64) private=$($r166Cli.PrivateMemorySize64)"; Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-monitor.log' -Value $r166Line; $r166LastTime=$r166Time; $r166LastCpu=$r166Cpu }
Write-Output "CLI_EXIT=$($r166Cli.ExitCode)"
$r166Source0.WaitForExit(); Write-Output "SOURCE0_EXIT=$($r166Source0.ExitCode)"
$r166Source1.WaitForExit(); Write-Output "SOURCE1_EXIT=$($r166Source1.ExitCode)"
$r166Source2.WaitForExit(); Write-Output "SOURCE2_EXIT=$($r166Source2.ExitCode)"
exit $r166Cli.ExitCode
```

r166最终构建0；前述启动命令已实际执行。源自然结束后重入命令：
```powershell
$r166Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r166-reentry.log' -PassThru; Write-Output 'Reentry_PID='; Write-Output $r166Reentry.Id
$r166Reentry.WaitForExit(); Write-Output "REENTRY_EXIT=$($r166Reentry.ExitCode)"; exit $r166Reentry.ExitCode
```

## r182：共享准备存储预约额度（实施中）

调用方显式提供 preparation_storage_limit_bytes；CLI 对应 --preparation-storage-limit-bytes。必须为正且可由本机 size_t 表示，无默认值。本机会话按用户委托选用 2147483648 字节（2048 MiB），全部输入、输出与画布共享预约根。该值不是进程 RSS 或不透明设备内存上限；decoder/output 探测先分配后收费等缺口尚未关闭，不能宣称完整分配硬界成立。旧单源结构不变。
