# r253：生产帧完成事件贯通（实施中）

## 依据与范围
沿 r250 已核验的 NVIDIA event record / stream wait 语义、FFmpeg 实际生产 stream 与 GStreamer memory 所有权原则，在生产输出首次发布前记录事件，消费者仅等待该输出所覆盖的工作。事件不能消除 record 之前已排队的工作，不能据此预先宣称修复 r249 超期。

## 已落地但未构建
- canvas tile 从裸 AVFrame 指针改为帧与 typed readiness 的共同借用；真实 tile 缺完成事实即失败。
- 准备与运行 canvas 共用该接口；黑底来自已完成的准备操作，显式标记 Completed。
- CUDA batch 通过生产事件在私有 stream 上建立依赖，保留批末完成确认及错误传播；host readback 保留其独立源 stream 同步。
- RGA batch 要求已证明的 Completed 输入，不接受 CUDA 依赖。
- aggregate 的临时 tile 存储及 planner 字节预算同步使用实际 tile 类型大小。
- 子智能体实现实际 allocation 事件保护、typed frame 访问及 clone/takeFrame 依赖携带；正在继续生产节点、planner 合同和准备帧接线。

## 未完成与验收边界
源码尚未冻结，未构建、未进行真实流验证。必须完成 decoder/filter/transfer 的已选生产事实、实际 context/stream 核对和准备期同路径接线；不得按帧格式在消费端猜测完成策略。完成后双独立审查、Release 全量重建、原规格 realtime 运行及 VLC 原生快照。完整目标仍包含四源 HEVC 输入重排、非固定节奏、音频静音/内容 A/V、API/ABI 以及 Windows 后 RKMPP。

## 预审与依据复核
独立预审 A 发现 SourceWait 观测器将 Status 转整数的编译问题，已改为保留 Status 返回并分别记录原 nativeCode 与 status_error。独立预审 B 指出已缓存 completion 失败必须继续传播给 enqueue wait，实施者正在修正。新增 frame wrapper 的访问字段及控制块需实际经既有 PMR 分配，不能只更新估算字节数。
NVIDIA 官方文档再次核对：record 捕获调用时 stream 内容，后续提交不改变该事件；event destroy 可异步释放，因此仍须完成确认后回收，不能以销毁句柄证明设备已结束。依据：https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__EVENT.html 。

预审修复核验：A 已实际复读观测器，明确该点静态 PASS；原 Status 完整返回，CUDA 原生码与项目错误码分开记录。主线已核对 enqueueCudaWait 使用同一 mutex 并返回已有 completion 失败。host readback 的 SourceWait 耗时观测已保留。新增 wrapper 预算入口正在迁移为显式 allocation scope，禁止缺 prepared resource 时默认降为 runtime allocator。完整接线仍未冻结或构建。

主线补齐计划传递：MediaVideoSourcePlan 新增 decoderReadiness/filterReadiness 内部事实；MediaVideoPlanOptionApplier 在对应 decoder/filter 节点调用集中 codec 编码并应用，不在 builder 推导 CUDA 策略。集中 codec、准备 readback 与运行节点 capture 由实施者继续接线；当时 codec 尚未创建；后续已补齐，完整节点接线仍待完成。

集中 codec 已落地并获 A 局部预审未发现阻塞：三键完整校验，context 非零，NULL stream 显式保留，uintptr 数值拒绝溢出和尾部字符；三键全缺仅表示原路径未配置，不得被需要 readiness 的合屏路径解释为 Completed。句柄只限当前进程，寿命由真实 prepared owner 保证。未构建，未实流验证。

## 首次冻结复审：FAIL
A/B 均明确 FAIL。准备滤镜提交后，sink 出帧到 capture 之间的目标校验/stream 校验失败可释放尚未完成的输出；sink 部分提交失败同样需要统一完成保护。将同步审查 runtime decoder/filter 提前返回和丢帧路径。另当前 CUDA 专属源事实阻断 RKMPP，必须补平台中立的完成合同与真实 RKMPP producer 证据，不能缩小原目标。
Release 全量构建会话 63216 已启动，源码在其结束前保持冻结。该构建即使成功也不代表本轮审查或媒体验收通过。

