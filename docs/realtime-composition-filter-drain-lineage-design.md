# rk487视频滤镜排空与lineage租约收尾

状态：两位独立审查均已DESIGN PASS；最小共享实现完成，待SOURCE双审、构建与真实流复验；运行失败见[原规格rk487记录](realtime-composition-rk487-validation.md)。

## 确证边界

rk487在VideoFilterNode26/45的sink排空后报`Codec flush left live lineage leases`，输出各排队74帧。当前emitFrame已在解析typed lineage后、发布前unref输出opaque_ref；typed lineage本身不持codec token。不能把74个下游帧直接当作74个未释放codec租约。

目标[ffmpeg-rockchip固定版本92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696/libavfilter/rkrga_common.c)已现场只读核验：submit_frame克隆输入；1332–1347的EOF流程排空async FIFO并解锁/交付dst，1349–1350无新输入直接返回，不清src列表；ff_rkrga_close约1250行才清src/dst/pat/pat_preproc。故filter报告EOF不代表全部内部AVFrame缓存已析构。

[FFmpeg AVFrame契约](https://ffmpeg.org/doxygen/trunk/structAVFrame.html)规定copy_props/ref传播opaque_ref引用，frame_unref释放；[AVBuffer契约](https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html)规定最后引用释放时执行销毁。现registry通过该真实回调退槽，finishGeneration要求代内租约为空，不能改成清表或跳过检查。

## 最小共享实现

仅调整VideoFilterNode::continueTerminal的顺序，适用于EOF、Flush、EndGeneration共同滤镜收尾：

1. 继续既有flushGraph及drainFrames；graph存在时必须看到实际AVERROR_EOF，EAGAIN不能视为完成。
2. 确认node无pendingFrame、pendingLineage、pendingPayloadCredit、pendingInputAllocations或acceptedInputOutputCount，且公共pendingOutputBuffer为空。公共FFmpegNodeRuntime原流程已经先提交pending交易，新增明确门禁防未来漏接；任一残留立即报错，不丢媒体。保留原“无graph却有lineageGenerations”的错误检查。
3. 在当前filter owner线程、既有lineage锁内复用resetFilterGraph释放已排空AVFilterGraph，触发库内真实引用释放。该方法只清graph、src/sink指针、input/sink timebase、lastSubmittedPts及graphInitialized/flushed/filterEof；不清lineageGenerations、terminalBuffer、terminalPending、terminalIsEof、generationDrained、outputTimeBase、sourceFramesOwner。
4. 对原lineageGenerations逐个执行原finishGeneration；全部成功才清集合。失败保留原错误，绝不强退lease、取消已经发布的帧或等待下游全空。
5. 保持原terminal授权和原FIFO发出顺序；EndGeneration仍只关闭当前代，EOF仍完成节点，Flush仍可按原路径重建。删除原先过晚且重复的Flush graph reset。terminal输出背压由公共pending transfer持有，不新增状态机/队列。

[GStreamer串行EOS](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)支持前序输出提交后再转发关闭事件；不要求下游消费完才发送事件。已发布AVFrame持有自身payload/hw_frames_ctx引用，释放graph只结束filter内部所有权，不代表回收下游帧。

## 约束与验收

没有planner策略、公开参数、容量或代次规则变化；不改FFmpeg、不加RKMPP特判。Windows滤镜走相同VideoFilterNode，存在共同影响；软件/CUDA等后端仍须真实EOF及原pending门禁，不能因当前RGA证据豁免其他后端。

先双独立审查，再构建和原120秒四源实流。必须核尾视频真实提交、原FIFO标记、后续SourceDrained/purge及重新入流；registry仍失败时保留失败并调查具体owner，不能扩大清理范围猜测修复。Windows同规格运行证据未获前不声称其验收通过。本修正不处理独立的取消资源释放问题。

实现核对：仅修改VideoFilterNode::continueTerminal，新增完整pending门禁，将既有resetFilterGraph移到原finishGeneration之前，保留原无graph残留校验和逐代严格finish检查。源码与文档UTF-8无BOM/CRLF及diff格式检查通过；未运行新构建或实流，不将rk487失败改标通过。

## 实施与验证记录

两位未参与实现的独立审查均给出DESIGN PASS及SOURCE PASS。RKMPP 8核增量构建PID3443879，exit0，4/4目标完成；未运行local CLI。新realtime CLI SHA256：4cb8f05eccba1ab2beb73b9db256b15271ddc7e5cbd90d67c019e5dc1b2eb72a。回退源码及旧CLI保留于目标out/rollback/rk488。原规格四屏实流及Windows共享回归尚未验证，不能记功能通过。构建日志关键结果已归档，本轮传输tar和构建日志按清单清理。
