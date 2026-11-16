# RTCP BYE 与合屏尾媒体排空调查

状态：只读调查与候选设计，未实施。原始实测证据及命令见 [rk456 验证记录](realtime-composition-rk456-validation.md)。

## 已证实与待证边界

rk456 输入 AAC 共 5168 AU；aggregate 最终 `real_samples=5226496`，相当于 5104 个 1024 样本块，差 64 AU。输出尾部与参考的对应关系显示尾音未全部出现，但总数差不能单独证明具体丢弃位置。

原日志 1344–1354 行已归档：18:46:19.441，source2 出现 `reason=1`、clock generation 2 invalidated generation 1，随后 `clock_invalidated → purging → binder_purge → purge_completed`。音频 binder cutoff=5193、视频 cutoff=3623。此事件比最终 caller stop 早约 48 秒，不支持直接归因最终 stop drain。1669 行记录真实样本数；1732 行记录音频入口 `completed_datagrams=5185`。入口数据报数并非 AAC AU 数，不能直接相减。

当前最强候选是 BYE 引发代次清理时仍存在待播放尾媒体。尚缺 BYE 时各 owner 最后接受、绑定、解码和真实提交边界，不能声称 64 AU 全部在 aggregate 或 binder 丢失。

## 官方依据及适用范围

- [RFC3550 §6.2.1](https://datatracker.ietf.org/doc/html/rfc3550#section-6.2.1) 指出 BYE 后仍可能到达迟延 RTP，成员应先标记离开再删除；这是成员管理规则，不提供应用播放排空时长。[§6.6](https://datatracker.ietf.org/doc/html/rfc3550#section-6.6) 的 BYE 不包含最后 RTP 序号或媒体时间，不能据此构造无损媒体终点。
- [WebRTC RTCPReceiver::HandleBye](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/rtp_rtcp/source/rtcp_receiver.cc) 清除 RTT、报告和 FIR 状态，没有直接调用播放队列 flush。该函数证明控制状态清理与媒体缓存销毁可以分离，不证明所有上层 teardown 都保留缓存；查询时 blob 为 `5498f6870f8fcd377b83a74cde89c9cef43cb341`。
- [GStreamer jitterbuffer 源码](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gst-plugins-good/gst/rtpmanager/gstrtpjitterbuffer.c) 的 `queue_event` 将 EOS 作为串行事件追加到队列；`gst_rtp_jitter_buffer_sink_event` 的 FLUSH_START 独立立即执行 flush。[官方 EOS 说明](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html) 同样区分处理缓冲数据后转发 EOS。可复用的是有序排空与取消分离，不能声称 BYE 自动等于 EOS。

## 当前共享实现落点

- `src/internal/graph/protocol/rtp/MediaRtpSourceUnavailable.h`：`SenderLeft` 为枚举值 1。
- `src/internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.cpp:190`：匹配 BYE 后立即 `invalidatePrepared` 并发布 `MediaRtpClockInvalidation`。
- `src/internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.cpp:412`：`acceptClock` 接收失效快照，交给 clock session。
- `src/internal/graph/sync/MediaAvEpochTransitionService.h`、`.cpp:53`、`.cpp:89`、`.cpp:229`：现有 activation、输出许可和代次转换；当前单一 `outputPermitted` 不能独自表达禁止新绑定而允许旧媒体排空。
- `src/internal/graph/nodes/sync/MediaContinuousAudioAggregateNode.cpp:88`：`servicePurge` 取消旧代 pending、清 `m_audio`、清输入 channel，并完成 purge 确认。
- `src/internal/graph/nodes/sync/MediaContinuousAudioAggregateBuild.cpp:100`：仅源 snapshot 允许输出且有 playback epoch 时复制真实媒体，否则生成静音。

以上属于共享 DAG，不是 RKMPP 编码器独立路径；Windows 存在同类潜在影响，不能以其先前验证通过排除此边界。无需修改 Windows FFmpeg。

## 最小候选合同：先取证，再决定实现

1. 保留 typed `SenderLeft` 原因，区分正常离源与时钟失效、传输断裂、错误取消；不改变现有独立失源和重入语义。
2. 只对有证据的正常离源设计关闭边界：未来接收/新绑定授权与既有 `MediaPlaybackActivation` 的播放授权分离。已绑定媒体保留原不可变 epoch 映射，不能借旧映射授权新代媒体。
3. 排空标记随现有媒体 FIFO 串行传播，由各 owner 确认此前已接受工作完成，再进入现有 generation purge。不能只检测瞬时队列为空：还必须覆盖在途 owner、解码器内部输出与待提交对象。音视频成员分别完成，输出合屏继续运行，尾部真实贡献结束后沿既有规则填黑/静音。
4. 最小范围先证明并保留 BYE 前已接受且已绑定媒体。若还需接纳 BYE 后迟到包，必须复用已授权逐源 `input_latency_ms` 和 `MediaRtpInputWaitPlan`，由 planner 推导有界关闭条件；不得另创默认时长或经验性延迟 purge。BYE 本身不能充当 RTP 接收无损屏障。
5. 复用原线程、FIFO、payload ledger、代次协调和 RAII owner；禁止新增公共字段、独立平台处理链、无界 retired-generation 列表或复制媒体队列。若一个旧代尚未完成而新代到达，必须先证明既有容量和转换合同可表达，不能偷偷并存无限代次。
6. 原始错误、Poisoned、显式取消继续优先；排空不延长媒体 deadline、不补发已逾期媒体、不吞丢包，也不将正常离源套用于所有失效原因。

## 下一轮优先证据

先使用现有统计；不足处仅增加每源每代有界累计量及一次边界快照，以 source、generation、transition、SSRC 和原 packet provenance 关联：

- BYE 的接收时刻、最后 ingress 序号及 RTP 时间；分别标明 RTP/RTCP，不能伪造两通道总序。
- binder 最后绑定 AU；解码最后输入/输出时间、样本累计；区分 fragment packet 与完整 AU。
- aggregate 在 purge 前持有的样本区间、最后真实提交区间、实际取消样本数和 pending 状态。
- 各 owner purge 完成及残留计数；不逐帧打印、不复制媒体、不改变调度或失败结果。

这些证据应闭合输入 AU → 绑定 → 解码 → aggregate 真实提交的尾部差额，再实施已有授权范围内的最小修复。源码方向可进入设计审查；固定时长推迟 purge 不成立。当前仍不能宣称根因完全证明或修复完成。
