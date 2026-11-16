# 会话停止：先发布取消状态，再唤醒等待者

状态：rk446真实RUNNING回调停止暴露竞态；两源码修复冻结，双独立SOURCE PASS，目标增量构建中，尚未实流复验。原失败记录见`realtime-composition-rk446-c-boundaries.md`，此文不替代实测记录。

`MediaGraphRuntimeLifecycleExecutor::stop/abort/synchronizeThreadedState`及failure-supervisor原先先取消payload ledger等待，再设置worker停止旗标。worker可能先收到Cancelled，因旗标尚未可见而记为真实首错；rk446首错为`graph payload credit waiters were cancelled`。不应吞掉所有Cancelled来掩盖发布顺序。

官方依据：[C++工作草案stoptoken.concepts第11–12段](https://eel.is/c++draft/stoptoken.concepts)要求原子发布stop-state后执行取消回调、通知可中断等待，并定义与stop_requested的同步关系。本项目复用现有atomic旗标和executor两遍停止API：全部worker.requestStop → session payload waiter取消 → node interrupt/唤醒 → join。取消进入既有`MediaGraphThreadedExecutor::requestStop`统一执行；stop、abort、failure-supervisor走同一路。非threaded scheduler仍直接取消等待，不新增线程/队列/平台分支。

保留worker的原判定：只有已协调停止且返回Cancelled才按停止退出；非Cancelled真实错误、未协调取消以及已记录primaryFailure不被覆盖。branch failure不取消整个session ledger，维持原故障域隔离；本次不扩展动态分支会话终止协议。

边界核验：Idle没有已启动worker，Stopped在join后发布；stop早返仍取消ledger以覆盖未启动会话的首次清理。abort复用requestStop后再worker.abort会重复interrupt；当前节点实现为置位、中断唤醒或幂等cancel/接收中断，未改变资源释放所有权，实际释放仍在join后的scheduler abort。context.reset中的取消发生于threadedExecutor.clear完成停止及join之后。ledger取消仅撤回等待/未claim授予，已有payload lease仍按引用实际释放。

待双独立源码复审及同规格RUNNING早停、正常完成、真实失败保留验证。该代码Windows/RKMPP共用，修复不能只凭目标机增量构建通过关闭Windows回归门禁。
2026-11-16：两独立审查者均核最终Idle/Stopped补充分支后SOURCE PASS。同步包SHA256 7b5f6d6fe4dcb5b0f12213ef88e9c4f93ff6e241f286a811769c6375ca2178cd；两源码逐项验证目标50993608基线，out/rollback/rk447保存源码、旧CLI和manifest，再应用。初始候选传输在认证前主动取消exit1，重新打包最终冻结版本后传输exit0；不是构建/媒体失败。目标8核增量构建PID2255207，结果待检查。
