# 合屏 opaque 保留契约（限定域设计已审查，待实施）

## 问题与范围

SourcePresentation 的 `queues.frame` 当前同时成为 decoder/filter 的 `video.lineage.capacity`。聚合器要求的 current＋successor 下界只能证明帧队列至少为 2，不能证明 codec opaque registry 足够推进。Registry 的槽位随最后一个 opaque owner 释放，输出 frame 的 unref 不会同步释放 FFmpeg DPB 原引用。

拟将 registry 容量作为独立内部 planner 产品：平台 capability adapter 提供版本限定的 opaque 同时持有上界，planner 加入提交/接收工作位并计费。不开公共参数，不修改 FFmpeg，不改变生产 DAG 或失败语义。帧队列仍按其自身时间与背压契约规划；未知 capability 在构建前失败。

## 本机版本证据（2026-11-09）

- 外部源码 HEAD：`20054712a242c54aa19d9ae9fca7a0381a2f1397`。工作树只有未跟踪 patch/recently_checked，相关 libavcodec 源码无 tracked diff。
- 工具报告：`N-125148-g20054712a2-g9cbd889670+3`，libavcodec `62.36.101`。版本字符串不能单独证明文件内容。
- 项目 `3rds/ffmpeg/bin` 与 `D:/mabs/local64/bin-video` 三个对应 DLL 的 SHA256 分别一致：
  - avcodec-62：`F720E06FB04379BF8187094B15FD84CA24B25C8567E2842B5457C5D31CFCE621`
  - avutil-60：`C14C5BB45AB95ACC6F992537C04E9561C9D9CDD627CE9CC6869B4BEEC42E2991`
  - avfilter-11：`D6B67A4870A21B330F1F2C143A8AF22B565A852C3B7BDD0CFA70E91E56976D0D`
- 以上是配置依赖一致性证据，不代表本轮执行了实流或核验了某个运行进程的已加载模块。

## 已审查的限定域上界

只审查 native h264/hevc 的软件路径与 native CUDA/NVDEC adapter；不包含 cuvid、RKMPP、自定义 get_buffer 或第三方后处理。单位是同时活跃的输入 token 种类；允许重复计算别名形成保守上界，不以解码输出帧数估计 token 数量。

| 产品 | 来源 | 候选数量 |
|---|---|---|
| H264 private frame 槽 P | DPB 36＋cur_pic＋last_pic_for_ec，各 f/f_grain；tf.f 为别名 | 76 |
| HEVC private frame 槽 P | 两层，各 DPB 32，每项 f/frame_grain | 128 |
| generic 槽 G | in_pkt、last_pkt_props、buffer_pkt、BSF.buffer_pkt、buffer_frame | 5 |
| H264 输出 FIFO B | 无独立 FIFO | 0 |
| HEVC 输出 FIFO B | 非空先消费，单次 decode 的 token 来源为原 private/generic 和一个新 packet | P＋G＋1 |
| 单 context C | private＋FIFO＋generic | P＋B＋G |
| 单轮 decoded_frames 唯一 token Q | worker 本轮至多取得一个新的 p->avpkt | C＋1 |

串行候选 registry 下界 `R=C+2`，另两位为引擎 pending 输入与 receive 输出。实际 frame-thread 数 T 大于 0 时，候选为 `R=(T+1)*(C+Q)+T+3`：parent＋T workers 保守各计 context 和输出批次，另计 T 个 worker packet、main next_pkt 与引擎两个工作位。只有已打开 context 的 active_thread_type 包含 FF_THREAD_FRAME 时才使用其 thread_count 作为 T，否则使用串行公式；不能用请求的 thread_type 或用户估计代替。所有乘加须做溢出校验。

能力门禁必须证明 LCEVC 未启用：decode.c 的可选 LCEVC 路径会在 lcevcdec.c 克隆输入 frame 并交给外部库延迟释放，超出上述容器模型。本机 configure 将 liblcevc_dec 列入默认禁用的 EXTERNAL_LIBRARY_LIST，工具配置没有 --enable-liblcevc-dec；未来 adapter 必须限定匹配版本/配置并明确拒绝该扩展，不能只检查 codec 名或 major。两种 native codec 未声明私有 .bsfs，默认 null BSF 才支持 G 中的单包槽计数。

双独立审查已检查 decoded_frames 交换、异常批次、默认 BSF 和 NVDEC opaque 所有者，均给出限定域设计 PASS；版本/配置门禁及实际 reopen 一致性仍须在实现中落地。尚未把上述数量加入代码。

## 可核验源码位置

本机 `D:/mabs/build/ffmpeg-git/libavcodec/`：decode.c 的 COPY_OPAQUE 与 BSF、internal.h 的 generic owners、h264dec.h 的 DPB/临时 picture、h264_slice.c 的 thread picture 复制、hevc/hevcdec.h 与 hevc/ps.h 的层和 DPB、hevc/refs.c 的 FIFO 入队、hevc/hevcdec.c 的先消费 FIFO、pthread_frame.c 的 worker decode 循环及主线程交换 decoded_frames。

项目：`MediaCodecLineageRegistry.cpp` 的最后 owner 归还槽位；`VideoDecodeNode.cpp` 的输出 opaque 清除；`MediaRealtimeCompositionSourceResourcesPlanner.cpp` 对 registry 与 queues.frame 的现有等值要求；`MediaPreparedSourceCandidateCostPlanner.cpp` 与 `MediaSourceLineageStoragePlanner.cpp` 的费用路径。

## 接线要求

1. 准备期读取并保留 typed opaque 契约，实际 decoder reopen 时再次核对；不把 compressed packet retention 复用成 opaque 事实。
2. registry 容量独立进入候选、选中快照、sealed 比较、最终 options 和费用核对；不能只修改运行时 options。
3. filter 使用其独立 ImmediateSingleOutput/retained input 能力与节点工作位，不把 downstream startup Nv 重复计入 registry。
4. RKMPP 保留既有 COPY_OPAQUE 能力门禁；容量增大不能修复缺失的标识传递。
5. 双独立源码审查与两平台构建后，仍须原规格 realtime 实流证明。本文件不代表合屏或候选求解器完成。


## 审查与当前结论

lineage_capacity_audit完成源码推导；另两位未参与推导的审查者timing_handoff_review_a、timing_handoff_review_b独立核验后均给限定域设计PASS。两者提出的LCEVC排除和actual active_thread_type要求已写入。没有以单次输入输出一一对应为前提；[FFmpeg COPY_OPAQUE官方说明](https://ffmpeg.org/doxygen/trunk/group__lavc__core.html)明确允许多对多及无输出，正是需要按引用生命周期计费的原因。

推导结果：无frame线程H264为83、HEVC为269；有T个实际frame workers时分别为164*T+166、536*T+538。仅是registry唯一token容量上界，不是AVFrame数量、payload、frame pool或全会话内存上界。版本身份由root本轮git只读核验，子审查者仅能读源码。未改生产代码、未运行媒体测试、未修改外部依赖，也未新增测试产物。
