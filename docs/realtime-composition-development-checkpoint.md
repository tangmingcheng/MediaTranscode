# 合屏开发检查点（未完成验收）

本地检查点保存此前累计未提交的项目源码、接口、工具与开发记录，防止继续积压。它不是通过完整真实链路门禁的成功提交，不表示可以合并或部署。

## 已有证据

- r384 完成三格、全失源黑屏、单路重入、再次失源及调用方停止行为观察。
- 最新 r385 在正常三格期间退出 1：音频等待缺失视频导致原输出期限失约；同帧 CUDA 合屏批次完成等待约 150 ms。完整失败与清理记录保留。
- 生产者进度封装已实现；两名独立审查者明确源码 PASS，Release 构建会话 74113 全量 793 目标构建退出 0。
- 新增内联编码 lineage 产品及 registry 泛型访问，复用 token/代次生命周期，按真实 Entry 类型计费；两名独立审查者明确源码 PASS。该产品尚未被 encoder 消费。

## 未完成项

编码器能力产品、进度传递及 scheduler 提交后消费仍待接通；r385 未关闭。CUDA 长等待内部原因、准备存储总准入、内容级音画同步/静音、发送线端证据、长稳、完整 C API/旧 ABI/异构输入矩阵以及 Windows 通过后的 RKMPP 验证仍未全部完成。当前两个局部源码 PASS 不代表累计全部改动已经完成双人最终审查。

相关实施路径见[编码输出进度计划](realtime-encoded-progress-plan.md)，阶段归纳见[六阶段工作说明](realtime-video-composition-stage-summary.md)。保留最新失败，不用早先成功覆盖后续回归。

## 检查点范围

纳入项目 src、include、tools、docs、CMakeLists.txt、plan.md 和 QUALITY_SCORE.md 的累计开发状态。out 下的源媒体、历史测试产物与恢复档案、NUL 及未跟踪的第三方头文件不纳入；不删除这些既有文件。未推送、合并或部署。

补充：registry 产品层 Release 全量构建会话 37353 已退出0，配置与构建均成功。两次构建均为 clean-first/all-target，未运行 local CLI 测试。历史归档存在行尾空格及混合换行告警，本检查点保留原证据内容，不把 diff whitespace 检查记为通过。

用户最新调整：先推进 RKMPP 构建、平台能力及基本合屏功能，再集中做两端更细致的可靠性验收；Windows r385 缺陷继续保持未关闭，不再作为启动 RKMPP 工作的前置阻塞。

## 滤镜执行契约贯通（2026-11-04）

源 planner 保存完整 filterExecution 产品，准备阶段、builder 和合屏资源计费复用同一产品，并校验滤镜描述及帧语义一致；不再由下游重建执行契约。两名独立审查者明确 PASS；Windows Release 全量构建会话 27862 退出 0，realtime CLI 产物时间为 14:55:30。该修改尚未提供 RKMPP Completed/独立输出池证明，rk386 预检失败仍未关闭，不记为实流验收通过。

按用户最新要求，后续 RKMPP 使用 all-target、8 核增量构建，不执行 clean-first；Windows 构建要求不变。
