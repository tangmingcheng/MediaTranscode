# 可复用 Datagram 发送控制实施计划

> 供实施智能体使用：逐个 Task 执行，每个 Task 使用新的实施智能体；临时测试不得进入版本库。

**目标：** 将 UDP/RTP 输出统一为“协议物化 -> 公共 pacer/shaper -> 非阻塞 transport”，TX timestamp/zero-copy completion 只作异步证据。

**规格：** docs/superpowers/specs/2026-08-22-reusable-datagram-transmit-control-design.md

## 全局约束

- planner 是唯一策略权威；下游不得 fallback、补默认值或按平台/codec/stream set 选策略。
- MPEG-TS/UDP、MPEG-TS/RTP、独立 RTP 复用同一 shaper/sender。
- wire sustained/peak rate 必须由 prepared encoder emission envelope、完整 mux/RTP/IP/UDP overhead 与 deployment service evidence 共同规划；encoder bitrate/VBV 不得单独等同 transport rate，input AU、startup/handoff/edge queue 不得推导 MTU、socket buffer、service rate 或 burst。
- 不等待 TX completion 才发送下一包；异步证据不得回写 shaper 或协议状态。
- 每个 Task 的临时 TDD 先 RED 后 GREEN，提交前删除 source/target/binary。
- 每个 Task 冻结后由两名未参与实现的智能体审查，修复后两者重新复审。

## Task 1：建立 wire Datagram 与 shaping 类型化产品

**文件：**

- Create: src/internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.{h,cpp}
- Create: src/internal/graph/runtime/buffer/MediaScheduledWireDatagramBatchBuffer.{h,cpp}
- Create: src/internal/graph/planner/realtime/MediaDatagramShapingPlan.{h,cpp}
- Create: src/internal/graph/runtime/buffer/MediaDatagramShapingPlanBuffer.{h,cpp}
- Modify: src/internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuffer.{h,cpp}
- Modify: src/internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuilder.{h,cpp}
- Modify: src/internal/graph/runtime/buffer/MediaBuffer.h
- Modify: src/internal/graph/model/MediaPayloadKind.h
- Modify: CMakeLists.txt

**接口：** MediaWireDatagramDescriptor 只含 generation、endpointId、payload offset/size、canonical release/deadline、global sequence；scheduled descriptor 另含 enqueueNotBefore、enqueueNotAfter、wireServiceDuration。MediaDatagramShapingPlan 一次性接收 service scope、endpoint、MTU 证据、service curve、burst、backlog/residence、batch/socket hard bound、非阻塞提交和可选 evidence plan。

**临时 TDD RED/GREEN：**

- 在 out/tdd 创建临时 executable，覆盖 payload overlap/gap、未知 endpoint、sequence 非递增、deadline 回退、overflow、缺 service scope/MTU 证据和 clone 完整性。
- RED 必须因类型或校验缺失失败；GREEN 必须 exit 0。
- 删除临时 source、CMake target 和 binary。

**真实验证：** 核对 plan encode/decode 字段完全对称；检索新产品无 RTP、TS、codec、socket API 依赖；运行 git diff --check 与 CRLF 检查。

**提交边界：** refactor(graph): define wire datagram shaping products

## Task 2：将三类协议输出物化为最终 wire bytes

**文件：**

- Create: src/internal/graph/nodes/output/MediaMpegTsWireDatagramMaterializerNode.{h,cpp}
- Create: src/internal/graph/nodes/output/MediaRtpWireDatagramMaterializerNode.{h,cpp}
- Modify: src/internal/graph/protocol/mpegts/MediaTsScheduledDatagramSink.{h,cpp}
- Modify: src/internal/graph/nodes/mux/ProjectMpegTsDatagramSinkFactory.{h,cpp}
- Modify: src/internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.{h,cpp}
- Modify: src/internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.{h,cpp}
- Modify: src/internal/graph/nodes/output/MediaScheduledRtpSenderMaterializer.{h,cpp}
- Modify: src/internal/graph/nodes/output/MediaScheduledRtpSenderNode.{h,cpp}
- Modify: CMakeLists.txt

