# RTP 恢复批次背压所有权修复

对 53c5c0d0 的独立审查指出：processReordered 在移动全部包和 discontinuity 之前调用 drain；reservePayloadBatch 返回 WouldBlock 时，当前 packet 留在成员队列，但局部 reordered 的尾部包和后续缺口被销毁。FFmpegNodeRuntime 将 WouldBlock 作为可重试状态，无法重新取得已经销毁的数据。这是源码可证明的错误，run41 未复现该背压分支，不能声称实测已经修复。

修改仅在 RawRtpInputNode：既有待处理队列按原顺序持有 packet 或 discontinuity 与 generation；先移交整个 reorder 批次，再处理队首，成功后才移除。WouldBlock 保留当前项和整个尾部，后续调用继续处理。没有新增媒体队列、线程、参数、策略或额外 payload 拷贝，仍由已有接收预算限制批次，RAII 管理数据所有权。

遵循 [FFmpeg send/receive API](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 的背压语义：尚未接纳的输入不能当成已消费，必须保留至下游可继续处理。这里只复用既有节点的 WouldBlock 重试模型，没有移植编解码状态机或其内部容量。

目标机 build9 最初按用户交付指令中止；53c5c0d0 库交付后重新全量构建成功。run42 同会话恢复并持续到源自然结束，收发零丢包；发送内核突发及 GPU 连续采样证据缺失使完整门禁仍 FAIL，详见 [run42 报告](2026-09-07-rk-a559-input-recovery-run42.md)。两位独立审查者均因服务额度限制中断；没有最终双 PASS。共享输入节点对 Windows 有潜在影响，按用户要求不运行 Windows 媒体验收。

用户澄清立即出库指已通过 run41 丢包恢复的 53c5c0d0，而不是本次新增的背压修复。目标机恢复 53c5c0d0 并重新构建出库；2d597ba6 保留在开发分支，必须另行测试后才能交付。使用目标机原有 `/home/tang/package_media_transcode_beta.sh <release-build-dir> <package-dir>`，不另建打包流程；发送队列突发、四路回归及双独立复审继续保留为未完成项。
