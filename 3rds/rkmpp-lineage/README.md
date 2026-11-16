# RKMPP 合屏身份适配补丁（开发中）

仅用于目标机隔离依赖；禁止应用到本机Windows FFmpeg。用户要求保留Windows已验证链路。

- mpp-lineage.patch 基于Rockchip MPP c08762ebfadeb4e986d2fed993bc7a54862d3ebe。
- ffmpeg-lineage.patch 基于本项目既有FFmpeg Rockchip ab1e61adaa21ff129caa8e01a1198156044567ed，包含原last_pkt关闭修复的基线不变。
- 这是开发检查点，双独立整批审查、最终编译及原规格真实流尚未完成；不是官方已合入补丁或成功验收版本。

补丁保存UTF-8 CRLF；目标依赖使用LF，应用前以 `sed 's/\r$//' <补丁 | git apply --check -` 在精确基线检查，再同样转换应用。两端传输原始LF补丁SHA256：MPP c4670a3a81f0dcd6913e8d30dc133ee72f290ecbc8a272b9f1fb36046273bfc5；FFmpeg 0fac2b41f1ebf384dc9857fc507e83c5fae0c09c8749c10fff1e373c5eff967f。本机已比对传输hash后转换行尾；后续修订须重新导出、更新记录。

设计、原依赖hash、独立源码目录与旧CLI回退副本见 [实施与回退](../../docs/realtime-composition-rkmpp-lineage-implementation.md)。新库仅安装项目内独立prefix，禁止覆盖系统库和原FFmpeg目录。

新增候选ffmpeg-encoder-split-readback.patch基于实际隔离FFmpeg HEAD 92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696，仅暴露编码器有效split配置，双独立限定源码审查通过；尚未应用/构建，不代表完整编码进度能力。先将CRLF转LF再严格dry-run。证据及错误完成缺口见[编码完成调查](../../docs/realtime-composition-rkmpp-encoder-completion.md)。
