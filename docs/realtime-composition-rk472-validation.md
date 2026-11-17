# rk472：接收事务的锁范围

状态：单文件双独立 SOURCE PASS，RKMPP 8 核增量构建4/4、退出0。未运行并发实流。

deliverContinuousDatagram 保持 deliveryMutex 覆盖单项序号选择、source 接受与 frontier 提交；receiver mutex 在 consume/resumeDrain 前释放。Accepted 返回后重新取 receiver 锁，先记录接受事实再响应停止，未接受不推进序号。stop/abort 释放 receiver 锁后 join，batch 与 prepared 状态在 worker 退出前保活；旧 capture 写者与 continuous 阶段顺序交接。

本项为后续 source generation arbitration 缩小锁范围，不宣称修复了已复现死锁；原先静态核验未发现确定反向等待。未增加队列或默认等待。依据既有串行媒体交付模型及仓库 owner/join 生命周期，保留原错误。

构建 PID 2875086；回退目录目标 out/rollback/rk472。临时清单：本机 out/acceptance/rk472-source.tar；目标同名tar与rk472-build.log。

未完成：生产 BYE Opening/Closing、捕获截止、排空标记产生、退休及双成员消费门，之后仍需原规格四屏实流及Windows共享链路回归。Windows FFmpeg未修改，local CLI未运行。

构建结果：CLI 13106552字节；SHA256=95a9f0d6e4e9e9e93a5f63efffcef66232a105892bd315421303ae27eeec227a。

清理复核：本机本轮tar删除；目标本轮tar/log删除且rk472查询为空；构建PID已不存在，回退产物保留。