**接口：** MPEG-TS/UDP 产生完整 TS datagram；MPEG-TS/RTP 产生 PT 33/90 kHz RTP 与 RTCP SR/BYE；elementary RTP 产生 codec RTP/RTCP。三者只输出 MediaWireDatagramBatchBuffer，RTP/RTCP 使用同一 global sequence。协议 commit token 只在非阻塞原子 enqueue 成功后提交；WouldBlock 重试复用完全相同的 bytes/sequence。

**临时 TDD RED/GREEN：**

- 固定输入验证 MP2T header/sequence/timestamp、RTCP endpoint/global sequence、TS/UDP 无 RTP header、elementary RTP 重试幂等。
- RED 证明旧 node/sink 仍直接持有 UDP transport；GREEN 后协议层不含 socket、timer 或 pacing。
- 删除临时 TDD 产物。

**真实验证：** 运行真实 CLI preflight/shape，确认三类输出都有 materializer 边且没有第二套 socket/pacing loop。

**提交边界：** refactor(protocol): materialize final wire datagrams

## Task 3：实现公共 service-scope pacer/shaper

**文件：**

- Create: src/internal/graph/runtime/network/MediaDatagramServiceShaper.{h,cpp}
- Create: src/internal/graph/nodes/output/MediaDatagramShaperNode.{h,cpp}
- Modify: src/internal/graph/planner/realtime/MediaScheduledDatagramPacingPlan.h
- Modify: src/internal/graph/planner/realtime/MediaScheduledDatagramPacingPlanner.{h,cpp}
- Delete: src/internal/graph/runtime/network/MediaForwardOnlyDatagramPacer.{h,cpp}（确认无消费者后）
- Modify: src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp
- Modify: CMakeLists.txt

**接口：** reserve(datagram, now) 只按 planner service curve 与连续 scope state 生成 enqueue window 和 service duration。同 scope 的 RTP、RTCP 与 UDP 共享 token/debt；rate、burst、backlog、residence 或 deadline 超限即失败。

**临时 TDD RED/GREEN：**

- deterministic fake clock 覆盖 RTP/RTCP 聚合 rate、跨 batch debt、generation 持续性、burst/backlog/residence 超限和 timestamp 有无不影响 reservation。
- RED 暴露旧 forward-only 局部位移；GREEN 对相同输入产生唯一非重叠预约。
- 删除临时测试并确认全树只有一个 shaper 状态实现。

**真实验证：** 用固定真实源执行 30 秒 MPEG-TS/RTP 门禁，记录 planner rate/burst、datagram 间隔和 shaper backlog/high-water。

**提交边界：** feat(output): shape datagrams by service scope

## Task 4：实现非阻塞 transport 与异步证据旁路

**文件：**

- Create: src/internal/graph/runtime/network/MediaDatagramTransmitPort.{h,cpp}
- Create: src/internal/graph/runtime/network/MediaDatagramTransmitSession.{h,cpp}
- Create: src/internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.{h,cpp}
- Create: src/internal/graph/runtime/network/linux/MediaLinuxDatagramTransmitPort.{h,cpp}
- Create: src/internal/graph/runtime/network/windows/MediaWindowsDatagramTransmitPort.{h,cpp}
- Modify: src/internal/graph/runtime/network/MediaUdpDatagramSenderPort.{h,cpp}
- Modify: src/internal/graph/runtime/network/MediaUdpDatagramSenderSocket.{h,cpp}
- Modify: src/internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.{h,cpp}
- Modify: CMakeLists.txt

**接口：** trySubmit(endpoint, bytes, evidenceId) 只返回 Submitted 或 WouldBlock；短写/未知结果返回 ambiguous terminal error。collector.drainAvailable() 只更新 evidence telemetry，禁止出现 awaitCompletion 控制接口。Linux/RK 与 Windows 只实现 OS socket、writable wait、timestamp adapter 差异；MSG_ZEROCOPY 本轮不启用。

**临时 TDD RED/GREEN：**

- fake port 返回 WouldBlock、WouldBlock、Submitted，验证同一 payload/endpoint/evidenceId 在原 deadline 内重试；短写、abort、close、首错保留必须 fail-closed。
- fake collector 注入 late/lost/duplicate/cross-generation evidence，验证 submitted 计数和 shaper state 不变。
- Windows loopback 与 Linux/RK 临时诊断验证 error mapping 和有界关闭；删除全部临时产物。

