# r244：RTP 协议预留账本固定存储

## 实现与设计依据

继续 r239 真实发送超期所要求的完整存储准入调查，复用 r242 的有限存储池及 credit 分离原则和已有 MediaFixedCapacityStorage，不新增队列算法。RTP 协议账本由 deque 改为固定环形存储，容量等于 planner 现有 maximumOutstandingDatagrams。每个存活 record 至少含一个 action，协议 outstanding 仅在整笔完成时扣减并同时 pop，所以 record 数≤outstanding≤容量；部分提交不会释放该协议容量。

创建时检查容量乘记录大小可表示，构造函数去掉 noexcept，使分配失败进入已有 AllocationFailed 路径。媒体和 terminal 两处插入均在锁内先检查固定容量，再递增 identity；无需动态扩容，移除不再适用的插入 bad_alloc 分支。Windows/RKMPP 共用此实现；未新增公共字段、平台分支或修改 FFmpeg。

本轮仅使记录存储几何确定，尚未接入 network PMR，actions/workspace 和完整事务存活计费仍未完成；serial 保持原状。不能据此宣称 r239 已修复。

## 审查与验证

两个未参与实现的独立智能体 A/B 对上述切片均明确限定 PASS；完整网络预算门禁仍未通过。首轮 Release 会话52979因构造定义缺右括号发生 C2143，exit1；已修正，两位审查者重新读取三个文件后均限定 PASS，明确承认初审漏检。修订后全量重建会话81373 exit0，configure/build exit0，716步骤完成。缓存、Ninja图及两个CLI产物已核验；local未执行。realtime为6066176字节，2026-10-16 13:35:24，SHA256 38F205FE7B600B7FDCB436EB6E464B010D6CAE757C2E92AE2AA0D7E7DC405488。已执行同规格三源、失源、重入、caller-stop实流；四次VLC原生快照已核验，CLI exit0。完整门禁仍未通过；实际命令、指标及清理见 realtime-composition-r244-validation.md。

## 容量调查补充

TSMuxSession 每个 pending 来源于一次 writeAccessUnit→beginPes，已有 pending 时拒绝第二个 AU；maintenance 使用独立 cursor。因此该协议批次不跨 AU 聚合。

但不能直接证明 TS framed batch≤raw encoder atomic：MediaTsVideoAccessUnitFramer 可注入真实参数集，并将短 length-prefix 扩为四字节 start code；prepared emission 仅取 encoder packet-capacity，最终 tsStreamDemand 只加 PES header，未显式纳入这些增量。保守余量不是上界证明。下一步必须使用真实 codec configuration 和 NAL layout 规划封装后 AU 几何。此为静态契约缺口，未证明当前验收流超限。

运行失败分类原则参考 [Linux send(2)](https://man7.org/linux/man-pages/man2/sendmsg.2.html)：不可原子容纳与暂时发送空间不足分开报告。该原则适用于内部准入，但不等价于照搬 socket 容量；当前永久超限 WouldBlock 缺口仅记录，未作为 r239 根因修复。