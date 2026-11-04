# rk387：GCC 分配契约与增量依赖修复

## 本轮修改与审查

- reservation allocator 的共享控制块几何复用已有 typed probe，限定 GCC 12.4 / libstdc++ 20240620 / C++20 / 非 debug；使用真实 MediaSharedAllocationAllocator<Object>，不估算控制块。
- commit factory 复用相同能力查询，核对规划几何后再转移存储所有权。Windows MSVC 145/202604 的实际 allocate_shared 类型已独立核对。
- 两名独立审查者对两文件均明确 PASS。Windows Release 构建会话 28129 配置/全量构建退出 0；未执行 local CLI 测试。

## 增量构建暴露的问题

首次 RKMPP 8 核增量仅执行 8 个目标并退出 0，但这是证据不足的构建：新头文件时间为 15:07/15:08，wire buffer 对象仍为 14:39；ninja -t deps 显示 #deps 0，生成的 GCC 命令缺少 -MD/-MF。

根因是项目 CMakeLists 全平台清空 CMAKE_DEPFILE_FLAGS_CXX 和 CMAKE_CXX_DEPFILE_FORMAT。现已将原设置限定于 MSVC，GCC 恢复 CMake 官方 GNU compiler module 的依赖生成机制；两名独立审查者均明确 PASS。Windows 分支有效设置未变。首次恢复依赖会因命令变化广泛重编，仍未使用 clean-first。后续必须核对实际 rule、对象时间及非零头依赖。

第二次增量已在同一远程目录启动，工具会话 84715；最终退出码尚待收取。不能将首次 8 目标成功记为 GCC 头文件修复已验证。

源码同步包 rk387-source.tar SHA256：889c7dac45a049f7414185f2aea5ac78774607c67db3408f643399cf3867515d。本轮没有运行媒体测试，没有新增抓包或截图。

## 下一步主线与未关闭风险

rk386 的 SourcePreparer 失败仍存在。共享 producer facts 需要分别表达 Completed 与 CUDA event；完成事实不得与独立像素分配混为一谈。RKMPP decoder 输出借用 MPP surface，RGA tile 通过现有 filterExecution 形成独立输出边界；准备和运行时均需复用相同节点、引用计费及错误传播。

已核对部署 RGA 正常路径在 imsync 成功后发布，失败返回 AVERROR_EXTERNAL；仍须证明 async_depth=0 的 EOF/失败清理边界。MPP 的去交错及 no-thread 错误传播边界沿用 r253 记录，不得用非 CUDA 推导 Completed。没有修改外部 FFmpeg/MPP/RGA。

网络分配修复不等于源码帧门禁通过，也不证明其他 startup 容器门禁已解除；Windows r385 及双平台完整实流验收仍未完成。

依赖恢复现场复核：生成的 rules.ninja 已包含 -MD -MT $out -MF $DEP_FILE；已重编 MediaVideoPlanOptionApplier.cpp.o 的 ninja -t deps 返回 #deps 437（VALID）。因此依赖记录恢复已有直接证据，但整轮 792 个目标构建仍在进行。

补充：本地与远端 rk387-source.tar 已按精确路径删除并核对不存在；用户提供的源媒体和既有产物保留。远端已观测 8 个 cc1plus 并行进程，可用内存约 4543 MiB，未发现内存耗尽。

完成语义调整：Completed 表示受支持 FFmpeg API 成功交付可消费帧、无须引擎追加生产者等待，不表示底层所有驱动失败均被正确传播。独立审查 A 已明确认可该边界；MPP AVBuffer 借用与独立 RGA 输出仍须分别规划，不能提前释放。历史 MPP/deinterlace 错误传播风险保留，但不再把证明任意第三方无缺陷当作基础功能前置条件。
