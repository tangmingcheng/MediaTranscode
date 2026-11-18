# r280：raw RTP准入事实与实施边界

状态：源码调查及b2独立只读核验完成，局部设计PASS，尚未实施。r279b实流期间保持源码冻结。

## 已有权威事实
MediaRtpAccessUnitEmissionContract已提供最大AU payload字节、每AU最大datagrams、最大datagram字节及authority；video通过既有deterministic packetizer算法推导，AAC LATM由PayloadLengthInfo和分片推导。WireTrafficEnvelopePlanner对照prepared emission并使用同一合同。不需要新增公共参数，禁止重复发明容量公式。

需修正此前“raw收集无上界”的笼统说法：正常producer已有合同及检查；未闭合的是准入前普通堆分配、检查时机和实际费用。节点callback自身未直接接入实际budget。

## 实际生产路径
- factory视频选择ScheduledDeterministicVideoRtpPacketizerSession：先检查AU大小，packetize生成NAL与payload集合后才检查count，随后再创建全部RTP datagrams，再逐包sink。不能只改sink就声称内部副本被计费。
- 非视频使用ScheduledRtpMuxFfmpegSession：emitMuxDatagram在sink前检查count；rewriteScratch在open按MTU reserve，仍是普通堆存储。sink失败被保存为terminal failure。
- 当前没有不分配的完整AU分区枚举接口；FFmpeg packetizer不能假定有无状态sizing pass，禁止运行两遍求容量。

## 实施合同
1. planner用现有emission合同形成类型化收集存储契约，金额复用现有allocationFootprint与checked arithmetic，并接入ledger。
2. 在调用有状态packetizer之前完成固定byte/descriptor存储准入；暂缺在此返回WouldBlock。callback只写已有容量，超合同是终止错误，不能作为可重试背压。
3. wire准备暂缺时保留完整收集结果，仅重试wire阶段；不得再次调用packetizer。必须证明持有收集额度后下游准备仍可推进，不能造成自身持有额度的永久等待。
4. 视频内部在原packetizer中抽取同一分片/聚合遍历用于直接写入，保留协议算法；node不得重建另一套处理链。音频继续使用FFmpeg adapter，将scratch纳入资源；FFmpeg内部缓冲另外列明。
5. 普通views应复用r278的同步indexed输入，避免再建vector。按实际owner同时存活周期计费，含旧generation；不能套经验倍数。

参考既有工业级buffer所有权与预分配原则：https://gstreamer.freedesktop.org/documentation/gstreamer/gstbufferlist.html 。协议算法继续沿用仓库已有RFC6184/RFC7798及AAC LATM合同；具体迁移前逐项核对现有实现，不以本地样例替代协议依据。

## 本轮源码核验与连接点
- `MediaVideoNalUnitScanner::visit` 已提供无集合分配的 NAL 遍历；`scan` 只是通过 visitor 收集 vector。迁移必须复用 visit，不能另写 Annex-B/length-prefix 解析器。现有 `MediaDeterministicVideoRtpPacketizer::packetize` 只有 ScheduledDeterministicVideoRtpPacketizerSession 一个生产调用点，可整体替换其输出方式，不保留两套同义打包算法。
- `MediaRawRtpPreparationStoragePlan` 属于输入准备存储；输出 collector 不能借用该名称或准备预算。输出网络域使用 MediaNetworkStorageLease/MediaNetworkMemoryResource。
- `MediaRtpDatagramMaterializerNode::processAccessUnit` 在 packetizedBytes 非空时跳过 writeAccessUnit；迁移必须用明确的已收集状态保留这个重试语义，不能以“有预算”替代“已完整打包”。失败的 sink 已使 session 进入 terminal failure，禁止将回调 WouldBlock 当作可重试打包。
- 下游 `MediaRtpWireDatagramMaterializer::materializeBatch` 在计算真实 outputSizing、workspaceCost、RTCP entryCount 后才调用 MediaWireCommitPreparation::admit。当前网络 ledger 的网络总额不包含上游 collector，也不为此保留后续 wire 额度；简单将 collector 计入同一 root 不能证明等待可解除。

## 工业实现比对与进展条件
GStreamer 的 allocation negotiation 在数据处理前约定池、尺寸及数量，buffer 在最后引用释放后回池；池耗尽允许等待，但这不自动证明跨阶段持有资源的等待图无环。参考 https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html 。本项目采用已有 lease 与固定存储承载相同所有权原则，不引入 GStreamer 依赖。

