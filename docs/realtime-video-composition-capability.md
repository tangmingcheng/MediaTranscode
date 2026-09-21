# 合屏依赖能力与失败语义门禁

## 已核实的部署

| 平台 | 实际库与源码 | 合成能力 |
|---|---|---|
| Windows | `3rds/ffmpeg/bin/avfilter-11.dll`，11.17.100；`D:/mabs/build/ffmpeg-git`，`20054712a242c54aa19d9ae9fca7a0381a2f1397` | `overlay_cuda`、`scale_cuda`、`overlay_qsv`、`xstack_qsv` 存在 |
| RKMPP | `ffenv on` 后 `/home/tang/ffmpeg-ab1e61a/bin/ffmpeg`，11.14.102；源码 `/home/tang/ffmpeg-rkmpp-close-ownership`，`ab1e61adaa21ff129caa8e01a1198156044567ed` | `overlay_rkrga`、`scale_rkrga` 存在 |

Windows 源码版本对应生产版本 `N-125148-g20054712a2-g9cbd889670+3`。仓库与 `D:/mabs/local64/bin-video` 的 `avfilter-11.dll` SHA256 相同：`D6B67A4870A21B330F1F2C143A8AF22B565A852C3B7BDD0CFA70E91E56976D0D`。此为源码/版本一致性检查，不是可重现构建证明。

设备：Windows RTX 4060 Laptop GPU，driver 610.62，8188 MiB；RK RGA driver 1.2.23，两个 RGA3 核及一个 RGA2 核。RGA3 输出范围 68×2 至 8128×8128、缩放 1/8 至 8、stride 对齐 16；720p 格子尺寸处于其范围，但多路吞吐尚未验证。

## Windows CUDA：异常传播静态风险，运行能力待验证

实际 `libavfilter/vf_overlay_cuda.c` 中 `overlay_cuda_call_kernel()` 返回 `cuLaunchKernel` 检查结果，但 `overlay_cuda_blend()` 的各平面调用不检查该返回值，随后仍调用 `ff_filter_frame()`。因此 kernel 启动失败可以未传播到调用方；该结论来自源码，不声称已运行复现。

此外，滤镜返回并不证明 GPU 完成；使用 main CUDA stream，没有核验两输入 CUDA context 相同。`ff_inlink_make_frame_writable()` 可能分配并复制整个画布。planner/adapter 必须核实设备一致性、上游完成依赖、输出完成和资源保留边界。