构建 63216 已退出 1：MediaCudaFrameReadiness.cpp 引用了本机 ffnvcodec 头未声明的 CUDA_ERROR_NOT_INITIALIZED。已改为直接记录缺失 driver exports 的项目诊断，不伪造原生返回码。未重建验证；源码已解冻以修复双审发现的异步释放缺陷。

## 第二次复审与重建

Release 全量重建会话 26883 退出 0，configure/build 均成功。失败 scope 修复可编译，但两位独立审查者仍明确 FAIL：VideoFilterNode 在 drain 成功后释放 pendingFrame，输出 completion 仅持有输出 allocation，未延长 scale_cuda 仍在读取的输入 allocation 寿命。本机 vf_scale_cuda.c 在异步 kernel 提交后释放输入（627 行），复制 frame props 不保留输入像素。下一步在现有完成所有权中保留消费输入的真实 allocation，并核对别名、无输出与失败路径；不修改外部 FFmpeg。不启动已知存在寿命缺口版本的实流验收，亦不宣称此次构建代表验收通过。

## 消费输入所有权修复：预算复用调查

已确认 MediaGraphPayloadResidencePlanner 不在下游 producer 处截断所有权遍历：沿相同媒体 payload 及 typed startup event 继续遍历，并计入可达节点/边的保留容量。MediaRealtimeCompositionResourcePlanner 将 aggregate storage 同时绑定视频及音频 aggregate 节点，再据此计算各源 producer 配额。因此不能仅看到 VideoFilter 的两个局部引用就断言 decoder 总配额遗漏下游驻留，也不应重复增加经验容量。

尚需随本次实现核验：每个输出最多保留一个消费输入，输入 AVBufferRef 快照元数据经 prepared resource 计费；错误和无输出不会产生额外等待队列；真实图的 startup wrapper 映射、aggregate 保留节点及 decoder 内部池的进展约束均覆盖。这些是源码调查结果，尚不是最新实现的审查通过结论。

预算专项独立审查结果：未发现必须增加 decoder frame 配额的证据。一输出至多保留一个输入且无独立等待队列时，既有可达驻留计数覆盖该关系。startup event 按实际 payload/units 映射，aggregate storage 的 retentionNodes 覆盖共享保留；这些机制可能保守多计，不能据此判遗漏。实际本机 nvdec make_writable 复制、decode 释放 private_ref，以及 cuvid 复制后 unmap，支持当前已准入 copied-output 不长期占用固定 mapped decode surface 的判断。此结论不保证 VRAM 分配成功、执行时限或整图无死锁；冻结后仍须核对 snapshot 原 credit、别名一对一及元数据实际准入。

## 2026-10-19：消费输入修复冻结重建

恢复后确认旧实施智能体已不存在，按当前工作树冻结并启动两位新的独立审查者。Release clean-first all-target 构建会话 65197 退出 0，configure/build 均成功。新代码已接入输入 allocation 快照、CUDA operation 保留及 ImmediateSingleOutput 契约；尚未取得本次双审结论或实流证据，不能沿用旧版本测试结果判定通过。关键七文件 UTF-8 无 BOM、CRLF 检查通过，git diff --check 通过。

2026-10-19 B2 新独立审查已明确给出本次 Windows 消费输入所有权静态 PASS：提交前快照保留原 credit，完成对象持有输入，alias 无环，零/多输出按合同失败并收束。另一位独立审查尚无结论，因此双审门禁仍未通过；本轮实流尚未启动。realtime 二进制 6118400 字节，SHA256 3F0F2CFF6B47379D3BA375BB2848DB7636FBE74581A4F40495D3EF1A86F48CB2。复验前源文件 124427809 字节、指定端口未占用、未发现媒体进程残留。
