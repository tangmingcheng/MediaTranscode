# 视频滤镜输入接受与输出背压状态调查

状态：发现可确证的源码重试缺陷；已实施最小接受状态修正，待审查与重建复验，尚未证明它是 r352 的唯一运行根因。r352 当前与前一输出的 encoder token 为29/28、输入PTS为858994/858993，各成功send一次和receive一次，但canonical sequence/期限相同。

## 官方语义与当前差异

[FFmpeg buffersrc API](https://ffmpeg.org/doxygen/trunk/group__lavfi__buffersrc.html)规定 add_frame_flags 返回非负代表成功；KEEP_REF让滤镜另持引用，不表示调用者可以因下游阻塞而重发。输入接受与输出提取是两个阶段，输出背压不能撤销已接受输入。

当前 VideoFilterNode::submitPendingFrame 在 add_frame_flags 成功后调用 drainFrames，仅当 drain 全部成功才 reset pendingFrame。drain 中 payload reserve 或 emitOutput 均可返回 WouldBlock；此时 pendingFrame仍在。onProcess先排空输出，随后发现 pendingFrame又调用submitPendingFrame，可能再次向FFmpeg提交同一输入及opaque token。编码器对此重新分配token，所以r352看到不同encoder token并不能排除此上游重复。

VideoFilterNode::rescaleAndValidateFrame调用 rescaleStrictlyIncreasingTimestamp；相同滤镜PTS可以被调整为last+1，而canonical lineage仍保持原时间。这说明相邻编码PTS也不足以证明不同源帧。该推断只描述源码可达路径，尚无本轮filter接受/重入观测，不能冒充运行命中证明。

## 修正必须覆盖的完整状态

- add成功立即结束“未提交帧”的所有权，之后只允许继续drain；输出WouldBlock不得再次add。
- 立即单输出消费契约需要跨调用保留已接受输入的消费状态和输出计数，不能只把pendingFrame.reset提前。当前receivedFrames为局部变量，下一轮drain没有该指针会误报；必须将契约状态纳入已有lineage生命周期。
- 允许背压发生在sink取帧前或输出保留事务后；已取出的输出不能重取/重复发布，已接受输入不得丢失。保持当前输出事务与payload credit机制。
- pendingInputAllocations必须覆盖GPU输出readiness绑定期间，完成后释放；失败保持原错误，purge/abort/reset清除已接受状态。保留producer failure scope的同步安全，不在尚未绑定输出时无条件release。
- 普通可缓冲滤镜与ImmediateSingleOutput消费同一接受状态机，只由既有planner契约校验输出数量。不得新增公共参数、外部FFmpeg修改或平台专属媒体链路。

## 验证与未完成项

先核对FFmpegNodeRuntime保留输出事务的接管边界，再以最小固定状态实现。双独立审查当前因服务额度受阻，不能报告PASS。之后Release全量重建、原r352 realtime规格复验；确认重复canonical现象是否消失、原期限是否仍失败、资源是否回零。Windows通过后再验证共享RKMPP路径，不能凭源码推断平台通过。当前发送等待延迟、周期维护规划、完整合屏矩阵及C API门禁仍独立未完成。

## 实施冻结

仅修改VideoFilterNode.h/.cpp：成功add后立即reset pendingFrame；立即消费契约在既有lineage state保存可选acceptedInputOutputCount，sink输出计数在emit前更新，跨payload/输出背压保留，到sink EAGAIN/EOF时校验恰1并释放输入引用。无新队列或分配，仅固定可选计数；purge/reset同步清空。普通缓冲滤镜继续原drain，不加一入一出限制。producer failure scope在失败/WouldBlock仍按原语义执行；成功排空才release。FFmpegNodeRuntime先接管并完成pending输出，再调用onProcess，因此不得在节点侧重发已取出输出。未新增公共参数或改动外部FFmpeg。两独立审查已请求，尚未取得结论。

## 最新双审结论

本轮重新请求后，crash_release_audit 与 publication_identity_review_b 均已独立完成源码复审并明确 PASS，覆盖滤镜接受状态及token/wait诊断（含Notified计数修正）。此前服务额度终止是历史状态，不再是当前审查阻塞。源码PASS不等于运行修复，Release重建及r353验证仍待完成；全量预算和完整合屏门禁仍未闭合。
