# r252：实际像素分配的就绪所有权

## 目标与依据
沿r249失败与r250已核验的NVIDIA事件语义、GStreamer memory/stream所有权原则，逐帧readiness必须随实际AVBufferRef保留。仅外层FFmpegFrameBuffer成员或独立extended_buf token不能覆盖codec内部只保留数据buf的情况。复用MediaFfmpegPayloadOwnership事务性包装，避免第二套同义像素owner。

## 本切片合同
- readiness owner不反向引用包装后的frame/buf；每个实际allocation wrapper持有原allocation、共享completion和credit，保持单向所有权。
- 在生产者首次对外发布前绑定；已存在的pool内部引用不会被追溯改写，本包装保留自身原allocation引用。
- 每个wrapper释放原allocation前尝试共享幂等completion，随后才释放原allocation和credit。显式完成接口保留错误；析构路径不抛出并诊断。
- 新completion相关owner/controlblock实际使用显式已有PMR；不新建无计费allocator。准备FramePtr复用同一机制。
- 原packet/frame credit路径复用同一实现，不扫描未知av_buffer_get_opaque或覆盖opaque_ref。
- 尚未接CUDA capture与canvas event wait，不能声称本切片修复超期。

## 驱动失败语义边界
本机libavutil/buffer.c的free callback为void；它不能向调用者返回Status。同步成功可以证明覆盖工作完成；同步失败只能保留/记录错误，不能证明设备完成。沿现有CUDA adapter错误处理边界，不新增重试、隔离保留队列或进程终止策略。不得将失败释放路径描述为驱动故障下的强安全保证。新路径显式capture/batch阶段应在发布前传播失败，析构完成仅作为最后防线。

## 验证
实现冻结后双独立审查，Release全量构建及原规格realtime实流。完整目标仍含四源HEVC输入重排、非cadence、内容A/V/静音、API/ABI及Windows后RKMPP。

## 冻结审查记录
两位未参与实现者 r233_boundary_review_a、r233_boundary_review_b 均明确限定 PASS：实际 allocation 引用、事务替换、析构顺序、一次完成结果缓存、PMR 生命周期及四处既有 caller 静态核验通过。当前 caller 的 completion 仍为空。新增 FFmpeg 自身 AVBuffer/AVBufferRef 分配不属于已证明的 PMR 计费范围；后续非空 completion 接线必须使用实际预算资源，operation 本体也须计费。
Release 全量重建会话 89708 退出 0，configure/build 均为 0；CMakeCache.txt、build.ninja 及两个 CLI 产物存在。realtime CLI 为 6076416 字节，2026-10-16 16:03:26。未执行 local CLI；尚未进行本切片实流复验，构建不代表媒体验收。

## 下一完整接线范围
生产节点成功取得输出后、首次发布前 record；实际 allocation 保留完成保护，FFmpegFrameBuffer/FFmpegFrameView 显式携带 typed readiness；准备期首帧、filter tile 与运行期共用机制；canvas batch 接收帧及依赖并执行事件等待，批末保持完成确认。新分配与 COW 不能继承旧事件作为唯一证明。planner 以真实设备、生产操作和 capability probe 建立内部合同，缺证据构图前失败，不新增外部参数或 FFmpeg 修复范围。
