# 解码输出持有与进展证据（r228）

## 结论与限制

2026-10-15，两名独立只读审查均限定PASS：在本机所查FFmpeg实现、实际CUDA输出、native未启用UNSAFE_OUTPUT且未注入外部固定输出池的条件下，下游持有公开输出帧不会一直占用固定NVDEC解码表面。完整硬件进展验收仍FAIL；这不是显存容量、无分配失败、其他平台或HEVC真实链路的保证。

## 实现与部署证据

- 本机源码HEAD为`20054712a242c54aa19d9ae9fca7a0381a2f1397`；针对下列五个文件的git diff为空。工具版本为`N-125148-g20054712a2-g9cbd889670+3`。源码身份与工具版本不能单独证明可重复构建。
- `D:/mabs/build/ffmpeg-git/libavcodec/nvdec.c`：315行real frames context初始池大小为0；531–571行映射buffer为只读，safe路径调用`av_frame_make_writable`。`libavutil/frame.c`重新申请硬件帧并复制，`libavcodec/decode.c:719`在输出前释放private_ref，不能遗漏该内部引用的解除。
- `libavcodec/cuviddec.c:619–647`独立申请CUDA输出并复制，758行unmap；`libavutil/hwcontext_cuda.c`输出池调用cuMemAlloc。native与cuvid须分别核验。
- mabs、仓库`3rds/ffmpeg/bin`、Release目录的`avcodec-62.dll` SHA256均为`F720E06FB04379BF8187094B15FD84CA24B25C8567E2842B5457C5D31CFCE621`。这证明三个部署副本一致，不替代源码到二进制的构建溯源。
- [FFmpeg官方nvdec源码](https://ffmpeg.org/doxygen/trunk/nvdec_8c_source.html)已有opaque输出分支，本机nvdec.c没有；不能把trunk行为套到本机二进制。[官方cuvid源码](https://ffmpeg.org/doxygen/trunk/cuviddec_8c_source.html)用于对照复制/释放策略，具体行号以本机文件为准。

## 仓库复用点与下一步契约

`MediaHardwareCapabilityProbe::validateOutputBranch`已有CUDA独立输出判断；`CodecResolverDecoderContextBuilder`回读decoder name/flags，只注入hw_device_ctx，未设置decoder hw_frames_ctx或extra_hw_frames。`MediaRtpSourceFirstFrameProbe::restartDecoder`释放探测帧及旧codec，仅保留device，重开后核对name、flags、线程、timebase和设备身份。Encoder的固定池不是decoder池证据。

将上述判断收敛为共享能力产品，并由实际firstFrame的CUDA格式、hw_frames_ctx设备/格式及实际decoder flags形成证据；SourcePlanner消费该产品，重开/claim继续核对身份。不新增媒体链路、线程或外部输入，不修改FFmpeg依赖。未知后端、UNSAFE_OUTPUT、opaque输出或无法证明外部池余量的路径应在规划阶段报告缺证据，不在节点fallback。固定DPB隔离只解决一种等待机制；预算失败、控制终止和最小工作集必须继续分别验证。

## 非cadence路径的剩余具体依赖

1. `MediaPreparedRtpSourceSessionPlan.cpp`仅cadence分支形成continuousRetention；SourcePresentation目前只有共享字节预算与reference数量上界。这不等于可直接分配的decoded startup队列容量。
2. `MediaRealtimeGraphResourceLedgerPlanner::plan`仍以surface productionRate计算队列；不能用输出fps或伪造输入fps补齐。
3. `MediaRealtimeCompositionSourcePlanner`仍拒绝非cadence并要求continuousRetention。需要先由准备存储、实际分配费用、源归属和最低进展条件形成类型化数量产品，再复用同步/队列planner，最后开放入口。

本轮仅调查及更新计划，无生产代码、构建或实流修改，无测试临时产物。r227原规格运行证据不扩大为上述新能力验收，整体仍75/FAIL。
## r229：共享输出分配能力产品

新增MediaDecoderOutputAllocationAdapter，以实际decoder name、hwaccel flags、open前是否提供hw_frames_ctx，以及已返回frames context形成可选类型化产品。仅为明确h264/hevc native safe或h264/hevc_cuvid、实际CUDA格式/设备、initial_pool_size=0且未提供外部池的路径提供独立复制证据；其他路径不产生该证据。SourcePreparer在真实首帧后保存产品，SourcePlanner核对name和descriptor；动态输出原局部判定复用adapter，IndependentFilterOutput仍按原独立滤镜证据判定。Replay新增外部frames事实一致性核验。

自查发现公共frames validator只确认device_ref存在，已补实际AVHWDeviceContext大小和CUDA type检查；两名独立审查员对修订版重新明确限定PASS。Release全量重建会话18267退出0，718项构建计划及必需产物检查通过。realtime CLI 6043648字节，SHA256 A977887C6155A0660C67E3967E123EDF1263617E47441C6B89924AB8448B4562。未新增实流结论，r227不能直接作为本版运行证据；非cadence门禁仍关闭，完整进展验收未通过。

容量接线另已确认：sharedCapacityBytes/sizeof(MediaBufferRef)仅为逻辑bookkeeping上限；budget.snapshot的capacity-retainedBytes不是原子预留，多源重复读取会重复承诺。后续须在会话级汇集全部源，分别规划压缩AU acquisition、解码frame startup及queue slots，并用实际lease保护分配；不得直接预分配该极宽逻辑上限，不得用短前缀样本或输出fps伪造未来输入上界。本轮无测试临时产物。
