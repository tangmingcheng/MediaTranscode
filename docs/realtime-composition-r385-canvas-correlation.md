# r385 画布完成等待与音频失约关联

2026-11-04，依据[r385归档](realtime-composition-r385-validation.md)和当前源码核验；未重跑媒体或修改外部依赖。此前“尚缺视频阶段证据”表述过宽，应修正为：已定位对应画布batch的完成等待，尚未定位CUDA区间内部原因。

## 同帧时间链

均为同进程steady纳秒。aggregate cursor2059、encoder PTS2059对应video sequence2060；前一包video2059对应PTS2058。

| 阶段 | 开始 | 结束 | 时长 |
| --- | --- | --- | --- |
| aggregate process cursor2059 | 18750030240700 | 18750180369200 | 150.1285 ms |
| 同次canvas compose | 18750030298200 | 18750180320800 | 150.0226 ms |
| batch提交 | 18750030312500 | 18750030441900 | 0.1294 ms |
| batch完成等待 | 18750030442100 | 18750180293700 | 149.8516 ms |
| encoder提交PTS2059 | 18750180394200 | 18750183094200 | 2.7000 ms |
| scheduler持有audio2956等待video | 18750059423500 | 18750183134000 | 123.7105 ms |

frame_wrapper_domain_review独立确认关联成立：canvas解释aggregate长调用的几乎全部时间；encoder在aggregate完成25µs后开始，随后恢复scheduler。不能只讨论scheduler等待而忽略这一上游证据。

## CUDA证据边界

同batch GPU event elapsed=148.596ms，sync/elapsed native均成功；source query ready=3、pending=0。源码在同一private nonblocking stream上记录start、提交batch、记录end，同步完成后读时间。

[NVIDIA官方cuEventElapsedTime文档](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__EVENT.html)说明其他stream操作可能穿插于事件间。因此不能把148.596ms说成八次copy连续占用GPU，也不能直接归因于WDDM/驱动。三路ready仅排除查询当时尚未完成的readiness，不排除后续GPU竞争或调度。

失败后只读设备命令：
```powershell
& 'C:/WINDOWS/system32/nvidia-smi.exe' --query-gpu=name,driver_version,driver_model.current,utilization.gpu,memory.used --format=csv
```
返回RTX4060 Laptop GPU、驱动610.62、WDDM、利用率0%、显存1925MiB。这不是故障瞬间快照，不证明当时GPU空闲。PATH找到wpr，未找到nsys；未安装工具、修改系统或启动追踪。

## 排序与异步完成的约束

video2060到达master=74146325000，自身dispatch=74159984801，尚余13.659801ms；audio2956 dispatch=74141617454，已先失约。不能断言video本身已过期，或异步化必然修复音频期限。

当前scheduler缺一条未EOF流就无期限等待。已有typed release/deadlineWait，但此分支未进入。上一video的dispatch仍早于audio，仅单调性不能授权提前发audio；恢复后video更晚属于事后证据。须核对canonical duration、sequence与encoder契约是否权威保证未来dispatch下界，不以样例30fps推导通用DTS。

Canvas adapter目前为同步接口，batch借用帧；CompletionScope负责正常和异常路径drain，producer完成后才发布surface。直接删除同步会破坏帧保活、surface复用及成功语义。异步方案必须共同规划source/target/readiness所有权、在途上限、预算、失败/取消及generation排空，并复用跨平台DAG；异步本身不消除事件区间148.596ms，仍须核对preparationLead。

当前九文件预算实现源码双PASS、构建成功，r385实流仍FAIL。本轮仅完成关联调查，未修改scheduler或canvas；不放宽原TS期限或降低验收规格。

## 后续调度实现门禁（当前源码复核）

