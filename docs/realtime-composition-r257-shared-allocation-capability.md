# r257：共享事务对象的分配几何能力

状态：探测器与分配布局校验allocator已实现，尚未接入生产create或预算，不代表实流通过。

## 当前实现证据

- `MediaDatagramCommitTransaction::create` 使用普通make_shared，控制块尚未计费。
- 本机工具链：MSVC 14.51.36231，`_MSVC_STL_VERSION=145`、`_MSVC_STL_UPDATE=202604L`。
- 本机include/memory 3033–3047：非数组allocate_shared的控制块类型只依赖对象类型和allocator类型；先调用allocator分配，再forward构造参数。
- include/xmemory 1178–1185：分配前清空临时指针；allocate抛出时不释放不存在的分配。
- include/memory 2504–2544：allocator保存在控制块中；对象销毁与最终控制块释放分离。
- [Microsoft STL官方源码](https://github.com/microsoft/STL/blob/main/stl/inc/memory)提供同类实现参照；最终依据是本机上述版本，不能以main分支替代当前工具链证据。

本机文件SHA256（均位于`D:/VisualStudio2026/VC/Tools/MSVC/14.51.36231/include/`）：

| 文件 | SHA256 |
| --- | --- |
| memory | 8955101828FE9D46C77E214E50415BAC5B1E59697E0F59BC46FA145AA1E8AE4C |
| xmemory | 0409C9668D5BAE4ABEECEEEF3EE29353C1F4039DC5A78565ACFE1CB34D442F38 |
| yvals_core.h | 10B59D42BDE2105E76CF16735E34DD0F1D45A943CB1B789C4A0406C67C2BD684 |

## 选定实现方向

保留一次合并分配，不改Model构造函数、不构造假Reservation。准备阶段与生产使用同一allocator具体类型，scope分别为Probe/Admitted。Probe在rebind后的allocate入口检查乘法溢出、记录真实bytes/alignment，随后抛不分配内存的专用bad_alloc派生标记；仅该精确标记与完整记录视为探测成功。

探测传入可转换为Reservation的占位参数；转换必须非noexcept，若真正执行则返回“不支持构造顺序”，不能terminate。普通bad_alloc、提前构造、缺失记录均为失败。此能力探测是生产准备流程，不修改Model以适配测试。

生产allocator值持资源与geometry，每次核对完整布局；单次分配由已审MSVC非数组allocate_shared源码保证，不声称运行时拒绝第二次同布局请求。PMR仍负责实际header/alignment计费和资源寿命，控制块弱引用残留期间额度不能归还。探测器与生产allocator类型必须相同，不能使用尺寸不同的counting allocator。

## 适用边界与剩余门禁

首次allocate中止只能观察第一笔请求，单次分配证明来自指定标准库源码，不能声称这是C++标准保证。RKMPP必须核验实际libstdc++/libc++实现并重新探测，不能复用Windows数值。布局不符必须在协议投影前失败，或遵循现有明确generation abort语义。

A/B2两位独立设计审查及实现静态复审均已通过本机限定切片。typed geometry、探测入口和分配校验已实现；准备期调用、生产接线与整笔存活存储预算仍待完成，当前串行门控不能解除。

## 实施进度

已新增typed geometry（count/bytes/alignment）及同类型allocator。Probe与Admitted使用同一scope variant；共享state仅用于同步准备期探测，生产分支值持resource与geometry。rebind及最后weak控制块保留资源，不指向调用栈。初始allocator构造仅向事务工厂与探测函数开放；分配入口检查乘法溢出及布局，释放入口核对原布局。探测只接受本state专用标记及完整记录，普通bad_alloc、重复请求、触达占位转换均不视为成功。

真实RTP与MPEG-TS/UDP事务Model各有非模板探测入口，确保编译其模板路径；没有替换Model或构造假Reservation。能力门禁为当前已审MSVC STL 145/202604，其他标准库暂返回Unsupported。

A/B2两位独立实际静态复审均PASS；Release全量构建通过（会话54151，configure/build退出码均为0；已核验CMakeCache.txt、build.ninja及两个CLI产物，未运行local CLI）。尚未执行probe取得数值，也未将生产make_shared切换至该allocator。上述54151构建对应简化前实现；后续已移除生产共享状态，避免递归bootstrap计费。资源及预算对象自身、Model控制块、整笔存活存储额度仍须接通；不能把本切片称为控制块已经计费。

## 生产分配状态简化

两位独立审查均确认：在已审工具链与封闭调用入口内，无需每笔堆建共享计数状态，也无需新增固定槽位。总额度不能证明请求次数；单次请求的依据仅是上述STL实现。已实施值持资源方案，四文件实际静态复审均PASS。allocator类型布局已改变，后续必须重新探测，不得复用旧geometry。Release全量重建通过（会话16653，configure/build退出码均为0；已核验配置文件及两个CLI产物，未运行local CLI）。生产create仍使用make_shared，serial仍保持。

生产调用现已改为prepare/bind两阶段，三处入口在协议投影前用普通make_shared准备真实空态Model，投影后无分配绑定Reservation。对应probe改为真实Model零参数构造。该顺序调整经双审及Release全量构建13803通过；仍未切换预算allocator，详见r257主计划。
