# CUDA Driver API 类型头来源

`dynlink_cuda.h` 来自 FFmpeg/nv-codec-headers，MIT 许可全文保留在头文件顶部。

- 固定来源：https://github.com/FFmpeg/nv-codec-headers/blob/f5a07297dba62ab42920b72dd85a672db208bd6d/include/ffnvcodec/dynlink_cuda.h
- 本机已安装版本标识：`13.1.15.0.0`。
- 2026-09-21 核验：已安装头与官方原文件逐字节相同。
- 上游原文件 SHA256：`c970d5817120ea481ba29b9c603bdc9a386985d9fc98a47f6588030dcdba87c1`。
- 仓库仅将行尾统一为 CRLF，SHA256：`d8cfab3f51c765ef93772d37fe50c2c71d39d88c9ff878daf1a15a82f650adf1`。

只提供 CUDA 公共类型及函数签名，不包含驱动实现。Windows 画布 adapter 从系统 `nvcuda.dll` 获取公开函数；RKMPP adapter 不包含此头。
