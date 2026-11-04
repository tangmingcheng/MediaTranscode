# r279：复用已准入的待发布分区存储

## 依据与范围
r278重入失败仍由整笔serial等待触发。解除前必须继续闭合存储与取消边界。本项仅删除两个materializer在成功准入后建立的普通deque，不修改serial、时限或协议算法。

GStreamer GstBufferList采用分组buffer所有权与索引访问，预分配可避免列表扩容：https://gstreamer.freedesktop.org/documentation/gstreamer/gstbufferlist.html 。本项目已有更严格的固定容量MediaWireDatagramBatchCollection及实际分配计费，应直接转移该集合，不能再复制到另一个容器。这里复用所有权原则，不宣称采用GStreamer运行时或获得其性能证明。

## 实施合同
- 共用待发布所有者同时服务MPEG-TS和raw RTP节点；保存既有集合或单个terminal report，禁止覆盖未发布输出。
- 普通输出整体move，复用FixedCapacityStorage的front/pop_front，逐项发布成功后立即释放该项引用；最后一项消费后释放集合backing，归还实际存储credit。
- 单个RTCP terminal以shared_ptr直接持有，不为一项再分配容器。两种状态互斥，null/空输入报错。
- WouldBlock保留集合与当前项；purge仍先授权global generation purge再释放，stop/abort沿原生命周期。无新增锁、线程、队列上限或平台adapter。
- collection费用继续来自r272同笔准入及ledger；不重复计费。不代表generation bootstrap、UDP工作区、raw packetizer工作区等其余存储已闭合。
- 实施后双独立审查、全量Release重建与原规格realtime验证。失败必须归档清理，不能将局部静态通过作为合屏完成。

## 取消链调查
a只读审计确认：两个materializer把publication reserve的Cancelled转换为WouldBlock，保留对象等待owner-thread purge；purge先authorizeGenerationPurge再cancelPendingOutputTransfer，stop/abort发生在全图停止join之后。所查global/registry/wakeup路径无逆序取得RTP锁。此结论仅关闭该具体取消路径，不覆盖所有transaction析构路径。

状态：已实施；a与b2独立局部静态审查均PASS。git diff --check通过，全量Release构建20304成功，configure/build均0，四项必需产物存在；realtime SHA256：5E278491F4C3396A0614B4040BBA3C38288C8B079C45472ABE3942750C039CF7。未运行local，尚未实流。

## 解除serial前剩余项（b2只读核验）
- raw RTP packetizer callback的嵌套payload数组、payload计数和随后views未使用network resource，累计收集量缺硬边界。需由真实packetizer与已批准atomic envelope证明容量；有状态packetizer不能假定可运行两遍。
- UDP protocol views与三个准备数组仍未同笔计费；应复用两遍Sizing、固定数组及同笔准入，ledger增加真实UDP footprint。
- 通用emitOutput targets与PendingTransfer channels仍有普通分配；r279不覆盖。
- generation Data、global state及身份/endpoint/waiter对象需按实际存活重叠核算。它们不是每笔预约都会新增，不将其称为r278超时直接原因。
- 已核验存储预算在同锁内arm，实际释放额度后锁外通知；非serial的global部分prefix提交也通知。未发现独立可证漏唤醒，但仍需证明等待期间释放者可继续运行及旧owner最终释放。

以上是未完成项清单；不得因r279完成就放开serial或声称整体预算闭合。

## r279b 后续实流证据
同一 Release 产物完成三路 120 秒输入、全失源黑屏、所选源 120 秒重入及再次失源，VLC 原生快照已观察。CLI 调用方停止返回 0，errors/deadline_misses/pressure_failures 均为 0；本轮 17 个临时文件及六个进程均已清理核验。详见 realtime-composition-r279b-validation.md。该单轮结果不证明历史偶发超时根治，delivery_evidence 仍为 not_proven；以上资源与背压未完成项保持开放。
