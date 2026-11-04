# r267：CUDA 合屏完成等待与音频发送超窗

## 已证明的失败链

真实链路与完整日志见 realtime-composition-r267-validation.md。以同一 steady clock 关联，不把不同阶段的独立最大值强行拼接：

| 阶段 | steady 时间／观测 |
|---|---|
| 前一视频提交结束，PTS6816 | 25300165017200ns |
| 聚合开始构造视频cursor6817 | 25300193626100ns |
| 同次canvas | 25300193633400至25300364380100ns，170.7467ms |
| 同次CUDA batch完成等待 | 25300193732100至25300364359600ns，170.6275ms；返回CUDA_SUCCESS |
| 下一视频提交开始，PTS6817 | 25300364479300ns |

因此199.4621ms提交间隔中的主要时间位于canvas完成等待，非该次编码提交。相关canvas细分记录batch_completion=170.6246ms、plane_copy总计0.0799ms；不同计时包装造成微小差值。thread_cpu_ns=0不能证明GPU忙或OS原因。

音频9786在调度器等待缺失视频头152.7538ms，随后提交已迟到；mux收到该包时超过dispatch 33.871797ms。maintenance已提交的服务尾部晚于音频deadline26.081645ms，严格失败并正确传播CLI退出1。不得通过改时间戳、丢包、放宽预算或隐藏失败处理该问题。

## 当前实现与官方语义

- MediaCudaCanvasAdapter.cpp的transfer：先向private nonblocking stream提交源event等待及异步copy，再由CompletionScope::complete调用cuStreamSynchronize。
- MediaVideoCanvasAdapter.h明确同步完成合同，batch只借用源帧；Producer完成后才publish目标。直接删除同步将使源引用/目标复用早于GPU完成。
- NVIDIA官方流/事件同步语义：https://developer.nvidia.com/blog/how-overlap-data-transfers-cuda-cc ；https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__EVENT.html 。stream wait提交耗时不是源event完成时间；stream同步等待也不能作为纯copy GPU耗时。
- 当前源readiness已有事件、保活引用及失败清理；这只证明源依赖可传递，不证明canvas产物能直接异步交给编码器。
- 本机FFmpeg源码nvenc.c在NVENC_HAVE_CUSTREAM_PTR条件下调用nvEncSetIOCudaStreams。条件源码不是部署库实际启用及跨平台支持的证据，不能据此省略同步。本轮仅读取外部源码，未修改依赖。

## 下一调查边界

需区分源event依赖、GPU操作持续时间与host线程唤醒延迟。现有数据只能定位API等待位置。PATH存在nvidia-smi，未发现nsys；不据此宣称系统未安装Nsight。后续观测不得引入额外逐tile同步或改变原规格。

异步方案若采用，须复用DAG readiness合同并同时证明源/目标保活、encoder实际等待能力、在途数量/内存预算、错误与取消清理；Windows和RKMPP合同共同审查，平台仅adapter不同。即使异步化也不能保证170ms GPU完成延迟消失。输出存储统一预算仍须完成，不能用此诊断替代原任务。

独立只读复核确认上述同次时间关联成立，但GPU等待来源未定。建议关联源lineage/cursor的source event query状态，以及所有依赖等待之后、copy之后的事件完成区间；不得增加逐帧阻塞同步。当前未修改生产行为，也没有修复后验收结论。