不能以调用滤镜后增加一次同步来声称补回所有已被忽略的立即启动错误。参考 [CUDA Driver API 错误与启动语义](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__EXEC.html) 与 [FFmpeg overlay_cuda](https://github.com/FFmpeg/FFmpeg/blob/9cbd8896708050f12ccfcb1dd9aefda9e57dc349/libavfilter/vf_overlay_cuda.c)。

## QSV 替代调查：本次尚未关闭门禁

现有 scanner 支持 QSV，生产 overlay/xstack 共用 `qsvvpp.c`。通常路径会返回 VPP/sync 错误，但实际源码 968～978 行在 EOF 排空同步失败后仅 warning，仍输出帧；DEVICE_BUSY 与 IN_EXECUTION 等待也没有总体截止时间。不能据滤镜存在或 `async_depth=0` 把 QSV 宣告为已完成等价验证的替代。

## RKMPP：机制可复用，运行证据待补

部署 `rkrga_common.c` 的 overlay secondary 仅支持 RGB，因此需要由 planner 组合既有 RGA 缩放/颜色转换节点；不可直接拼接两路 NV12。`async_fifo=async_depth+1`，src/dst/pat 资源保留至 `imsync` 完成。framesync 每输入 current/next 与滤镜边队列需额外计量。版本查询时有 `mpp_platform: client 12 driver is not ready!`，尚不能将其归因于合成失败或判定无影响。

生产 Windows 公开 `AVFilterGraph.max_buffered_frames`，可限制滤镜边队列；它不包含 framesync 已取出帧或硬件在途资源。RK 对应 API 和生产 CLI 最终动态链接路径仍需核实。

## 依赖范围与证据更正

2026-09-15：用户明确不同意扩大到 Windows FFmpeg 依赖修复。保留现有依赖，不修改外部 FFmpeg 源码、不重建或替换 DLL。原先把静态异常传播风险认定为合屏实施阻塞、要求先修依赖的结论撤回；没有当前环境启动失败的复现证据。GPU 异步提交本身不是缺陷，同一 stream 的顺序依赖应结合实际所有权核实。

已查询官方文件提交历史及邮件记录，尚未找到匹配此 kernel 返回值问题的官方修复；不能据此断言官方不存在 issue。保留上述静态风险，不将正常运行诊断等同于异常路径已验证。按原计划验证现有依赖并推进生产 DAG；不得增加吞错包装或降低失败语义要求。

## 初次部署调查命令与证据边界

Windows 执行 `D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -h filter=overlay_cuda`、`C:/Windows/System32/nvidia-smi.exe --query-gpu=name,driver_version,memory.total --format=csv,noheader`，并直接加载生产 DLL 查询 filter registry/version。

RK 通过已有 SSH 执行 `ffenv on`、`command -v ffmpeg`、`ffmpeg -hide_banner -version`、`ffmpeg -hide_banner -h filter=overlay_rkrga`、源码 `git rev-parse HEAD`/`git status --short` 及 `/sys/kernel/debug/rkrga/driver_version`、`hardware` 读取。会话已退出，未安装工具或启动真实媒体流。

## 2026-09-15 Windows 现有依赖运行诊断

范围：两次独立读取同一 120 秒连续 MP4 源，H.264 1280×720、30 fps、8.013 Mbps；CUDA 硬解、两路 scale_cuda、两级 overlay_cuda、NVENC H.264 CBR 8 Mbps，输出 MPEG-TS 文件 1280×720、30 fps，复制第一路 AAC。不是 RTP 输入，也不是生产 DAG 验收。

实际命令（PowerShell，直接执行，无测试脚本）：

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -n -init_hw_device cuda=cu:0 -filter_hw_device cu -re -hwaccel cuda -hwaccel_device cu -hwaccel_output_format cuda -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -re -hwaccel cuda -hwaccel_device cu -hwaccel_output_format cuda -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -f lavfi -i "color=c=black:s=1280x720:r=30:d=120,format=nv12" -filter_complex "[0:v]scale_cuda=640:360:format=nv12[left];[1:v]scale_cuda=640:360:format=nv12[right];[2:v]hwupload[canvas];[canvas][left]overlay_cuda=x=0:y=180:shortest=1[mid];[mid][right]overlay_cuda=x=640:y=180:shortest=1[out]" -map "[out]" -map 0:a:0 -c:v h264_nvenc -rc cbr -b:v 8M -maxrate 8M -bufsize 16M -c:a copy -progress D:/Code/MyCode/MediaTranscode/out/acceptance/cuda-composition-diagnostic-20260915.progress -f mpegts D:/Code/MyCode/MediaTranscode/out/acceptance/cuda-composition-diagnostic-20260915.ts
& D:/mabs/local64/bin-video/ffprobe.exe -v error -count_frames -show_entries stream=index,codec_name,width,height,r_frame_rate,start_time,duration,nb_read_frames -of json D:/Code/MyCode/MediaTranscode/out/acceptance/cuda-composition-diagnostic-20260915.ts
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -v error -nostdin -n -ss 60 -i D:/Code/MyCode/MediaTranscode/out/acceptance/cuda-composition-diagnostic-20260915.ts -frames:v 1 D:/Code/MyCode/MediaTranscode/out/acceptance/cuda-composition-diagnostic-20260915.png
```

结果：合成进程 PID 14676，源自然结束，exit 0；3600 帧、120 秒、30.10 fps 处理速度、dup/drop 均 0。全文件 ffprobe exit 0，确认 H.264 1280×720、30/1、3600 帧；AAC 5169 帧。60 秒位置抽帧可见等比左右两格及上下黑边；随机 seek 抽帧命令 exit 0，但报告一次 `co located POCs unavailable`，不据此宣称全程画面无异常。

进程采样（约 19/26/59/102 秒）：WorkingSet 318021632/318541824/320528384/322400256 字节，PrivateMemory 621670400/624291840/626200576/628568064 字节，累计 CPU 1.1875/1.421875/2.46875/4.03125 秒。仅为短时采样，不能判定长期无增长；未采集持续 A/V 漂移，未执行 VLC 播放、realtime CLI、RTP/RTCP、断流恢复或 RKMPP 验收。

结论：现有依赖在上述正常路径成功启动并完成合成，未复现 CUDA 启动错误。该结果不覆盖异常传播风险，也不是合屏功能完整验收 PASS。外部源码及 DLL 均未修改。原计划的多源绑定、时钟域、缺口恢复、资源准入和 CLI/C API 实施仍待完成。
## 2026-09-21 黑帧画布：只读调查与最小接入边界

本节为源码与元数据调查，未构建、未运行媒体链路，未修改外部依赖。`HardwareTransferNode.cpp:213` 的 Upload/Map/Unmap 仍未实现；`MediaEncoderPacketLayoutCapabilityProvider.cpp` 的清零探针不能证明黑色。正确像素由 [FFmpeg av_image_fill_black](https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html) 按实际软件格式与有效 color range 生成，不能以全零代替。

### 完成语义与构建条件

- 本机部署源码 `D:/mabs/build/ffmpeg-git/libavutil/hwcontext_cuda.c:238` 逐平面调用 `cuMemcpy2DAsync`，只有下载分支同步；错误跳转后最终仍返回 0。上游当前 [CUDA 实现](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavutil/hwcontext_cuda.c) 返回错误并增加部分失败清理，与部署不同。外层同步不能补回已丢失的立即复制错误。
- 可实施方向是生产画布内部统一 CUDA adapter：复用官方逐平面 `CUDA_MEMCPY2D` 算法，所有 CUDA 版本均显式检查 context push、每次复制、stream synchronize 与 context pop；不按版本 fallback，不改外部 FFmpeg。目标指针及 stride 来自实际 pool frame，保留 YUV420P 的 U/V 特殊布局。使用公开 `AVCUDADeviceContext.cuda_ctx/stream`，不读取私有 internal；借用 stream/context，以 AVBufferRef 保持生命周期，不销毁借用资源。host staging 可由 `cuMemHostAlloc` 有界分配；已提交操作完成前不得释放，任何失败都不能发布画布。依据 [NVIDIA Driver API](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MEM.html)。
- 实际 CMake 使用 `3rds/ffmpeg/include`、`3rds/include`，其中没有 CUDA/ffnvcodec 类型头；`D:/mabs/local64/include/ffnvcodec` 虽存在，但不在当前构建接口中。需正式发现已安装头并按能力配置 adapter，不能硬编码本机绝对路径，也不能把只在本机找到头称为构建通过。RKMPP 构建不应依赖 CUDA 头。
- [RKMPP 上游源码](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/master/libavutil/hwcontext_rkmpp.c) 上传为 map WRITE/OVERWRITE、CPU copy、unmap；缓存同步在 unmap，正 initial_pool_size 由 allocator 限制。该结论尚未在本节核对远程部署对应文件，不能直接作为部署 completion 证明。CUDA 普通 pool 的 initial_pool_size 仅预分配，不能作为硬上限。

### 色彩范围缺口

本次直接执行以下元数据命令，exit 0；源为 H.264、1280×720、yuv420p，color_range/color_space/color_transfer/color_primaries 均 unknown：

```powershell
& D:/mabs/local64/bin-video/ffprobe.exe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt,color_range,color_space,color_transfer,color_primaries -of default=noprint_wrappers=1 D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4
```

生产 `builder/codec/CodecResolverEncoderContextBuilder.cpp:236` 仅复制源 codecParameters.color_range；`planner/capability/MediaVideoEncoderReadback.cpp:36` 记录 context 原值。部署 nvenc.c 的 H.264/HEVC 配置将 JPEG/YUVJ 映射为 full-range，其余为非 full-range，但不回写 context，不能把 unknown readback 视为明确范围。

权威输入应为同一 prepared 编码配置真实生成的 SPS/VUI：有 GLOBAL_HEADER 时 NVENC 经 nvEncGetSequenceParams 生成 extradata；无 GLOBAL_HEADER 时须从已有 capability 编码探针产出的首个参数集取得，不能为探测改变生产编码选项。`protocol/codec/MediaVideoNalUnitScanner` 可复用 NAL 分割，`protocol/rtp/MediaH264SpsCodedSizeParser` 与 `MediaHevcSpsCodedSizeParser` 目前只解析尺寸，未到达 VUI；RandomAccessAdapter 读取 GOP/refresh，不能复用为范围解析。需以官方语法扩展统一参数集事实解析，并区分显式 full-range 与经完整语法确认的规范缺省。不可因解析失败或缺少参数集就推定 limited。

公开 av_parser_parse2 不导出此范围事实。公开 decoder readback 也不能统一补齐：部署 HEVC decoder 在 signal flag 缺省时设置 MPEG，H.264 decoder 仅在该 flag 存在时更新范围，仍可能 unknown。FFmpeg CBS 的规范推导可作算法依据，但它是内部接口，不可作为已安装公开 API 链接。当前 SPS/VUI 权威范围产品仍缺失。

### 最小生产闭环

planner 形成格式、尺寸、有效范围、上传完成机制与内存边界的类型化画布契约；在 `nodes/metadata/CodecResolverNode::prepareEncoder` 发布 codec 前绑定真实生产 hw_frames_ctx，不能以 capability 自建 pool 代替。共享画布 producer 初始化一份不可变黑模板，aggregate 按输出时间克隆独立 AVFrame header；合成可写目标另行分配，不能修改共享黑模板。producer 不新增线程或无界任务队列。

预算必须覆盖实际 staging 字节、回读核验峰值、硬件 surface 分配字节、常驻模板与有限 pending headers，并通过现有 reservation 约束 CUDA 活跃 surface。`MediaFramePayloadFootprint::logicalBytes` 只是逻辑像素字节，不足以证明物理分配硬界。

本节尚缺 SPS/VUI 范围产品、实际生产帧池预算、CUDA/RKMPP adapter 与 aggregate 消费闭环；未新增 helper 或实现代码，不能将单独 helper、存在滤镜或上传成功当作功能完成。原 Windows→RKMPP 真实链路、断流/恢复及双独立审查门禁保持不变。
