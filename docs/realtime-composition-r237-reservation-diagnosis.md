# r237：全局 reservation 拒绝条件诊断

## 依据与边界

r236 原规格真实链路出现6次同文案WouldBlock后超期。同文案实际覆盖串行提交门控、数据报数量和字节额度三个独立条件，终态不能证明失败瞬间原因。

[Flink官方网络栈说明](https://flink.apache.org/2019/06/05/a-deep-dive-into-flinks-network-stack/)采用显式credit、backlog及可用性通知。这里仅复用准确观测credit状态的原则；其TCP接收端额度不等价于本项目UDP提交契约，不照搬算法或容量参数。

## 实施

保留现有线程、mutex、reservation所有权、通知及planner产品。拒绝时同锁读取三个条件、请求/占用/上限、generation、投影序号、队首reservation身份及已提交前缀，附加到原WouldBlock消息；诊断异常保留原错误。materializer现有失败日志记录最近一次阻塞，固定字符缓冲由256扩至1024（仅诊断空间，非媒体容量），仍显式报告截断。

未修改公开参数、失败语义、调度期限或媒体队列容量。当前观测只区分最后一次拒绝条件，不证明何时释放、何时通知及通知后调度延迟；若证据需要，继续补关联。字符串格式化会产生观测开销，需审查锁区间。Windows实流通过前不进入RKMPP。

## 状态

源码待双独立审查、Release全量构建及原规格realtime实流验证；没有修复或验收通过结论。

双独立审查初次均FAIL：锁内格式化会影响sender释放。已改为锁内只复制固定值及optional队首记录，解锁后格式化，不再读取可变账本成员；两者复审均限定PASS。git diff --check通过。Release全量构建会话4713运行中，尚无构建结果。仍无实流通过结论。

Release全量重建会话4713完成exit0，configure/build均0，必要CMake/Ninja及两CLI产物已核验存在；没有运行local CLI。新realtime SHA256=FEB352AA7F649C2EA45653BE55982712DC08EAAD5B24216E1E486D1CC6140454。下一步原规格realtime实流，当前尚未验证。
