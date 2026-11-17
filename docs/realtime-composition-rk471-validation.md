# rk471：协议排空的真实残留查询

状态：十文件双独立 SOURCE PASS，RKMPP 8 核增量构建161/161、退出0。未运行实流，不代表关闭入口完成。

增加无分配、无修改的 protocol drainSnapshot：分别返回 reorder 实际数量、协议 pending 数量与 depacketizer 的 Empty/Incomplete 状态。H264/HEVC 覆盖 AU 字节、timestamp 和 FU fragment/header/timestamp；AAC 覆盖跨包 fragmented AU；当前 Opus 按单包完成。没有 flush、丢弃、补媒体或推导尾部时长。receiver 仅对 AcceptedReady/AcceptedWaitingCredit 推进 ordinal。

两位审查者确认没有遗漏实际 holder 导致假 Empty；查询必须由现有 source owner 同步调用。该结果不覆盖 receiver batch、payload lease、原输出 FIFO或下游 holder，也不证明历史未丢包。生产关闭还需要这些边界共同成立。

构建 PID 2840731；回退目录目标 out/rollback/rk471。临时清单：本机 out/acceptance/rk471-source.tar，目标同名tar与rk471-build.log。解包出现约0.39秒未来文件时间戳提示，未调整媒体时钟。

下一步：按调查文档的 Opening→Closing 短事务接通 BYE、RTP owner捕获截止、排空/残缺尾处理与双成员退休消费屏障。Windows FFmpeg未修改，local CLI未运行。

构建结果：CLI 13106552字节；SHA256=f2dfcbb5fbd1c4a23c526cb5c241c2d33f27812aceef9797864eb0c6f3eb25ea。

清理复核：本机本轮tar删除；目标本轮tar/log删除且rk471查询为空；构建PID已不存在，回退产物保留。
