# r248：画布复制批处理与完成依赖调查

## 当前结论
r247 是实际失败证据，不是修复通过。上一轮有进展：同一输出 AU 4609 的成功 receivePacket 只有 1100ns，receive 返回至 scheduler head 为 44600ns；canvas cursor4608 的调用耗时189287400ns，相关复制中源等待104332800ns、目标等待84612200ns。全失源黑屏已观察，重入仅恢复过画面，随后失败。

## 当前代码与权威方法
- MediaVideoCanvasProducer.cpp 的 compose 依次执行黑底、各非空 tile；MediaCudaCanvasAdapter.cpp 每次 copy 都同步私有 stream。黑底 Completed 不执行源等待，所以其约79ms不能解释为黑底未准备好。
- VideoFilterNode.cpp 包装并发布 frame 前没有逐帧 CUDA 完成事件。本机只读核验 vf_scale_cuda.c:413,470 使用 hwctx->stream，hwcontext_cuda.c:391–392 初始化 NULL。
- [NVIDIA Event API](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__EVENT.html)规定事件捕获 record 时已提交工作，后续提交不会扩展它；应在生产完成提交后、发布前记录。消费者侧临时 record 不能冒充该帧的独立完成事实。
- [NVIDIA Stream API](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__STREAM.html)提供设备端 wait event 与 stream 完成等待。批内顺序提交后统一完成是可用方法，但不能保证 GPU/OS 的执行时限。NULL stream 的既有依赖仍须保留。

## 拟实施边界（设计审查中，未改生产代码）
1. 将单项复制协议统一为同步批复制：成功返回表示全部已提交工作完成。准备阶段与运行时复用同一接口，不保留语义重复接口。
2. CUDA 使用既有私有 non-blocking stream，所有输入依赖保留，批末统一完成；RGA 继续真实 IM_SYNC 操作，差异只在 adapter。无新增线程、媒体队列或外部依赖改动。
3. 批描述由冻结 geometry 推导；不能用临时无界 vector 或经验常量容量。调用方持续持有全部源与目标，异常/中途失败也在释放前完成已提交工作，保留首个错误。
4. capability preparation 必须运行实际批接口；运行时与准备阶段均覆盖黑底及 tile。共享 producer 变化需审查 Windows/RKMPP，按 Windows 验证后 RKMPP 顺序。
5. 后续逐帧 event 要在生产者记录、随 surface 所有权传递，纳入现有有限存储和 RAII。不得仅把 consumer stream synchronize 改成现场 record/wait 后宣称完成依赖隔离。
6. 观测必须区分主机调用耗时与设备事件完成；当前189ms不是纯 GPU 执行时间。即便批处理验收成功，亦不能将 r239/r247 根因自动标记解决。

## 验收与未完成
两位独立审查者A/B均明确设计限定PASS；尚未实现、构建或实流复验。下一步先落实批描述容量和失败路径，再实现与双审查。实流保持原源、参数和120秒，VLC原生snapshot，完整合屏目标及其他未完成矩阵不缩减。

## 双审查落实项
A/B均要求：黑底先于tile；动态源逐项保留依赖；completion先于context恢复；同步报错不发布目标；host默认流屏障保持；最终等待单列为batch completion，不计入最后tile。不得宣称减少同步次数即解决超期。
批描述候选采用借用视图：固定黑底与目标引用，加已有tiles span和geometry rectangles span；数量一致性在提交前校验，空tile表示无覆盖。这样不创建每帧vector或样例容量数组，复用调用期间已有所有权。准备阶段也传同一视图，不建立第二套媒体链路。

## 实施与源码双审查
七文件实现已冻结：借用batch视图、统一copyBatch、prepare/runtime共用；CUDA一次context和批末completion，逐项源等待保持；host默认stream屏障不变；RGA逐项IM_SYNC；新增独立batch_completion诊断。两位未参与实现者实际读取全部七文件及调用点，均明确限定PASS。尚不证明迟到修复。Release全量构建会话22670运行中。
非阻断优化：caller与batch入口重复validate，RGA会重复import/imcheck/release；后续应归并校验责任，不删除必要校验。

Release22670已退出0；实际运行见r248-validation.md，初始及重入源均完整120秒，caller_stop退出0、错误0。第二次失源黑屏截图缺失，完整门禁仍未通过。实测最大批完成79.2303ms，不能判定此前迟到已根治。17个临时产物已记录后清理。
