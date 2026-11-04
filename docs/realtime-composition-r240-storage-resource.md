# r240：复用存活分配计费基础

## 范围与实现

为 r239 后续网络运行存储约束抽取通用 PMR 实现：新增 `MediaBudgetMemoryResource<Budget, Lease>`，准备资源改为其专用类型别名，删除原独立 cpp，factory 改用类型定义 include。不新增公共参数，不改变串行 reservation 门控，网络预算尚未接入。

分配布局、对齐和溢出检查沿用原准备资源；先取得实际分配字节（包含 header/padding）的 lease，再申请内存。每块内存持有 resource owner 和 typed lease；释放内存后才归还额度，owner 最后释放。`reserve` 必须精确返回 `Result<Lease>`，lease 移动构造与析构必须 noexcept。不同 Budget 类型不共享容量。错误字符串由 preparation 改为通用 budget 描述，错误分类保持。

此抽取复用已有算法，避免另建同义分配器；工业依据及适用差异沿用 [r239 边界核查](realtime-composition-r239-reservation-boundary.md)。仅统计 C++ 分配请求，不把 allocator 内部开销、RSS 或 VRAM 宣称为受测数值。资源对象自身的初始创建仍不经过该 PMR，网络 planner 接入时必须计入独立基础开销。

## 门禁

- 两名独立智能体均对当前抽取切片明确给出限定 PASS；不覆盖网络接入或合屏完成。原 cpp 属未提交新文件，审查没有可用 HEAD 前后 diff，结论针对当前实现及所有权契约。
- Release 全量构建：会话39451最终 exit 0，configure/build exit 0，clean-first/all-target 完成；缓存、Ninja 图和两个 CLI 产物已核实。realtime 6058496 字节，2026-10-16 11:32:38，SHA256 A508849E6EA93EB2F3E79E722DA55A682D1C4EDDA568562037D458BBE90F1E77。local 仅核实产物，未执行。
- 实流：尚未执行。r239 失败仍未修复。

## 后续必做

把网络存储的 typed budget、planner 分配几何和实际 owner 绑定到此公共机制；覆盖 workspace、vector 扩容重叠、事务与控制块存活，再评估移除串行门控。原计划四源、非 cadence、A/V、失源重入及 Windows 后 RKMPP 门禁保持。
