# RKMPP 依赖修订

目标依赖以 [FFmpeg Rockchip](https://github.com/nyanmisaka/ffmpeg-rockchip) 提交 `d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf` 为父版本，应用本目录保留的 [关闭所有权修复](ab1e61a-decoder-close.patch)，得到独立提交 `ab1e61adaa21ff129caa8e01a1198156044567ed`。MPP 来源仍为 `c08762ebfadeb4e986d2fed993bc7a54862d3ebe`。

差异原样取自该依赖提交，仓库保存为 UTF-8 CRLF。在父版本的干净依赖工作树中，先用 `sed 's/\r$//' <差异文件> | git apply --check -` 检查；成功后以相同换行转换传给 `git apply -`，再核对 `git diff`。不将 CRLF 注入第三方 LF 源文件。

修复仅在 `rkmpp_decode_close` 的 MPP reset/destroy 后无条件释放私有 `last_pkt`。原版本在提交 EAGAIN 后可持包返回；关闭未释放该引用，不能以公共 flush、提前退还账本额度或忽略泄漏弥补。[FFmpeg MediaCodec close](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/mediacodecdec.c) 同样在关闭时显式释放私有缓冲包。

目标独立安装目录为 `/home/tang/ffmpeg-ab1e61a`，不覆盖原依赖。构建沿用原配置，仅更换安装前缀；实际生成的 `libavutil/ffversion.h` 与 `ffbuild/version.sh` 均为 `ab1e61a`。目录是此次部署位置，不是产品支持条件。

共享 `MediaRkmppDependencyIdentity` 要求精确运行时版本，并比较 libavutil、libavcodec、libavfilter 的完整构建配置。解码驻留、自然 IDR 与源隔离三个 adapter 共用此身份；旧版本不再授权相应契约。该检查不证明库内容或 MPP 二进制来源：交付必须保留实际配置、完整依赖哈希、CLI 加载路径及进程映射，确认同一套库。不得仅替换 libavcodec 而保留旧 libavutil 版本标识。

本记录不代表新依赖构建或真实链路验收通过；结果分别记录在对应 Windows、RKMPP 验收报告中。仓库只保留最小源码差异与来源证据，不纳入完整第三方仓库或测试材料。
