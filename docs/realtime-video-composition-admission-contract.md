# 合屏最终准入与元数据存储门禁

基线35510847；本轮为源码与工业合同调查，没有生产代码修改、构建或新媒体测试。调查改变实施顺序：先闭环真实输入准备及共享元数据有限存储，再切换最终compiler。不能靠新增无调用者的facts类型或放开aggregate白名单推进。

## 权威依据

- [GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)：激活前协商分配容量，最后引用归还；适用于planner容量与RAII owner分离。
- [FFmpeg AVBuffer](https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html)：存储与引用分离；aggregate持源候选不产生新的像素owner，canvas输出产生独立owner。
- [C++ vector容量合同](https://eel.is/c++draft/vector.capacity)：reserve仅保证至少所请求容量，shrink_to_fit也不是强制请求。元素数和capacity不能互换，不能用假定增长倍数制造跨平台硬上界。

## 当前代码证据

| 位置 | 已证明的事实 | 对下一实现的要求 |
|---|---|---|
| CompositionSourceResourcesPlanner | 同图逐输入envelope/ingress、逐producer单位界限 | 补并发驻留和真实prepared replay，不能相加N整链ledger |
| PreparedRtpStartupPlanner::plan | 从实际prepared video/audio replay、AU节拍、采集窗口规划startup | 在逐源准备事务调用，并将产品绑定同一输入；decoder线程内部packet retention不是startup缓存 |
| FinalGraphResourceLedgerCompiler::compileLedger | 全图hasVideoFilter、全图pending决定唯一encoder池 | 改为明确owner/path事实；唯一canvas池直接采用现有CanvasRetentionPlanner |
| AvContinuousAggregatePlan.maximumMetadataBytes | 只有验证/消费，当前没有planner派生写入者 | 不可接受任意budget字段加运行时检查作为准备证明 |
| Aggregate::validateMetadataBound | 计lineage/contribution及string/vector capacity，不含deque block/map allocator开销 | 保留既有已计费范围；先受控存储，再形成实际容量合同 |
| CanonicalAudioSamplesBuffer/AudioResampleLineageMapper | 正、连续整样本片段；映射样本数匹配帧 | 复用fragment数≤samples不变量；AudioEncoderFrameQueue已使用这一界限 |

候选音频fragment总数≤maximumAudioCandidateSamples；输出fragment数≤codecFrameSamples；视频贡献与pending generation数均为源数N。源identity来自assembly.video/audio.sourceIdentity，输出identity来自outputGroupKey。以上只证明逻辑元素数和身份内容，不能证明move进入对象的容器capacity。小size可以带大capacity；当前事后metadata gate不能替代分配前准入。

## 按依赖实施的生产闭环

1. **真实逐源准备**：复用现有input capture/probe lease、PreparedRtpStartupPlanner和原deadline/stop；sealed replay与startup产品同源。输入资源保持唯一owner，禁止复制整链preflight而重复打开输出encoder。
2. **共享metadata存储**：在现有lineage/fragment数据层引入有界backing storage与RAII lease，并接通源decode/trim/resample和aggregate真实输出调用者。采用固定存储或有界allocator，在分配超过合同前失败；不另建合屏专用lineage链。共享不可变identity owner，不能用指向可purge字符串的悬空view。
3. **owner与保留产品**：各producer的新分配、各节点对上游的引用、固定候选存储分开。源候选保留源owner；encoder clone保留canvas owner。元素数沿既有样本规则推导，使用实际存储合同生成字节，未知事实在发布前失败。
4. **统一compiler**：同一遍历消费冻结全局预算、producer事实、owner/retention、固定存储和唯一输出池。保留旧single/dynamic的consumer-edge终止槽及root service去重。合并同owner引用、累加独立owner容量；不足时失败，不把预算抬到required。CompositionResourceBinder最终替换single planningLedger入口。
5. **完成接线与验证**：接已批准inputs/grid/audioSource公共准备事务及纯视频源；完成Windows原规格2–4路、单源/音源丢失恢复、全失连续黑场静音、背压和可信结束后，再做RKMPP。单源失败复验不能作为多源成功证明。

元数据共享存储的具体接口和迁移范围须在实施前结合实际创建调用链设计审查；本记录不是未经调查的allocator实现方案。不能只增加一个永远缺事实失败的compiler重载作为上述闭环完成。

## 线程、背压与失败语义

保留原DAG调度及aggregate单owner：先处理source purge，后处理输出背压，每次输入消费受candidate合同限制，一个待提交输出事务。lease随最后引用释放，pool耗尽按既有背压/失败合同处理，不扩容、不丢弃约束。shared source/output处理在Windows/RKMPP复用；仅真实设备/操作系统adapter不同。同步驱动调用的取消仍是边界协作，不保证强行打断。

保留runtime容量、credits高水/余额、每源代次/候选、CPU/内存、A/V漂移及退出证据。可明确观察范围外的allocator bookkeeping和驱动内存；已知候选backing payload、lineage/string/vector元素存储不得全部移出计费范围。完整进程内存硬界仍未证明。

## 状态与风险

设计方向PASS，当前完整准入FAIL。完整合屏维持42/100；r48的no-progress/4对象和AAC重入问题没有修复。本轮没有新增运行成功项、没有测试产物、没有修改外部FFmpeg或master。源码调查不替代运行验收，后续metadata接口落地及上述每项仍需实际实现、双独立审查与原规格真实链路证据。
