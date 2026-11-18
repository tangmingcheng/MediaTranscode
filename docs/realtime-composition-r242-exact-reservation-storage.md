# r242：全局 reservation 精确存储

## 修改与依据

沿 r239 存储边界调查及 r240 公共计费基础，复用已有 `MediaFixedCapacityStorage` 替换全局 reservation 的 wireBytes/materializedAt 两数组和记录 deque。数组容量等于请求条目数；账本容量等于 planner 已给出的 maximumOutstandingDatagrams，不引入公共参数或经验常量。

上界证明：每个正常存活 record 至少有一个未提交 datagram；完整提交立即 pop，因此 record 数不超过 outstanding datagram 数。固定账本启动时分配，运行 push 不扩容。乘法范围在 create 检查，数组尺寸溢出返回 InvalidArgument，分配失败返回 AllocationFailed。

继续沿用 r239 文档中的有限存储池/credit 原则；此次只形成确定的分配几何。存储使用现有组件，不新增环形队列算法，不把未接入的预算声称为完成。尚未绑定网络 PMR，完整数组仍可能在容量拒绝前于锁外分配。

## 错误语义与审查

初审两者均 FAIL：曾增加提前容量拒绝，改变终态错误优先级及背压通知。现已删除，保持原 entry 校验→purge/poison→身份→容量拒绝顺序；identity 自增移至内部固定容量检查之后。两个独立智能体对修订均明确限定 PASS。

序号投影、连续前缀提交、整代 purge、串行门控、协议 deadline 和媒体参数不变。固定数组和账本属于共享 Windows/RKMPP 路径，不新增平台分支。

## 验证与未完成项

- Release clean-first/all-target 会话52645最终 exit 0，configure/build exit 0；缓存、Ninja图及两个CLI产物已核实。realtime SHA256 5E31EBA328F45463190C0C02E8B447F63E5D2E528FC9DE3FA5C8CF1862ABEDB0。local未执行。
- 未执行新实流，r239 FAIL 保持；不宣称固定存储修复超期。
- 后续必须接入独立网络 typed budget，planner 覆盖完整事务、构建 workspace 和批次存活，再允许多 reservation；原规格 realtime/VLC 快照及 Windows 后 RKMPP 门禁保持。
