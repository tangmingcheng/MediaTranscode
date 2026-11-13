# rk416 域级边策略合同修复计划

真实失败：rk416 CLI 在 DAG 校验报告 RTP control edge differs from its planned metadata contract。Windows源本轮提前结束，因此不算完整验收；此构建期结构错误独立成立。

调查依据：[GStreamer allocation negotiation](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html) 按具体连接协商并保存分配合同，而非把出口合同覆盖所有输入。这里复用其合同作用域原则，不移植其运行期协商或默认回退；项目仍由planner提前决策。

代码证据：ProtocolInputBuilder用各source metadata连接；CompositionResourceBinder丢弃source edgePolicies，仅保存output策略；GraphShapeValidator把它传给所有domain。还影响preparedVideoFrame和synchronizedVideoFrame校验。

实现：把现有内部edgePolicies从会话binding移至每个domain，source/output/shared三类构造点分别传既有planner产品，view读取当前domain。无新增对外参数、队列、线程、媒体算法或平台分支；不从实际edge推导预期，不放宽失败语义。现有RAII所有权保持，队列/背压/容量仍来自原planner。共享单源保留原outputEdgePolicies值，合屏source恢复自己的值。

验证：两名未参与实现者独立审查，8核RKMPP增量all，再原规格四路realtime；Windows共享影响明确保留，按当前授权暂不Windows编译。记录实际错误/CPU/RSS/A-V/画面，按清单清理。四文件已实现，尚未完成构建及实流验证。

审查：两名未参与实现者对冻结四文件均明确SOURCE PASS。域对象尺寸增加，现有vector属于普通绑定元数据；未发现旧sizeof预算依赖，不据此声称总内存不变。目标构建及实流待执行。
