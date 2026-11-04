# r281：激活时预分配 RTP 固定协议记录

状态：实现、双独立复审和 Release 全量构建完成；同规格实流功能观察完成，整体门禁未通过。

## 问题与依据
r280 调查发现 registry claim 原先只占槽位，固定 records 在首 AU 打包后才向共享 root 分配，不能把账本列出费用等同于持有保证。沿用 GStreamer allocation negotiation/预分配池使用前确定存储、最后 owner 归还的原则：https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html 。

采用标准 std::pmr::monotonic_buffer_resource 的显式初始缓冲及 null_memory_resource，不增加依赖或协议链路。参考 Microsoft：https://learn.microsoft.com/en-us/cpp/standard-library/memory-resource 。本机 VS2026 14.51.36231/include/memory_resource:602 的构造器不分配，do_allocate 在初始缓冲上对齐并取空间；超界调用 null upstream 后失败。该实现 release():627 在无上游 chunks 时不会 rewind 初始缓冲，不能依赖 release 恢复容量。

## 实现与所有权
- planner 内部 recordAllocation 几何来自 MediaRtpWireProtocolState::reservationStorageGeometry；原 footprint 复用同一几何。没有新增 CLI/C API。
- registry 发布前 checked 求和，一次预约全部 records，逐份 split 并 allocateFromReservation 得到实际 backing；不是仅列出预算。
- Entry 内嵌 PMR 对象，backing 由 MediaOwnedMemoryAllocation 持有，无新增 shared budget/resource 控制对象。表尺寸由现有 ledger storageFootprint 自动计费，records 保留原费用，不重复叠加。
- permit 保持 registry 存活，返回该实例 PMR；materializer 创建前核对精确 geometry。state 的 reservations 比 permit 先析构，随后槽位 release 在互斥锁内 destroy_at/construct_at 重绑原缓冲，允许合法重用；root backing 不提前释放。
- Entry 只在固定表发布前移动，移动时用相同 backing 构造 PMR；已 claim 时禁止移动。析构先销毁 PMR，再归还 backing。没有增长、没有外部存储 fallback。
- 失败路径自动释放已 split/未 split 额度和已构造 backing；空 UDP 实例不 reserve(0)。不修改 serial、时序和包格式。

## 审查与未完成
初稿采用子预算/resource；A 局部 PASS，B 指出两个新增堆控制对象未计费，故未通过联合审查。已用上述内嵌 PMR 替换，正在重新双审。初稿曾全量构建成功，但不代表当前最终源码通过。

现有 registry/state 等旧控制对象、scratch、collector+wire 联合预约以及跨 generation 存活仍待闭合；不得把此切片视为全部网络内存或合屏验收通过。完成后仍需同规格 Windows realtime 实流，随后再 RKMPP。

最终替代实现已获 A、B 双独立明确 PASS；registry.cpp SHA256 A28D047DA9D2260D79E4426B390DF032E7EFC99E39324ACEA27DB057FCB66A36。审查核对了本机 STL 源码及空 UDP、多实例、失败释放、槽位复用。当前等待冻结版本全量构建与实流，不以静态 PASS 代替运行证据。

## 最终运行证据
最终源码全量构建成功，必需产物齐全，realtime SHA256 2F2E7D07293F2FBF769116C4A2994B06CA4709F5E146C36E856200AED720BAA7。三路初始源与所选重入源均完整120秒，VLC原生快照确认三格→全黑→左下格恢复→全黑；调用方停止CLI返回0。errors/workerErrors和sender deadline_misses均0，payload预约/释放均259844，最终0字节/0对象。出现4次payload WouldBlock压力计数及late_output_commit，发送最大submit lateness为171836940ns；delivery_evidence=not_proven。未把这轮标为完整验收通过，未把源重入等同registry槽位重复claim覆盖。详见 realtime-composition-r281-validation.md。17个临时文件及六个测试PID已核验清理。
下一步继续collector+wire原子预约与scratch预算，保留已有调度迟交及网络/接收端证据缺口。