实施前必须将 collector 与该 AU 下游 wire 准备的同时存活费用纳入同一个可证明的准入条件：其他可独立释放的输出 owner 排空后，仍持有的 collector/adapter/固定协议记录不得挤占当前 AU 必需的剩余额度。必须覆盖 SeparateAvRtp 两个生产者及旧 generation 的实际存活，不可用单源通过或经验倍数证明。独立只读审查正在核对此条件；本节不代表实现或运行验证已通过。

## 准入方案收敛（源码核验，未实现）
只给 collector 先 reserve(C)，随后 wire 再 reserve(W) 的方案不采用：共享额度不能证明后一次预约最终成功。改为调用有状态 packetizer 前一次预约 C+W_bound；split 出 C 的真实 backing，保留 W 的 backing guarantee。wire 获得实际 sizing 后从同一 lease 结算，不再向 root 二次预约。协议 backlog 等待仍保留现有语义，不能据此放开 serial。

W_bound 使用已有实现的闭式上界，所有加乘检查溢出：N 为 emission 最大包数，D 为最大 RTP datagram 字节，R 为 RTCP endpoint 的已规划最大 datagram 字节，E=N+1；输出存储上界为 N*D+R+storageOverheadContribution(E,batchGeometry)，再加 workspace.storageFootprint(N,E) 和 commit.storageFootprint(commitStorage,E)。无需虚构时间戳或执行 N 次 Sizing.append，也不在 planner 重复 RTCP 协议长度常量。

首次 wireMaterializer 构造的协议记录存储也必须在该预约前已分配或纳入同次保证：现状 registry claim 只占 slot，实际 reservation records 在 packetize 后 createWireMaterializer 中才分配，不能漏计。输出 collector、视频无分配遍历、音频 scratch、首次记录与 wire lease 的传递须作为同一实现闭环；仅添加 planner 类型不算完成。

独立只读审查已确认上述反例与连接点；最终实现仍需两个未参与实现的智能体复审及真实链路验证。

## 视频打包存储迁移实施进度
已替换 MediaDeterministicVideoRtpPacketizer 的全 AU payload 返回集合及 session 的全 AU datagram 集合，使用调用方提供的一包工作区和同步 sink。NAL 解析复用 visit：第一遍校验并计数完整 AU，第二遍按原 greedy STAP/AP 与 FU 规则写出；最后 NAL/最后 FU 决定 marker，sink 失败仍终止 session。输入数据在同步调用期间保持借用，sink 必须在返回前消费 payload。

协议依据：RFC6184 5.7.1/5.8 与 RFC7798 4.4.2/4.4.3；沿用单 NAL、贪心聚合、分片及 decode order，不新增 packetization mode。https://www.rfc-editor.org/rfc/rfc6184.html 和 https://www.rfc-editor.org/rfc/rfc7798.html 。

本切片仍未完成：session 的单包工作区暂在 open 分配，尚需迁入同一存储契约；collector、音频 scratch、首次协议记录和原子 C+W lease 尚未接入。禁止将此切片或构建通过当作 r280 验收完成。当前已冻结四个生产文件，正在双独立协议/边界审查及 Release 全量构建；未执行新的媒体验收。

双独立复审结果：A 初审发现 sink 的 bad_alloc 异常转换边界被移除，已恢复 session try/catch 并记录 terminalFailure；修复后 A、B 均明确 PASS（仅四文件协议/所有权切片）。第一次 Release 全量构建成功；修复后第二次全量构建正在运行。真实 raw RTP 验证与预算接入仍开放。

## 本轮构建与实流结果
异常边界修复后第二次 Release clean-first/all-target 构建成功，configure/build 均为 0，必需产物齐全；realtime SHA256 EA52FE8F1A4AC7FA4A1C6237B51712088FD4FB84B4ECB634BEA3BC42B0FEBF45。四文件双独立复审 PASS。
真实 separate RTP 输出已覆盖三路 H.264 720p30 输入→HEVC 720p30 8Mbps CBR/AAC 192kbps，全部初始源和重入源各 120 秒完整结束。VLC 原生快照确认三格→全黑→左下格重入→全黑；调用方停止 CLI 返回 0，错误/截止失败为 0；17 个临时产物及六个 PID 清理完成。命令与完整指标见 realtime-composition-r280-validation.md。delivery_evidence=not_proven，预算接入和接收端 A/V 仍未完成，未将此轮标为完整验收通过。
下一步：将单包工作区、collector 及首次协议记录接入类型化存储契约，并兑现原子 C+W 预约，不保留中间 vector 作为最终实现。

首次协议记录分配前置已由 r281 实现：registry激活时实际分配固定backing，内嵌PMR禁止增长，不再在首AU打包后争抢记录额度。双审/构建及实流证据见 realtime-composition-r281-protocol-record-backing.md；不代表collector+wire原子预约或整体预算闭合。
