# 合屏依赖能力与失败语义门禁

## 已核实的部署

| 平台 | 实际库与源码 | 合成能力 |
|---|---|---|
| Windows | `3rds/ffmpeg/bin/avfilter-11.dll`，11.17.100；`D:/mabs/build/ffmpeg-git`，`20054712a242c54aa19d9ae9fca7a0381a2f1397` | `overlay_cuda`、`scale_cuda`、`overlay_qsv`、`xstack_qsv` 存在 |
| RKMPP | `ffenv on` 后 `/home/tang/ffmpeg-ab1e61a/bin/ffmpeg`，11.14.102；源码 `/home/tang/ffmpeg-rkmpp-close-ownership`，`ab1e61adaa21ff129caa8e01a1198156044567ed` | `overlay_rkrga`、`scale_rkrga` 存在 |

Windows 源码版本对应生产版本 `N-125148-g20054712a2-g9cbd889670+3`。仓库与 `D:/mabs/local64/bin-video` 的 `avfilter-11.dll` SHA256 相同：`D6B67A4870A21B330F1F2C143A8AF22B565A852C3B7BDD0CFA70E91E56976D0D`。此为源码/版本一致性检查，不是可重现构建证明。

设备：Windows RTX 4060 Laptop GPU，driver 610.62，8188 MiB；RK RGA driver 1.2.23，两个 RGA3 核及一个 RGA2 核。RGA3 输出范围 68×2 至 8128×8128、缩放 1/8 至 8、stride 对齐 16；720p 格子尺寸处于其范围，但多路吞吐尚未验证。

## Windows CUDA：失败语义门禁 FAIL

实际 `libavfilter/vf_overlay_cuda.c` 中 `overlay_cuda_call_kernel()` 返回 `cuLaunchKernel` 检查结果，但 `overlay_cuda_blend()` 的各平面调用不检查该返回值，随后仍调用 `ff_filter_frame()`。因此 kernel 启动失败可以未传播到调用方；该结论来自源码，不声称已运行复现。

此外，滤镜返回并不证明 GPU 完成；使用 main CUDA stream，没有核验两输入 CUDA context 相同。`ff_inlink_make_frame_writable()` 可能分配并复制整个画布。planner/adapter 必须核实设备一致性、上游完成依赖、输出完成和资源保留边界。

不能以调用滤镜后增加一次同步来声称补回所有已被忽略的立即启动错误。参考 [CUDA Driver API 错误与启动语义](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__EXEC.html) 与 [FFmpeg overlay_cuda](https://github.com/FFmpeg/FFmpeg/blob/9cbd8896708050f12ccfcb1dd9aefda9e57dc349/libavfilter/vf_overlay_cuda.c)。

## QSV 替代调查：本次尚未关闭门禁

现有 scanner 支持 QSV，生产 overlay/xstack 共用 `qsvvpp.c`。通常路径会返回 VPP/sync 错误，但实际源码 968～978 行在 EOF 排空同步失败后仅 warning，仍输出帧；DEVICE_BUSY 与 IN_EXECUTION 等待也没有总体截止时间。不能据滤镜存在或 `async_depth=0` 把 QSV 宣告为已完成等价验证的替代。

## RKMPP：机制可复用，运行证据待补

部署 `rkrga_common.c` 的 overlay secondary 仅支持 RGB，因此需要由 planner 组合既有 RGA 缩放/颜色转换节点；不可直接拼接两路 NV12。`async_fifo=async_depth+1`，src/dst/pat 资源保留至 `imsync` 完成。framesync 每输入 current/next 与滤镜边队列需额外计量。版本查询时有 `mpp_platform: client 12 driver is not ready!`，尚不能将其归因于合成失败或判定无影响。

生产 Windows 公开 `AVFilterGraph.max_buffered_frames`，可限制滤镜边队列；它不包含 framesync 已取出帧或硬件在途资源。RK 对应 API 和生产 CLI 最终动态链接路径仍需核实。

## 需要明确授权的依赖修复范围

推荐直接修复实际 Windows FFmpeg 的 CUDA 错误传播，并重建、更新匹配依赖，不在 DAG 内加吞错包装器：

1. 每次 kernel 调用保存并检查返回值；失败立即进入统一清理路径，不继续发帧。
2. 统一清理保留首个错误，处理已提交 GPU 工作的完成与资源生命周期，并检查 context 恢复结果；不能在工作未完成时释放其输入资源。
3. planner/adapter 校验设备一致性并落实完成契约、滤镜队列上限和帧持有预算。
4. 仅变更合屏所需依赖代码；保留已有三项 codec 修复，记录源码提交和二进制 hash，回归现有生产链路。

该方案将修改原计划之外的 `D:/mabs/build/ffmpeg-git` 和 Windows FFmpeg 部署依赖。按 AGENTS 第 23 条，在依赖范围获得同意前不修改该外部源码或替换 DLL。DAG 基础方案与合屏规格保持原批准要求；本门禁不得被标记为通过。

## 本轮命令与证据边界

Windows 执行 `D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -h filter=overlay_cuda`、`C:/Windows/System32/nvidia-smi.exe --query-gpu=name,driver_version,memory.total --format=csv,noheader`，并直接加载生产 DLL 查询 filter registry/version。

RK 通过已有 SSH 执行 `ffenv on`、`command -v ffmpeg`、`ffmpeg -hide_banner -version`、`ffmpeg -hide_banner -h filter=overlay_rkrga`、源码 `git rev-parse HEAD`/`git status --short` 及 `/sys/kernel/debug/rkrga/driver_version`、`hardware` 读取。会话已退出，未安装工具或启动真实媒体流。
