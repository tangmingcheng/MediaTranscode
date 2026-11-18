# 编码输出进度契约实施计划

目标：解决 r385 中已就绪音频因另一流暂时无头而失约的问题，同时保持严格跨流排序、原输出期限及通用 DAG。此计划不是验收完成记录。

## 已核验依据

- [r385 关联调查](realtime-composition-r385-canvas-correlation.md)已定位同帧画布完成等待；视频到达时自身仍余 13.659801 ms，音频已失约。
- 合屏生产者用同一 planner 有理数网格计算当前和下一 presentation，成功提交才推进 cursor。它可以声明后续帧时间下界；任意包 duration 不具备此语义。
- aggregate → output encoder 直接连接。普通 filter、帧率转换和 transfer 会重建 wrapper，不能自动继承未来输出进度。
- [NVIDIA 编码指南](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.1/nvenc-video-encoder-api-prog-guide/index.html)描述输出缓冲区队列顺序；本机 nvenc.c 每个就绪表面生成完整 AVPacket。保序输入到输出的时间映射还须核验实际无重排配置，不能仅凭缓冲区 FIFO 推导。
- 独立审查 encoder_progress_evidence 条件认可附包传播方案；实际能力产品、实现与运行验收仍未通过。

## 类型与模块边界

1. 新增内部 `MediaVideoPresentationProgress`，定义为同流同代次在本帧之后发布的全部帧 presentation 的下界；绑定承载 wrapper 的 lineage。它不是当前帧 duration，也不是编码输出 DTS。
2. `MediaCanonicalVideoFrameBuffer` 显式携带可选生产者事实。aggregate 提供精确 next；其他生产者明确表示无此事实。不要塞入通用 canonical lineage，防止时序变换节点误继承。
3. 在已有 codec lineage variant 中增加编码输入产品，包含 canonical lineage 与生产者事实。复用 registry/token 的 opaque 与 submission-order 路径、容量和代次退休，不增加旁路 map；资源 planner 同步计算真实类型尺寸与所有权。
4. planner/capability adapter 生成保序完整帧输出及时间映射契约，builder 显式传给 encoder。缺能力时不能承诺进度；承诺后缺事实必须在相应边界报错。通用 encoder/scheduler 不按平台或 codec 名称分支。
5. 编码包 wrapper 携带独立 decode 下界。只有满足已认证输入输出关系时转换；不得覆盖本包 canonical decode，不得混用 packet ticks 与 canonical 纳秒。

## 提交状态与边界

- 进度附在其对应的完整输出包上，受原有 pending/backpressure 保活，不增加进度线程或共享旁路状态。
- EAGAIN 不发布或推进进度。WouldBlock 时包和事实共同等待；scheduler 仅在该包成功提交后采纳后续下界。
- 证明条件为：当前输入之后所有输入 PTS ≥ B；编码保序；当前包是本输入唯一完整输出；canonical DTS 映射已证明。满足后当前包之后所有输出均 ≥ B，包括已接受但尚未完成的输入，因此本方案不需要第二套 accepted 队列。
- scheduler 每流持有同身份、同 generation 的已提交下界。缺头时仅当另一流头严格早于下界才放行；相等保持现有 tie-break。未获证明时不能因墙钟超时猜测顺序。
- flush、purge、重新激活清除旧进度，旧 generation 包不可推进新代次。动态输出各自绑定能力和状态。
- 所有状态有固定尺寸或使用原有有界 registry，新增元数据纳入预算；错误保留原因，输出期限不放宽。

## 实施与验证顺序

1. 生产者类型、wrapper、编码 lineage 产品与精确资源费用一起实现；核对全部重建 wrapper 的调用方和 generation 访问者。
2. 实现 prepared 能力 readback、builder 传递、encoder 附包转换及完整输入事实校验。
3. 实现 scheduler 提交后更新与缺头选择，补充进度来源、下界、代次和等待原因诊断。
4. 冻结代码后两名未参与实现的智能体审查，逐项核对工业实现语义；任一未明确 PASS 则修复并双复审。
5. Release 全量重建；仅 realtime 原规格 FFmpeg→CLI→VLC，覆盖三格、全失源黑屏/静音、重入、再次失源、调用方停止及共享单源回归。实际命令与结果先归档，随后清理逐轮产物。
6. 按用户最新指示，当前先推进 RKMPP 构建与基本合屏，再集中做两端细致可靠性验收；不能把未认证后端静默改为特殊媒体链路。

仍需独立处理 CUDA 批次内部约 149 ms 的原因、准备存储总准入、内容级 A/V/静音、线端发送、长稳及其余功能矩阵。进度协议通过不等于整个合屏通过。

## 当前实施状态

生产者事实封装已落地：新增 MediaVideoPresentationProgress；帧 wrapper 要求显式传入可选事实并校验下界；aggregate 提供 next，全部其他重建调用明确无事实。固定字段由既有 sizeof 预算计算覆盖。progress_frame_review_a 与 progress_frame_review_b 均明确源码 PASS，仅限此 slice。Release 全量构建会话 74113 已退出0；registry 产品层追加后会话 37353 同样全量构建退出0。尚未接入编码 lineage、prepared 能力和 scheduler；不得据此关闭 r385 或宣称完整协议已完成。
