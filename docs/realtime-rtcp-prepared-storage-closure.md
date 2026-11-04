# RTCP 与 Prepared resource 存储闭环

本轮修复独立审查提出的两个缺口：resource bootstrap 未进入旧 Raw 精确容量，以及 RTCP 树只有实账、没有可证明峰值。尚未编译或真实流验收。

## 权威依据与实现边界

- [GStreamer GstRTCPBuffer](https://gstreamer.freedesktop.org/documentation/rtplib/gstrtcpbuffer.html) 使用指向原 compound buffer 的 packet 游标；本仓库采用相同借用视图模型，不引入 GStreamer 依赖。
- [RFC 3550 §6.1/6.5/6.6](https://www.rfc-editor.org/rfc/rfc3550.html) 给出 compound 长度、padding、SDES item 的 8 位长度、终止与对齐、BYE 格式。现有 parser 的所有格式错误顺序及文本保留；完整验证成功后才允许 tracker 消费。未知 packet 仍按原规则接受。
- [C++ allocate_shared](https://eel.is/c++draft/util.smartptr.shared.create) 通过 rebound allocator 请求对象及控制存储。bootstrap 复用 Raw entry 既有完整实际调用 readback 模式，记录单次真实请求并计入 typed header/padding；不增加 MSVC 专属门禁，不用首请求推断完整构造。

## 所有权与费用

`MediaRtcpCompoundParser` 成功结果仅借用原 datagram，遍历不分配、不复制嵌套树。两个生产消费者仅为 `InputClockSession::observeRtcp` 与 `RawRtpInputPreparer::rtcpBelongsToSsrc`，都在当前 datagram 生命周期内同步完成。pending tracker 保存 SR 标量副本及新 CNAME owner，不保存迭代临时对象地址。

CNAME 接受后通过 `MediaRtcpBytes` 复制到既有 `MediaOwnedByteStorage`，只请求实际长度，不含终止符或 spare capacity；allocator 持有 Prepared resource，move 不分配。copy-assignment 先构造替代 owner，峰值包括旧、新两个 backing。协议 U 仍使用 `MediaLineageVector`，未删除共享实现。

新 `MediaRtcpPreparationStoragePlanner` 对 Raw 输出 parser backing=0；对 Session 按 datagram 上界、源成员数、已规划输出事件上界推导 CNAME 峰值。稳定 owner 为 tracker current/pending、mapper、validator evidence/calibration、各 binder 全组 snapshot；串行回调临时峰覆盖 event、publication snapshot 和单字段替代。它是纯成本，不再次创建 fixed lease。

Prepared resource bootstrap allocator 仅持 budget，不持自身 resource。object 最后强引用释放后，control 及 bootstrap lease 仍存活到最后弱引用释放。共用 block 按原顺序释放 backing、归还 lease、释放 owner；Network 创建路径不新增准入。

## 尚需完成

Session 与候选联合容量必须消费 CNAME 峰；所有 resource 创建点必须对应费用：Raw、Session source/各 stream、handoff prefix 和 IdentityProbe。后两者须覆盖与原 owner 同时存在的阶段。完整 G 仍以联合产品、独立复审、构建及原规格真实链路结果为准。
