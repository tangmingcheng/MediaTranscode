# Beta运行故障分类修复与master合并审查

master基线d832820b，四路损伤恢复验收冻结97d55142；新增修复只删除MediaRealtimeBetaSession.cpp中4行错误分类。文件SHA-256 `a0d8941ec334697cd854ef2b90b9c9f4d203d6bab8ed26784542008a52e0305e`。

## 根因与最小修复

出口writability/original deadline超时由MediaScheduledDatagramSenderNode返回IoFailure，controller保留WorkerFailure；Beta把任何运行期IoFailure归为SOURCE_LOSS，导致源正常时仍向调用方误报源丢失。普通outcome和紧急firstFailureSignal发布均受影响。

删除该错误特判，缺乏权威来源时使用已有RUNTIME_FAILURE；不改变原始error/native/detail、调用方停止、startup分类或公共枚举，不修改DAG、线程模型、队列、硬件、媒体参数或CLI。参照[GStreamer错误域与通用FAILED原则](https://gstreamer.freedesktop.org/documentation/gstreamer/gsterror.html)，不自行猜测源失活，也不扩大失败来源架构。

## 验证

目标机临时C++诊断直接include真实BetaSession.cpp并调用原分类方法，覆盖运行期IoFailure/WorkerFailure、preflight错误、调用方取消。旧版实际输出`outbound_io_completion=3 expected=5 result=FAIL`、exit1；新版输出`outbound_io_completion=5 expected=5 result=PASS`、exit0。Beta目标8核编译链接exit0。诊断是错误投影的补充验证，不冒充真实媒体验收。

CLI SHA-256仍为`7464cf735973a6f68d71a935e0f4a343bbff92ddd0615a8880451260e738f470`。已完成的run53/55/56/57四项生产DAG不变，按用户要求不重测。临时/tmp/mt-beta-classification.cpp及可执行文件已删除；诊断内容、red/green/build日志保存在目标机out/acceptance/rk-beta-failure-classification，未纳入仓库测试设施。

## 审查边界

PR32改为合并master；master是分支祖先，无双方分叉冲突。a559指定基线原比master多216个提交，因此两位未参与实现者补做完整master增量的功能与架构路径审查，未把原7文件容量审核外推全范围。覆盖planner、装配、生命周期、硬件契约、输入恢复、协议物化、公共sender、资源预算、平台adapter、Beta及构建依赖。发现的Beta分类P2经修复复审关闭；评分与最终结论记录于QUALITY_SCORE.md。

风险保留：核心提交后的驱动聚集；Windows、音视频和其他组合未在本轮重测；长期负载/硬件无响应、TX timestamp诊断债务。裸RTP停源仍按无输入超时exit1，100 ms不代表端到端播放延迟。SOURCE_LOSS枚举保持ABI稳定，目前没有权威来源的运行故障不使用该精细分类。