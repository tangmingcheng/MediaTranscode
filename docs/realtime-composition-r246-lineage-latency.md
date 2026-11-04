# r246：同AU进入scheduler前的延迟定位

## 真实失败与调查依据

r245约25秒audio837在等待video时超期；已有最大canvas事件不是同一个lineage，不能拼接为因果。先定位canonical AU构造到scheduler观察head这一边界，再决定是否继续深入encoder/canvas，避免添加无法关联的大量采样日志。

工业依据：[GStreamer latency tracer](https://gstreamer.freedesktop.org/documentation/coretracers/latency.html)区分全管线和逐element延迟；[buffer-lateness](https://gstreamer.freedesktop.org/documentation/rstracers/buffer-lateness.html)按buffer记录流时钟与管线时钟。本项目复用MediaAccessUnitProcessingTrace的steady-clock观测方式；只借鉴按同一媒体身份测量边界，不宣称实现GStreamer tracer。

## 实施

- canonical AU在构造时保存一个可空steady时间，复用mediaAccessUnitObservationNow；State关闭时为空。其值随对象不可变，跨线程发布后不修改。
- scheduler已有first/late head诊断附带节点、完整lineage身份、sequence、canonical构造时刻与日志观察时刻。两者差值包含构造后发布/背压/通道等待和scheduler读取前时间，不称为纯队列耗时或成功提交时间。
- 一个AU只增加一个inline optional时间，不增加动态诊断分配、历史ring或线程。诊断输出沿用已有first/late频度，缺失时间写unavailable；不改变媒体参数、失败语义、排序和期限，不引入平台分支。
- Windows/RKMPP使用共享canonical AU和scheduler实现；先Windows原规格实流、原生VLC snapshot。此步仅缩小迟到边界，不能宣称已取得完整canvas→encoder→scheduler同帧因果链。
## 审查与构建记录

首轮A/B均FAIL：可选logMediaHeadArrival未隔离字符串分配异常。首次Release会话17481 exit1：C2678，强类型MediaNodeId不能直接流输出。已将完整诊断置于function-try-block，并输出nodeId().value；两位重新实际读取源码后均限定PASS。修正后全量构建会话52713进行中。不得把首次失败构建用于实流。

修正后全量Release52713 exit0，configure/build均0、717步骤；两CLI、cache与Ninja均存在，local未执行。r246实流失败并完成清理，见r246-validation。同AU251构造→head65500ns，已将主要调查点收敛到canonical构造前。独立核验发现NVENC receive回调还可send frame，bitstream lock为doNotWait=0；下一步需要同lineage encoder输入、send/receive起止与返回码，不将API名字等同GPU实际提交。当前没有改变CUDA同步策略，完整迟到根因与合屏验收仍未完成。
