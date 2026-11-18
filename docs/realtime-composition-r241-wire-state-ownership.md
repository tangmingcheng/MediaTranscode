# r241：全局 wire 状态创建失败的所有权修复

## 证据与修改

网络预算绑定调查发现 `MediaWireGlobalSequenceState::create` 原先把裸指针交给 shared_ptr，控制块分配失败后 catch 再次 delete。同一指针存在重复释放路径。[C++ 标准 shared_ptr 构造规范](https://eel.is/c++draft/util.smartptr.shared.const) 第6段规定裸指针构造抛异常时删除指针，第29段规定从 unique_ptr 构造抛异常时不产生效果。

当前改为 try 内构造对象并立即交给 unique_ptr，再移动到 shared_ptr；不再手动 delete。构造声明与定义去除 noexcept，使成员容器分配失败能进入 AllocationFailed 返回路径。VS2026 本机 `MSVC/14.51.36231/include/deque:643–645` 的默认构造调用 _Alloc_proxy 且未声明 noexcept；共享代码也不能假定所有目标标准库的默认容器构造均不分配。

线程、投影顺序、reservation 门控、预算和媒体参数均未改变。该缺陷由源码与标准语义确认，没有运行内存故障注入，也不是 r239 超期的已证实原因。修复适用于 Windows/RKMPP 共用创建路径，没有新增平台分支。

## 验证

- 两名未参与实现的独立智能体均明确限定 PASS：对象构造失败、unique_ptr 持有以及共享控制块分配失败的释放所有权正确；错误类别保持 AllocationFailed。
- Release clean-first/all-target 构建会话8553最终 exit 0，configure/build exit 0；缓存、Ninja图及两个CLI产物已核实。realtime SHA256 C8A62F4DD921C47437F02F6525C7C4B87B1122D88B5BE4328A7DEB73873AE6B8。local未执行。
- 本轮未执行实流；不宣称异常分支实测或合屏验收通过。

## 后续

继续网络存储的 planner 几何、typed budget 和实际 owner 绑定。r240 仅完成公共分配器抽取，网络存储遗漏、r239 串行阻塞及完整合屏门禁仍未完成。

已定位可直接复用的精确存储：runtime/storage/MediaFixedCapacityStorage.h（元素容量固定、无扩容余量）与 MediaOwnedByteStorage.h（单次精确字节分配），两者均接受 PMR。网络接入应优先复用这些组件及 r240 的公共计费资源，避免用经验倍数猜测 std::vector 容量。实际接入尚未实施。
