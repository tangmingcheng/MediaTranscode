# 动态视频输出的共享源隔离

共享路径使用 `VideoDecode → VideoFilter → VideoOutputFanout`。滤镜仍由通用 DAG 节点执行；编码组仅订阅分发器，不持有解码器固定输出池的帧。

当前独立拷贝能力限定于 FFmpeg Rockchip 修订 `ab1e61adaa21ff129caa8e01a1198156044567ed`。该修订仅修复父版本 decoder close 私有包泄漏，滤镜与帧池代码不变；[来源与差异](../3rds/ffmpeg-rockchip/README.md) 单独保留。adapter 共用精确版本及配置一致性检查，并核对 `scale_rkrga` 可用性；planner 选择等尺寸拷贝及 `async_depth=0`。原 d90e3a1 和其他未验证版本不再授权此组合。

依据：[rkrga_common.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavfilter/rkrga_common.c) 的独立帧池初始化、输出分配及 fence 等待；[hwcontext_rkmpp.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavutil/hwcontext_rkmpp.c) 的 MPP buffer group 与 AVBufferPool。

同步完成允许发布独立输出，但源帧缓存仍可能保留一帧至 `clear_unused`，该引用须由类型化保留契约计账。驱动分配的字节数仍是观测值，不能由动态池推断无限容量或硬字节上界。

源 SAR 来自已有真实参数集 decoder readback 或输入格式快照，沿 session source facts 保留。`0/1` 表示未知，保持未知；运行时使用实际首帧 SAR，不替换为 `1/1`。输入池身份变化、输出仍引用原池或 fence 失败均终止对应共享源。

状态：源码接线完成，尚未通过本轮 Windows/RKMPP 真实流验收。
