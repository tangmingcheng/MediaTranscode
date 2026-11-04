# r239：消费已预留的准备存储额度

## 契约

沿用[共享容量调查](realtime-composition-shared-capacity-design.md)与其中Flink独占credit/共享余量/所有权归还依据。现child capacity只限制最大占用，不能保证多源同时获得各自额度。本切片新增由实际lease支撑的预算，避免把snapshot剩余额度当承诺。

- fromReservation消费一个正数、独占、已准入lease；新预算容量等于lease字节数，保留lease直到自身最后一个持有者释放。
- 后续reserve与既有PMR allocator消费本地counter，不再次向原父预算收费。分配释放归还本地额度；预算销毁才归还父额度。没有增加媒体拷贝、线程、队列或公开参数。
- 失败和异常由RAII回收已准入lease；分配不能超过本地容量。新预算使用本地mutex，分配时不嵌套获取父锁；父额度释放发生在预算析构时。
- 将输出encoder及canvas的既有planner准备容量接到此路径，真实准备函数消费同一预算。保留完整保证额度可能比按实际使用即时归还占用更多共享空间，这是显式保证的成本，不伪称RSS或VRAM增长。

## 限制及下一步

这还不是全部源必需工作集的一次性联合准入；输出和canvas仍依次取得保证。尚未给非cadence decoded startup定义完整收费与有限count，也未解除任何入口检查。PMR费用、runtime像素和compressed AU仍保持原有范围，不能拿本额度除像素大小推帧数。

接下来须审查冻结源码、全量Release重建并以原规格realtime验证成功和错误收尾；随后将会话级必需额度产品与source allocator联合接线。Windows完整门禁前不进入RKMPP。当前没有运行验证结论。

## 审查进度

两名独立审查者均限定PASS，确认实际lease消费、无重复root收费、所有权和异常路径。root retainedBytes包含独占但未使用的保证额度，已在snapshot接口注明，不代表活跃分配字节。Release全量构建会话24549进行中；无实流结果。

Release全量重建24549已exit0，configure/build均0；必要CMake/Ninja与两CLI产物存在，未运行local CLI。realtime SHA256=64F5E6F02C0C502EF6C39445251FE04468BB7B834A04201EE4795A905942F265。尚待原规格实流验证，不宣称资源准入验收完成。

r239原规格实流已执行：准备成功、共享额度832927224/2147483648、三格VLC快照成立；约99秒因串行reservation门控后物化超期exit1，完整FAIL。三源120秒exit0，失败清理归档及12件产物删除完成。该失败未证明保证额度逻辑错误，也不能记其完整验收通过。详见realtime-composition-r239-validation.md。
