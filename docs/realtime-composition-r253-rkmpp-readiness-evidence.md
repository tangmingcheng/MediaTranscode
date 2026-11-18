# r253：RKMPP 完成事实调查（未完成）

## 已核验源码
- 仓库依赖身份为 FFmpeg Rockchip ab1e61a（父 d90e3a1，仅 decoder close 释放 last_pkt 差异）和 MPP c08762e；本轮不修改外部依赖。
- [FFmpeg rkmppdec.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/rkmppdec.c)：rkmpp_get_frame 调用 decode_get_frame，排除无帧、discard/error 后发布图像。
- [MPP mpp_dec_normal.c](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/codec/mpp_dec_normal.c)：普通 HAL 线程 795–827 行先 mpp_hal_hw_wait，再清除输出使用标记并 push_display。
- [MPP mpp_dec.c](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/codec/mpp_dec.c)：put_frame 的直接输出分支将完成帧加入 mFrmOut；启用去交错时另走 vproc，不能仅用直接输出分支证明。
- [MPP mpp.c](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/mpp.c)：mpp_get_frame 从 mFrmOut 取出并执行 buffer sync。
- [FFmpeg rkrga_common.c](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavfilter/rkrga_common.c)：正常输出 1325–1333 行先 imsync，失败返回错误；EOF drain 分支只记录同步失败，因此不能仅凭滤镜名称推断所有返回路径安全。

## 可复用与缺口
仓库 MediaVideoSourceIsolationAdapter 已有精确依赖身份及 async_depth=0 的同步独立输出事实，后续应复用并读回实际配置，不新设默认值。共享 readiness 应表达经验证的 Completed 或 CUDA event，平台 adapter 提供实际生产事实，节点及 canvas 使用相同链路。

尚需核验部署实际配置与依赖身份、MPP 错误传播，以及 RGA EOF/失败边界；本调查不代表完整生产事实、构建或 RKMPP 运行通过。先完成 Windows 修复和验收，再运行 RKMPP。

## 补充核验：无线程与去交错分支

- [MPP no-thread](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/codec/mpp_dec_no_thread.c) 的 282–306 行在输出前调用 mpp_hal_hw_wait；调用顺序可证明等待被执行，但不能单凭此处未检查的返回值证明驱动失败均被传播。
- [MPP vproc](https://github.com/rockchip-linux/mpp/blob/c08762ebfadeb4e986d2fed993bc7a54862d3ebe/mpp/vproc/mpp_dec_vproc.c) 的 start_dei_v1/v2 调用 IEP_CMD_RUN_SYNC 并返回状态；若干调用者未检查 start_dei 返回值即继续发布。不能将收到任意 RKMPP 帧直接视为无条件成功完成。
- 当前 MediaDecoderRuntimeFacts 没有去交错配置 readback。后续需核验真实打开配置与实际走过的生产分支；不得通过强制关闭去交错或修改外部依赖绕过。此处是源码边界证据，不是 RKMPP 实流复现。
