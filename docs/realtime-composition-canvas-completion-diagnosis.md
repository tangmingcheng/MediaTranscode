# 合屏 CUDA 完成等待诊断

日期：2026-11-02。范围仅增加同一批次诊断；不改变同步、发布、发送、容量或优先级。

r379 的 video cursor 3125（canonical sequence 3126）在 compose 内等待约132 ms，随后 scheduler 等待该video head导致audio 4486超时。现有壁钟不能分辨GPU执行/排队、驱动等待与CPU恢复调度。

## 权威依据与适用性

[CUDA event接口](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__EVENT.html)允许在同一context创建带计时event并反复record；同步完成后读取elapsed。测量区间约0.5微秒分辨率，可能包含其他stream工作交错，且不覆盖start event实际执行前的排队，不能视为纯拷贝耗时或CPU等待分解。[CUDA stream接口](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__STREAM.html)的同步仍负责完成语义，elapsed不得替代完成/错误检查。

[GStreamer实际CUDA内存实现](https://github.com/GStreamer/gstreamer/blob/main/subprojects/gst-plugins-bad/gst-libs/gst/cuda/gstcudamemory.cpp)通过stream与待同步状态维持生命周期，CPU访问和释放前完成同步。本项目保持私有NON_BLOCKING stream、原source event依赖、CompletionScope及同步后publish，不因诊断删除等待。

## 固定资源与流程

每个CUDA canvas adapter准备期创建两个可复用timing events，最多两个driver-owned event；无逐帧分配、后台线程或新队列。宿主仅固定handle与标量记录；driver内部字节未知，不冒充引擎payload预算。event RAII在所属context销毁，library及frames device引用活到销毁之后。部分创建失败也由同一owner清理。

仅现有State诊断scope活跃时测量batch。在黑底/tile提交之前record start，在批次提交（包括部分失败）之后record end；然后照旧cuStreamSynchronize，成功后读取同对events的elapsed。记录调用PID/TID、stream/context身份、start/end record原码、同步前后steady、同步原码、elapsed原码与availability。诊断不可用不伪造0时间；原媒体提交/同步/恢复错误保持原顺序，诊断错误仅作为明确缺失证据，不能吞掉媒体错误。无需新增公共参数。

数据写入当前线程现有MediaCanvasComposeObservation::Scope的固定Record，随同一最大compose快照一起保留，禁止独立GPU最大值拼接。仅在既有结束日志输出；不添加每帧日志。沿用aggregate video cursor与encoder token/scheduler sequence定位，不另建关联表。

准备失败边界：缺计时exports或event创建失败仅使计时明确不可用；但实际context push失败作为CUDA执行环境错误传播，不解释为可继续的能力缺失。push成功后，无论event创建成功或部分失败，都显式close恢复context；恢复失败必须使prepareStream失败，析构重试不得使准备结果变成成功。若此前event创建已失败，保留其阶段和原码并记录清理证据，同时传播致命的context恢复错误；已创建event仍由RAII持有和销毁。正常批次同步与原媒体首错顺序不变。

## 解读与验证

GPU interval长只支持event之间设备时间长，不等于拷贝引擎持续忙；GPU interval短而同步壁钟长仍可能是start前设备排队、驱动阻塞或线程调度。线程CPU计数有粒度限制，ETW不可用时不得声称已区分Waiting/Ready。event命令有观测开销，结果只能作为原规格链路补充诊断。

本块需冻结后双独立审查及主agent安排Release构建、原规格realtime复验；当前不宣称构建、实流或修复通过。检查正常/部分提交失败均保留同步和释放顺序，诊断Unavailable有原始原因，event数量固定，无每帧日志或产物残留。

## 本轮验证

初始化context恢复失败传播已修复，两位独立复审PASS；与容量接口同批Release第二次全量785目标成功（首次容量类型C2665失败保留）。r380同批计时实际可用：最大compose47.0356ms、batch completion46.2865ms、GPU event46.8623ms，全部相关CUDA原码0。该样本早于重入阶段sender首错，不能混作其原因，也不能追溯证明r379的132ms根因。r380发送失败及原生画面/清理详见realtime-composition-r380-validation.md，未宣称媒体故障修复。
