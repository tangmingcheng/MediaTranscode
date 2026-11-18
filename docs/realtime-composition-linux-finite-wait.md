# Linux有限等待的线程timer slack作用域

状态：实现已获source_drain_review_a/b双独立源码PASS，RKMPP构建及原规格实流状态见rk486记录；未声称消除调度抖动或保证准时。

## 依据与行为

[PR_SET_TIMERSLACK官方手册](https://man7.org/linux/man-pages/man2/PR_SET_TIMERSLACK.2const.html)说明slack属于调用线程，影响futex/pthread_cond_timedwait等等待；1ns为最小非零API值，0会恢复继承默认值。[PR_GET_TIMERSLACK](https://man7.org/linux/man-pages/man2/PR_GET_TIMERSLACK.2const.html)返回当前值。目标sender已观测SCHED_OTHER和50000ns，不能把该观测值写死为恢复值。

仅Linux的MediaNodeWakeup有限正timeout路径改变：入口先采steady时间，checked形成原timeout的绝对deadline；诊断、获取锁、读取/设置slack耗时均不能续期。已通知/取消或非正timeout按原predicate优先返回。实际等待前以原始syscall的完整long读取当前线程slack，暂设1ns，condition_variable使用wait_until同一deadline。无限等待、Windows等待实现及媒体deadline均不变，不新增公开参数、不自旋。

## 所有权与失败

MediaLinuxTimerSlackScope只归当前调用栈和线程所有，保存真实previous；显式restore在正常到期、通知、取消和wait异常后均执行。读取到0无法通过SET=0证明精确恢复，因此变更前失败。GET/SET/restore失败不fallback。wait与restore同时失败时保留wait错误code/native及原说明，追加restore诊断；wait异常先保存exception_ptr并执行恢复，再生成错误说明。

析构noexcept只作未完成恢复的兜底重试；若再失败用无分配stderr诊断保留errno/previous，不能静默吞掉。显式restore失败仍向调用方返回失败，即便兜底重试成功。m_activeWaitPolicy的RAII清理早于mutex解锁；slack恢复/兜底也发生在同一worker线程。未改线程调度策略。

## 观测与待验

保留原完整wait call与API wall指标；分别新增slack SET与restore的before/after、count/total/max及errno。API wall只包condition_variable等待，包含内核等待和重新调度，不称纯定时器延迟。新增固定诊断字段无媒体队列。

源码格式检查不能替代真实链路：需观察实际恢复值、SET/restore开销与等待lateness，验证正常到期、通知、取消、失败保留及持续运行无slack泄漏。原完整120秒源、转码、码率、RC模式、输出截止和AV漂移门禁不降低。取消资源释放问题另见realtime-composition-abort-resource-release-design.md，本修正不把它视为已解决。
