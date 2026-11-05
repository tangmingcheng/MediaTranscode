# 合屏源显示时间修复：r393 后续实施边界

状态：调查完成，尚未实现或验收。生产基线 ed1a22ae；目标仍为 Windows 后 RKMPP 的完整多路合屏，不降低 r393 的四路异构规格。

## 已核验的根因与复用位置

- r393 的 HEVC RTP 时间戳回退被 uint32 差值转换为超大正间隔，继而产生错误的准备容量。详见 [r393 实测](realtime-composition-r393-validation.md)。[RFC 3550 §5.1](https://www.rfc-editor.org/rfc/rfc3550.html#section-5.1) 允许重排媒体的 RTP 时间戳不按发送次序单调增长；取绝对值或只收集正差仍不能证明固定帧间隔。
- 合屏入口 `MediaRealtimeCompositionInputPreparer.cpp` 当前强制 CanonicalIntervals。统一目标是 SourcePresentation：保留 AU 身份和原始 PTS，解码后形成 SourceDisplayPoint，由已有 aggregate 的独立输出时钟选帧。不得按 H264/HEVC 建立两条处理链。
- `MediaCompositionSourcePlanningKernel` 已有无 cadence 的准备容量、资源账本、所有权校验；`MediaAvContinuousAggregateVideo` 已有显示点选帧。优先复用，不能扩大经验容量规避错误。
- 独立只读审查 timing_path_review 判定：方向可复用，但直接切换入口不能通过 RKMPP；Windows 也尚无本轮完整运行证据。

## 本机与远程固定依赖证据

2026-11-05 通过 SSH 实读 `/home/tang/ffmpeg-rkmpp-close-ownership`，HEAD 为 `ab1e61adaa21ff129caa8e01a1198156044567ed`，工作树干净。未修改依赖。远程无 rg，改用 grep/sed 读取。

| 核验位置 | 事实与边界 |
| --- | --- |
| rkmppdec.c:1054、1118、1131、675 | 输入 PTS 转换后写入 MppPacket，输出通过 MppFrame PTS 转回；存在时间戳传递通道，但尚不能证明任意源的唯一身份映射 |
| rkmppdec.h:84–90 | 中间时间基为 1/1000000，双向 av_rescale_q；后续身份方案必须证明精度、溢出、重复 PTS、跨 generation 的行为 |
| rkmppdec.c:670；decode.c:1592、1563 | 输出调用 ff_decode_frame_props；公共 helper 从 last_pkt_props 复制 opaque，并非从该 MppFrame 的对应 AU 取回。不能仅打开 COPY_OPAQUE 就宣称解决重排身份 |
| rkmppdec.c:1152 起 | 没有新输入时使用 MPP_TIMEOUT_NON_BLOCK；发送 EAGAIN 时内部才使用 100ms 接收；draining 使用阻塞接收。100ms 不是对外完成通知或有界完成 SLA，不能直接充当新的轮询常量 |
| MediaPipelinePlanner.cpp:347、365 | RKMPP 目前规划 SubmissionOrder，并要求输入 cadence 规划 receive interval；这两项均与统一 SourcePresentation 尚不兼容 |
| MediaRtpSourceFirstFrameProbe、MediaVideoDecodeLineageOption | 当前 SourceAccessUnit 仅允许 COPY_OPAQUE，探测和运行时需使用同一身份传播契约，不能只改 probe 放行 |

上述固定版本父提交及补丁范围见 [既有依赖记录](../3rds/ffmpeg-rockchip/README.md)。Windows 本机 mabs 的 rkmppdec.c 与该依赖不同，不用它替代远程能力证明。

## 实施顺序与验收门禁

1. 完成后端身份与接收调度的类型化契约设计，再统一切换合屏入口。Windows 复用 COPY_OPAQUE；RKMPP 在现有 adapter 内证明可关联的返回身份，不使用提交 FIFO 猜配。
2. 身份参考 [GStreamer V4L2 decoder](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gst-plugins-good/sys/v4l2/gstv4l2videodec.c) 的 frame number 往返关联方式，以及 [GstVideoCodecFrame](https://gstreamer.freedesktop.org/documentation/video/gstvideoutils.html) 的独立身份。它是调查候选，不是 RKMPP 已支持的证明；必须先核对 MPP 是否原样保留载荷以及是否参与排序、字段解码、时间戳修正，不能盲目替换 PTS。
3. 调度继续复用 VideoDecodeNode 和 DAG executor。FFmpeg context 单线程串行访问，不新增并发 send/receive；若需要 adapter 执行单元，必须明确取消、join、flush 和有界交接队列。完成通知能力不足时，需从已有显式部署期限形成有依据的调度产品，不能把名义 VUI 帧率、输出帧率或依赖内部重试常量当输入事实。
4. 资源继续由 planner 及既有准备预算、lineage registry 和 payload credit 管理。保留 admission 背压；容量耗尽、未知身份、非精确映射不得静默丢弃或 fallback。generation purge 应同时清理未完成身份与输出引用，使用既有 RAII。
5. 可观测证据至少包括每源提交身份、输出身份、真实 PTS、generation、输入 PTS 回退与输出身份重排、等待/背压及停止原因。现有 ReorderedFrames probe 可用于含 B 帧的验收；无 B 帧源不能被强制要求产生反转。
6. Windows Release 先复验 r393 同规格四路 H264/HEVC；再复验单源/选中音频失源、全源失源、重入、连续输出。SourceDisplayPoint 不依靠帧 duration 自动失效，必须确认 lifecycle 撤销输出后黑屏/静音正确。
7. 共享修复 Windows 通过后，RKMPP 使用现有目录增量全部目标、8 核编译，再完成相同链路。最后补两路、C API、持续 A/V 漂移、内存、出口与长时可靠性，双独立审查后才能声称完整完成。

当前没有新增公共字段，没有降低源参数，没有依赖修复授权扩展。此文档不是测试通过记录；本轮仅只读调查，无媒体进程、抓包或截图产物。