**真实验证：** Windows clean-first x64 Debug 和 RK Release 构建；核对 requested/effective SO_SNDBUF、pressure、timestamp available/unavailable 与 delivery_evidence=not_proven 如实输出。

**提交边界：** feat(network): submit datagrams without completion gating

## Task 5：接入公共 sender、Planner、DAG 与参数契约

**文件：**

- Modify: src/internal/graph/nodes/output/MediaScheduledDatagramSenderNode.{h,cpp}
- Modify: src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.{h,cpp}
- Modify: src/internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.{h,cpp}
- Modify: src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h
- Modify: src/internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.{h,cpp}
- Modify: src/internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.{h,cpp}
- Modify: src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp
- Modify: src/internal/graph/runtime/validation/MediaOutputAuthorityShapeValidator.cpp
- Modify: src/internal/graph/runtime/validation/MediaRealtimeVideoGraphShapeValidator.cpp
- Modify: tools/realtime_video_cli/main.cpp
- Modify: include/media_transcode_beta/realtime.h
- Modify: src/media_transcode_beta/MediaRealtimeBetaOwnedConfig.{h,cpp}
- Modify: src/media_transcode_beta/MediaRealtimeBetaFixedProfile.{h,cpp}
- Modify: src/media_transcode_beta/MediaRealtimeBetaRequestMapper.cpp

**接口：** sender 状态机固定为 WaitReservation -> TrySubmit -> WaitWritableWithinDeadline -> CommitSubmit，不存在 AwaitCompletion。planner 从类型化 deployment service/MTU/resource facts 生成唯一 shaping plan。按参数基线移除 caller-owned queue/startup/handoff/packet-size 等内部产品；保留的 transport fact 必须带 scope/evidence。

**临时 TDD RED/GREEN：**

- planner 覆盖缺 MTU/service/rate-burst/resource evidence 的 DAG 前失败，并证明改变 input AU/encoder bitrate 不会改变 socket/shaper plan。
- graph shape 覆盖三类输出各只有 materializer -> shaper -> sender。
- sender fake clock 覆盖 early wait、WouldBlock retry、late、generation mismatch、stop/abort causality和首错。
- GREEN 后扫描 CompletionGated、AwaitCompletion、UserspaceSendReturn、PacingHeadroomNumerator、PacingBurstPackets，生产执行代码应无命中；删除临时测试。

**真实验证：** Windows 与 RK 对三类输出分别做 30 秒真实 CLI 门禁，记录 production DAG shape、shaper scope、pressure、queue、CPU/RSS 和退出原因。

**提交边界：** refactor(realtime): route outputs through datagram shaper

## Task 6：全规格真实验收、文档、评分、双审与 PR

**文件：**

- Modify: README.md
- Modify: ARCHITECTURE.md
- Modify: QUALITY_SCORE.md
- Modify: plan.md
- Create: docs/completed/reusable-datagram-transmit-control.md
- Review: origin/master...HEAD

**临时 TDD 收口：** 确认没有临时 source、target、binary、mock 或脚本；运行 git diff --check、UTF-8/CRLF 与重复 sender/shaper 扫描。

**真实验证：**

- Windows clean-first x64 Debug、RK Release 构建成功。
- 每个平台先 30 秒门禁，再以 out/acceptance/test-continuous-120s.mp4 验收 MPEG-TS/RTP、MPEG-TS/UDP、独立 RTP，不降低任何源参数或转码步骤。
- 每条链路记录精确 FFmpeg source、CLI、VLC/receiver、端口、PID、CPU、RSS、A/V drift、shaper backlog/residence、socket pressure、loss/order、TS continuity/PCR、异步 evidence 覆盖率、退出原因和清理命令。
- VLC 可见，CLI/FFmpeg 隐藏；停止 source 后等待 CLI 真实退出并检查进程残留。TX timestamp/sender pcap 不替代 receiver 和画面证据。

**提交与审核边界：**

- 更新完成文档与 QUALITY_SCORE.md，列出剩余风险。
- 两名未参与实现的智能体同时明确通过；修复后两者重新复审。
- 提交 docs(realtime): record datagram shaping acceptance，push 同一分支，创建 PR，再由新智能体审核 PR 并明确通过。
