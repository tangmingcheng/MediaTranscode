# 实时 worker 优先级契约计划

## 当前证据与范围

2026-10-28，只读调查方案；未改变线程、系统或媒体行为。当前源码与 r232 已记录缺口一致，r233 未修复此项。`MediaThreadingPolicy` 只有 Normal / High / Realtime 名称，没有平台映射或优先级保证的定义。

- 合屏 Preparer、SourcePlanner 和实时 planner 设置 High，运行时 validator 要求 High。
- 主图 `MediaGraphRuntime::setThreadingPolicy` → `MediaGraphThreadedExecutor::setPolicy` 保存策略，但 executor 构造 worker 时不传策略。
- 动态 `MediaRuntimeBranch::prepare` 保存策略，start 构造 worker 时同样不传策略。
- `MediaGraphWorker::start` 直接创建 std::thread；run 在 Linux 只设置线程名。当前 src/tools 无设置或读取 OS 优先级的直接调用，也无对应动态加载调用。

确定缺陷是计划优先级未执行，不是已证明的 r366 发送失败根因。线程 mode 已参与执行，不能称整个 threading policy 无效。外部启动器、FFmpeg 自有线程及实际 OS 动态调度不属于上述源码结论。

## 官方依据与映射边界

| 平台 | 已证明的机制 | 不能从现有 enum 推导的内容 |
| --- | --- | --- |
| Windows | SetThreadPriority 设置相对优先级；基础优先级还取决于进程类；GetThreadPriority / GetPriorityClass 可读取。新线程起始为 NORMAL。 | High 不唯一对应 ABOVE_NORMAL 或 HIGHEST；Realtime 不授权设置整个宿主进程 REALTIME_PRIORITY_CLASS。不得自行把名称当作数值契约。 |
| Linux / RKMPP | pthread_getschedparam 可读策略/参数；pthread_setschedparam 有权限及策略限制。普通调度与 FIFO/RR 不同；nice 影响普通调度，Linux/NPTL 为每线程属性，降低 nice 受权限/资源限制约束。 | High 不唯一确定 nice=-1/-5/-10，也不代表 FIFO/RR；SCHED_OTHER 的 sched_priority 不能直接照搬 Windows 值。 |

依据：[Windows 线程优先级](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority)、[调度优先级](https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities)、[Linux pthread 调度](https://man7.org/linux/man-pages/man3/pthread_setschedparam.3.html)、[Linux nice 与权限](https://man7.org/linux/man-pages/man2/setpriority.2.html)。这些接口定义机制，不提供本项目 High 的唯一映射。映射必须作为内部产品语义明确设计并审查，不能声称由探测结果自动推导，也不能通过样例调参选择；不增加公共字段。

[MMCSS](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service) 是官方媒体任务调度机制，但要求真实任务分类、服务能力及任务配置；不把全体节点直接注册为 Pro Audio。Windows 优先级与[电源 QoS](https://learn.microsoft.com/en-us/windows/win32/procthread/quality-of-service)是不同事实，不因隐藏窗口就假定 EcoQoS，也不顺手修改全局电源策略。

## 共同启动契约

1. 先定义既有三档的内部可验证语义、支持范围与平台映射依据，明确 Normal 是保留部署基线还是要求普通级别；未定稿前不实施优先级行为。优先级不等于硬实时期限保证。
2. planner 消费当前平台、部署权限与调度能力事实，形成类型化 worker 启动产品；主图和动态 branch 必须传递同一产品到共同 worker。adapter 仅封装真实 OS 差异，不在节点或 sender 临时决策、不 fallback。
3. 保持现有每节点一个 owner 线程和 DAG 队列/背压模型，不新增媒体线程池、额外媒体队列或自旋。所有 worker 先构造；真实 worker 在首次 node.process 前应用产品并 readback，以有界启动握手报告成功或原始失败，失败线程不得执行媒体工作。
4. 共同启动握手负责启动中取消、部分启动失败及回收：记录首个失败，取消未启动者，唤醒并 join 已启动者，沿现有 failure supervisor/exit token 发布结果。主图和动态 branch 使用同一机制，不复制状态机。握手状态每 worker 固定容量，锁内不等待其他 worker 退出。
5. 准备期发现缺少能力/权限必须失败；真实线程应用/readback 的竞态失败同样传播，不能仅记录警告后继续。Windows 保存 API 返回码、线程相对优先级及进程类；Linux 保存策略、sched 参数、nice 和所需权限/限制。权限探测不是未来调用必成功的保证。
6. 生命周期依附现有 worker，平台资源采用 RAII；不改宿主进程优先级或系统注册表。由调用方借用的线程若未来纳入，须另有恢复契约，本轮不扩展该范围。观测包含节点、PID/TID、计划值、实际值及失败阶段，不把 readback 当作运行时动态优先级全程不变的证明。

## r366 等待 adapter 与验收

现有 Windows adapter 已创建 high-resolution timer，相对时间向上换算为负100ns单位，单次周期和 tolerable delay 均为0，错误传播；两个等待句柄依次 notification/timer，result1是 timer。未发现可证明的参数错误。[创建](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)、[设定](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex)、[等待结果](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)。

r366 的29.7615ms API区间仍不能区分 timer 信号迟到、Ready 后排队或采样边界抢占；修复未消费策略不能预先宣称修复该失败。需要同TID的 timer / Ready / Running 关联证据才能进一步归因；不以无管理员追踪能力为由停止其他已授权工作，不放宽deadline、修改外部FFmpeg或加入经验自旋。

实施冻结后双独立审查，Windows Release realtime 原规格先验证主图与动态branch的真实readback、启动失败/取消/清理及原媒体指标，再进行RKMPP同规格验证。只使用VLC原生快照；记录并清理本轮临时产物。当前仅方案，无构建、实流或修复通过结论。
