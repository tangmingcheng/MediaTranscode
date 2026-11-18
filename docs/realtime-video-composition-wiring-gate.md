# 真实合屏接线：准备与资源准入依赖核查

核查基线a7836de5。结论：同步装配入口已具备，但不能直接循环单源preflight接成合屏；必须先解除实际资源与拓扑准入的依赖环。本次是源码诊断与实施顺序纠正，没有运行或宣称新的媒体验收，完整FAIL、42/100。

## 已核实的断点

| 当前代码 | 直接证据 | 影响 |
|---|---|---|
| `MediaRealtimeVideoRunController.cpp:651` | 只调用单源preflight；SourceRuntimePlanner、CompositionGraphBuilder、PreparedVideoCanvas的新入口没有公共调用者 | 已有组件不等于可用合屏 |
| `MediaRealtimeCompositionGraphBuilder.cpp:45` | validateOptions在构图前要求实际encoder、canvas、各源decoder，校验真实池/设备 | 不能凭几何字段生成图后假装已准备 |
| `CodecResolverEncoderContextBuilder.cpp:274` | 打开hardware encoder前要求正initial_pool_surfaces和非空pool_authority | 禁止猜pool、先随便开encoder再扩容 |
| `MediaFinalGraphResourceLedgerCompiler.cpp:195`及`:515` | 要求非空graph；从encoder输入边、pending及encoder retained计算pool | 与上一项形成环 |
| `MediaVideoCanvasProducer.cpp:39`及`:64` | 要求storage上界，并从生产pool分配与readback核对 | 不能用同形状替代池证明实际可用 |
| `MediaRealtimeGraphResourceLedgerPlanner.cpp:76` | 多个videoSurface事实取最大单元字节、最大驻留数；只有一份inputRetention | 描述单链阶段峰值，不能代表N源并发总量 |
| `MediaFinalGraphResourceLedgerCompiler.cpp:390`、`MediaGraphPayloadProducerRegistryCompiler.cpp:11` | aggregate无retention/producer合同支持 | 仅把aggregate加入白名单会漏算实际分配与保留 |

依赖环为：`实际encoder/canvas → 组合图构建 → 最终graph资源编译 → encoder pool容量 → 实际encoder/canvas`。这是当前接线次序的缺口，不是外部FFmpeg依赖修复请求。外部FFmpeg保持不改。

## 实施顺序

1. **一次生成最终逻辑拓扑。** 拆开结构规划与物理资源绑定；逻辑产品只读持有最终MediaGraph与源/输出成员合同，不暴露可启动runtime binding。canvas geometry与storage allocation分型，不以0或虚假owner占位。源/输出时钟、几何、cadence、真实边容量、candidate及贡献上界必须已有事实；缺失仍先失败。
2. **编译同一拓扑的总准入合同。** 复用现有compiler算术，补aggregate真实pending/candidate/header/metadata等保留量及源owner映射。并发源相加、同一allocation别名去重，唯一encoder/FIFO/协议/发送器只记一次；区分engine payload、prepared storage和外部设备/驱动allocation。不得N份整链ledger求和，不得只调整总预算让样例通过。
3. **准备唯一生产资源并回读。** 从已批准pool合同调用原MediaVideoEncoderPreparer，保留其实际context；各源首帧、同decoder owner、实际滤镜与tile操作证据继续受准备预算和取消约束。实际canvas从该生产池准备。readback超界或设备/池不匹配直接失败，不修改图容量重试。
4. **强制准入与绑定后发布。** 全部源与输出资源完成后才产生可供原runtime compiler/registrar消费的绑定产品。复用当前shape/registration校验与payload credit gate；未绑定或账本不完整的拓扑不可启动。整图只构建一次，不生成并丢弃provisional DAG。
5. **接多源事务与公共入口。** 同一owner管理所有输入、原deadline、取消、prepared replay与预算lease；失败统一回滚。NeedMoreEvidence沿同decoder flush/replay，不重置截止时间。接入已批准inputs/grid/audioSource；补纯视频非音源，完成原Windows→RKMPP矩阵。

线程模型仍为既有节点worker及单aggregate owner；不会以后台无界tick/任务队列弥补慢源。背压沿原有界边与原子发布，资源移交和失败清理使用既有RAII owner。公共运行时和平台adapter不新增替代媒体链路。最终验证仍需源独立失联、黑场/静音、恢复、全源失联持续输出及可信结束，不能以本次结构审查替代。

## 工业依据与适用边界

- [GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)：先协商格式与分配属性，再配置/激活池；引用归零归还缓冲，有限池耗尽形成背压。适用于把逻辑拓扑与物理激活分阶段，不能据此推导本项目具体surface数量。
- [FFmpeg AVHWFramesContext](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html)：设备帧池及initial_pool_size由具体后端实现约束。实际容量、bytes、设备兼容与完成语义仍须由部署能力和真实readback证明；不能把initial_pool_size一概当所有后端的物理硬上限。

上述分阶段是准备过程，不是另建运行DAG。既有r43仅单源共享路径：源120秒exit0、CLI no-progress exit1、4逻辑对象残留，不能覆盖新接线；详见[原记录](realtime-video-composition-source-runtime.md)。
