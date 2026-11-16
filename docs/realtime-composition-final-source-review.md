# 合屏整体源码双独立审查（2026-11-16）

基线：生产源码ce71265d；实测归档HEAD4b4fe970。审查期间未修改生产代码。审查A为pacing_diagnostic_review，审查B为remaining_acceptance_audit；两者均未参与本轮实现，分别明确给出SOURCE PASS。不是完整交付PASS，不替代真实链路证据。

| 功能 | 两者共同结论 | 当前依据与适用边界 |
|---|---|---|
| 输入与planner | PASS | MediaRealtimeCompositionInputPlanner检查2–4源、选音、逐源等待与准备预算；graph builder核对身份/cadence/格式/队列，无下游猜测默认 |
| 共享DAG | PASS | Windows/RKMPP共用聚合、代次、scheduler、存储与TS/RTP segments；仅真实硬件能力adapter不同 |
| 独立源生命周期 | PASS | purge清旧代candidate及pending后确认；黑场/静音填真实间隙，独立输出时轴继续；仅实际全部seal形成永久结束，普通RTP失源不冒充EOF |
| C入口/ABI | PASS | 独立合屏类型不改变旧布局；start返回前配置转为自有数据；回调发布同步、回调内release拒绝、外部release停止并join。二进制边界仍以rk432/rk433事实为准 |
| 编码完成水位 | PASS，受限后端 | 实际readback/依赖身份/完整AU及PTS/DTS约束；同代同master域strict小于下界才放行音频，成功commit后才存进度，未知能力不授权 |
| 停止与首错 | PASS | worker停止标志先于ledger取消；mux区分RequestedStop/EndOfInput，准备取消独立终态，已激活严格finish，原错与owned IO关闭错误保留 |
| 资源边界 | PASS，限定域 | RAII lease及planner候选容量成立；声明预算不是RSS/驱动/VRAM或所有普通new的全域上限 |

## 行业对照

[GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)的有序输入/GAP/结束事实原则适用；本项目独立源代与区间贡献不等于复制其全部实现。[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)允许零到多个输出，因此完整AU进度必须受后端契约限制，不能通用化为一输入一包或EAGAIN即完成。[C++ stop-state](https://eel.is/c++draft/stoptoken.concepts)先发布状态再唤醒的原则支持本轮停止顺序，不宣称采用完整标准stop-token实现。

## 实证与剩余边界

Windows r449与RKMPP rk450已覆盖当前共享停止版本真实活动输出收尾；r449另有选中源完整120秒重入。两轮各自独立归档、提交及清理。两名审查者均未发现新的确定阻断源码缺陷。

rk427只证明当时服务状态与期限冲突被拒绝；原因证据不足，不能说已修复，也不能反向断言部署长期不可满足或自动追加无限重复运行门禁。内容A/V周期歧义、线端服务证据未证明、短程内存不能代表长稳等限制继续保留。旧单源rk433终态失败与深层部分准备回滚，仍按已有证据范围陈述，不借其他成功场景替代。

历史flash/beep设计中的p95/p99/每小时漂移数值有来源，但原合屏计划未找到明确继承语句。不得仅从后续总结新增这些数值门禁；也不得宣称已有证据达到它们。下一步执行已记录的内容时间标记补充测量并汇总真实覆盖矩阵。
