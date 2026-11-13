# GCC 启动存储适配计划

## 失败与边界

rk412 已进入四路解码能力绑定及 HEVC 编码器准备，随后被启动索引的 MSVC-only 几何门禁拒绝。后续 generation hash 的几何、planner 和受限 memory_resource 同样假设 MSVC，必须一起适配。目标为 GCC 12.4/libstdc++ 20240620、C++20、非 debug 模式；其他未审计实现继续明确失败。

只改变标准库分配事实适配及其类型化合同，复用现有索引、哈希、启动状态机、planner 和 DAG。无新公共参数、无运行时 fallback、无试分配猜测、无媒体阈值调整。本机 FFmpeg 不变，目标机直接增量编译；共享公式变化需核 MSVC 原值等价，Windows 实际复验仍是未完成项。

## 权威实现与类型化产品

- 目标实际 stl_list.h/stl_tree.h：GCC list/tree 的 header 嵌入对象，元素分别分配 `_List_node<T>`、`_Rb_tree_node<T>`；MSVC 使用独立 heap head。复用仓库既有 GCC 版本门禁与节点事实，不把 MSVC 固定 head 数套到 GCC。
- 目标实际 hashtable/hashtable_policy 实现：核实缓存 hash 的 node 类型、空桶的嵌入存储、prime rehash 策略和真实请求顺序。桶数由已审计库的策略产品推导，不发明素数表或通过试分配回读。
- 索引合同显式描述每容器固定请求数；共享 planner 同时用于正向定价和预算反解。IntervalCoverage 的嵌套 pending 容器及临时 move head 也按同一事实计费。
- generation hash 合同显式描述空表节点/桶请求与最终桶请求。共享 planner 按这些事实计算保留/峰值；resource 仅校验计划允许的请求与 readback，不自行选择布局或回退。

## 实施与验证

1. 完成目标头文件、链接 libstdc++ 身份及已有 helper 的逐项源码核验，记录确切文件/行与版本。
2. 修改内部 geometry 类型、两个 vendor adapter、index/generation planner 和 hash resource 的状态校验，保持 MSVC 数值及生命周期等价。
3. 两名未参与实现的智能体分别审查公式、零请求分支、rehash 重叠、溢出、反解一致性及 Windows 影响；修正后双方重审。
4. 本机提交，哈希保护同步目标，原 rk-release 8 核增量 all 构建；保留原失败与超时记录。
5. 原规格四路 realtime 复验，不以构建或容量诊断替代输出、失源恢复、内存和 A/V 验收。记录后按清单清理临时产物。

状态：9文件完整适配已通过两名独立审查者整批SOURCE PASS；目标机8核增量all构建通过，rk413及后续原规格四路realtime已越过原startup storage Unsupported门禁，但完整合屏验收仍未通过（见rk413、rk414验证记录）。RTP 64 包窗口与 UDP 链路问题保留独立证据，不用本适配掩盖。

## 已核目标源码依据

目标头根/opt/mt-tools/gcc-12.4-native/include/c++/12.4.0：c++config.h34/37确认release12/update20240620；stl_list.h234/453/517确认元素node与内嵌header；stl_tree.h216/562/665确认Rb节点与内嵌header；hashtable.h388–400确认空表逻辑1桶指向_M_single_bucket而不分配；213及48–54确认节点是否缓存hash由实际hasher traits决定；2533–2535确认rehash交给_Prime_rehash_policy::_M_next_bkt。独立调查已直接读取这些目标文件，无候选容器试分配。

实现范围：两个model storage plan、两个startup geometry、index/generation planner、HashMemoryResource h/cpp，以及必要的GenerationState geometry声明。固定head请求与可选初始桶进入内部事实合同；不新增CLI/C API字段。Pending上界为N(N−1)/2+(N+1)×head；hash保留最大元素节点与初始节点、按实际初始/最终桶重叠计峰值。MSVC值必须逐公式保持原结果。

## 实现检查点

内部index合同新增emptyHeadRequests，正向预算和反解共用同一事实；hash合同新增initialNodeRequests及optional initialBuckets。GCC节点取实际cache trait，调用真实prime policy推导final bucket，resource按事实进入Ready并校验rehash/seal。MSVC保留head1、初始8桶/双cell、旧峰值公式。两个validate均重新读取当前adapter几何，而非用传入计划自证。未改IndexMemoryResource、媒体状态机、共享对象分配helper及外部依赖。

第二审查直接复核目标hashtable.h2525–2545与/opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/hashtable_c++0x.cc：空表rehash先max(request,_M_bkt_for_elements(1))再_M_next_bkt，返回至少2且设置next_resize；因此随后插入不走未准备空表的首次增长路径。源码审查不代表构建或媒体通过。
