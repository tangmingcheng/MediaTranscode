# 合屏发送服务累计观测

rk522 首代音频已排空，但重入发生发送期限失败。已知67包的纯服务需求14.536320ms，发布可用时刻至最后TAT为23.033004ms；现有等待迟到仅解释其中3.492146ms，不能直接认定剩余开销来自socket。

沿既有owner线程诊断和成熟pacer服务债务递推（[WebRTC pacing controller](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/pacing/pacing_controller.cc)），复用已有notBefore、started、completed和预约serviceDuration，累计成功提交数量、服务、起发迟到、提交区间及前次服务结束到本次notBefore的间隙。目标恒等式为：末次nextService减首次notBefore，等于四项累计之和。等待迟到包含在起发迟到内，不重复计费。

固定空间，沿用sender单owner；无新增线程、锁、时钟调用、包保存、队列或公共参数。数值溢出、非法顺序及不明确提交失败标记不完整，诊断不改变生产结果；只观察已成功提交的前缀，不宣称整批全部完成。无部署参数、发送速率、deadline或外部依赖变更。

目标无ETF内核支持，TX timestamp不证明Launch Time能力；本次不启用未经证明的硬件路径。累计值解释执行开销，不提供SCHED_OTHER硬实时保证。

当前：实现已通过双独立源码复审及RKMPP八核增量构建（9/9，退出码0），realtime CLI SHA256 为 c4da927148a750bb9eccdefc1fad7bd89141447c42096f519e65add0188dea4a；随后原规格四源实流重入，依据分解结果选择真正修复位置。完整验收未通过，Windows共享链路回归仍需执行。