对照 [Apache Flink 官方进度水位语义](https://nightlies.apache.org/flink/flink-docs-stable/docs/concepts/time/#event-time-and-watermarks)：进度由生产者随数据传播，多输入取各流进度的最小值；队列暂空并不构成进度。这里只借用显式进度协议，不能照搬允许迟到数据的处理策略到严格 DTS 排序输出。

当前不能实现“上一包 dispatch + duration 即下一包下界”：VideoEncodeNode 每次 receive_packet 后独立发布，未提供该帧全部包已经发布的完成事实；MediaEncoderOpenContractAdapter 的 Immediate readback 仅核验无 B 帧、delay、zerolatency 和 lookahead，未证明包/帧基数及未来 DTS。音频 mapper 已检查连续 packetPts 并保存 packetEnd，但尚未作为跨节点、跨 generation 的已提交进度发布。独立审查对此捷径判定 FAIL，当前源码复核一致。

下一实现必须满足：

1. planner 依据实际编码器能力形成类型化 decode-time 进度契约；无充分能力证据不能猜测。
2. 生产者仅在下界之前的所有包成功提交后发布进度，背压时不得越过尚未发布的数据；同一有序边承载媒体与进度，避免额外共享线程状态。
3. 进度包括流身份、generation、canonical dispatch 下界。scheduler 只有在另一流头严格早于已证明下界时才能放行；相等时间保留现有 tie-break。
4. flush、重采集及 generation 退休必须清除旧进度；动态输出分别绑定自身编码器与代次。
5. 进度存储、边容量和在途所有权纳入原资源计划，保持有限内存；错误必须保留原始原因，不放宽 mux deadline。
6. 输出进度与缺头等待原因应可观测；通过同规格 Windows realtime 及共享单源链路复验，再进入 RKMPP。

这是实现前置门禁，不是已完成设计或新增公共参数。当前 video 生产者证明仍缺失；不得仅添加 scheduler 超时放行。CUDA 批次内部长耗时原因仍独立待查。

## 本机 NVENC 生产者证据补充

只读核验 D:/mabs/build/ffmpeg-git/libavcodec/nvenc.c；仓库 HEAD=20054712a242c54aa19d9ae9fca7a0381a2f1397，文件 SHA256=5C9BCF95D198AB520002304BC818A4AB00A3AA76F5AE87B238B40E2C34787234。未修改或重建外部依赖；源码版本与部署 DLL 的构建对应关系尚须核实。

- process_output_surface（2763）：锁定一个输出表面，按完整 bitstreamSizeInBytes 分配并复制一个 AVPacket；成功后 receive_packet（3324）将该表面归还 unused 队列。
- nvenc_store_frame_data / retrieve_frame_data（2720附近）：按输入帧保存 duration 与 opaque，取回时把 opaque_ref 移交给输出包。这提供实际后端的帧对应依据，但通用 FFmpeg API 本身没有被扩展为一帧一包保证。
- nvenc_set_timestamp（2659）：PTS 来自 outputTimeStamp；DTS 使用由 frameIntervalP 决定的延迟及输入 timestamp 队列。无重排配置下延迟为零的分支仍需与实际 prepared 配置绑定，不能只根据样例帧率推导。
- 当前 VideoEncodeNode 在 EAGAIN 时只返回 false，没有发布“此前所有输出已提交”的进度。EAGAIN 本身也不能证明尚未提交的未来帧的 DTS；背压 pending、动态输出和退休必须共同处理。

下一步应先核实部署构建身份与实际无重排能力，再将后端证明表达为 planner 产品，并以同一通用 DAG 边发布已提交进度。禁止在 scheduler 中按 NVENC 名称建立专用放行分支。以上是新增源码证据，不是运行通过或调度修复完成。

## 独立复审与部署身份复核

2026-11-04，encoder_progress_evidence 只读复审：实现门禁仍 FAIL，但下一未提交帧时间并非完全缺失。MediaAvContinuousAggregateVideo.cpp:16–20 已依据 planner 有理数帧率分别计算当前和下一 presentation；MediaAvContinuousAggregateNode.cpp 的提交路径才推进 cursor。可由该生产者显式声明后续 presentation 下界，不能由 scheduler 根据任意包的 duration 推测。普通单源/VFR 需要其自身的生产者证明。

最小实现链为：生产者类型化 presentation 下界 → planner 认证保序完整帧输出能力 → encoder 跟踪已接受但尚未提交的输入，取其最早时间与上游未来下界的保守边界 → 随已提交媒体或同一有序边传播 decode 下界 → scheduler 消费。receive EAGAIN 不能代替输出提交；WouldBlock 时边界不能越过 pending 包。新增状态须覆盖现有 reset/flush/代次退休与动态输出独立生命周期。

重新执行 ffmpeg -version：N-125148-g20054712a2-g9cbd889670+3，libavcodec 62.36.101，退出0；nvenc.c 相对 HEAD 无改动。重新计算 mabs、仓库 3rds/ffmpeg/bin 与 Release 副本，avcodec-62.dll SHA256 均为 F720E06FB04379BF8187094B15FD84CA24B25C8567E2842B5457C5D31CFCE621。版本与部署副本一致，但仍不能声称已证明源码到 DLL 的可重复构建；未找到 ffversion.h 或构建中间产物作为进一步溯源证据。无外部文件修改。

复审通过的是上述事实来源与实现方向，不是新调度实现或完整验收。下一步优先形成通用类型与生产者/编码器能力产品，避免继续把已存在的合屏时间来源当作阻塞。
