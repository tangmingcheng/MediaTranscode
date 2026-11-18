# r277：RTP终止预约边界

## 依据与设计
RFC 3550 §6.6规定BYE表示源不再活动：https://www.rfc-editor.org/rfc/rfc3550.html#section-6.6 。本项目通过同一协议锁下的有序reservation链落实该语义；RFC并不规定内部projectedTerminal字段。

当前terminal在存储准备、global reserve与协议reservation入队全部成功后设置projectedTerminal。普通媒体入口只检查terminalCommitted，允许BYE已预约但尚未发送时继续预约媒体。修正是在现有入口同时拒绝poisoned、projectedTerminal或terminalCommitted，不新增状态机、参数或平台分支。

失败准入不设置terminal，仍可重试；成功预约后bind失败沿用transaction析构poison，不回滚后继续媒体。重复terminal与未发送任何媒体的静默离开行为不变。generation切换重建协议state。保持串行约束，尚不能据此放开多笔预约。

## 验证状态
已实施；两名未参与实现的审查者a与b2均明确局部静态PASS，覆盖协议锁、准入失败重试、绑定失败poison及generation隔离。git diff --check通过。Release全量构建会话1527成功，configure/build退出码均为0；CMakeCache、build.ninja和两个CLI产物存在。realtime SHA256：29A930FBC7AE24B6A4D8E60EF6410EFF6042BC8AFF06CE17972883824B3DF1A0。未运行local，尚未真实流验证。本修正不代表r276截止时间失败已解决。
