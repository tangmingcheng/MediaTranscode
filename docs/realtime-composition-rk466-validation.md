# rk466：raw 有序关闭标记出口

状态：五文件双独立 SOURCE PASS；RKMPP 8 核增量 9/9、退出 0。没有启用 BYE producer，不构成实流通过。

prepared source 的原 packet FIFO 接受合法 EndGeneration，按实际 MediaControlBuffer 与队列 entry 字节及一个对象计费，沿原 take/purge 释放。普通 payload 路径保留；控制不伪装媒体负载，不新增队列。

raw 在前序 pending 完成后转发标记；首次发布和背压重试均持 generation arbitration，核对 live closing 与成员 sealed frontier。退休标记取消，其他错误保留；发送后停止读取，原 purge 优先取消 pending EndGeneration、清输出并复位。Raw 普通 factory make_unique 的完整 sizeof 纳入源固定准备存储，未找到既有重复计费。

source_drain_review_a、source_drain_review_b 均 SOURCE PASS，git diff --check 通过。后续 producer 仍必须实现冻结映射、已接受媒体排空及满 FIFO 信用等待，不能因本片出口支持而直接发标记。

构建 PID 2660287，wait=0。CLI 13079416 字节，SHA256=762aa41b48d967c2bc3ab2867318aa1ecbc47ba5430346bc48579a41198d02f6。原 CLI、源码及 incoming/existing 清单保存在目标 out/rollback/rk466；回退需移除清单差集新增文件再恢复。

本轮没有 CLI/FFmpeg/VLC 实流或 local CLI 测试，Windows FFmpeg 未改。临时清单：本机 out/acceptance/rk466-source.tar；目标同名 tar、rk466-build.log。归档后按清单清理，保留回退及构建。

尚未完成：prepared source 关闭状态与时钟映射交接、RTP 接收 owner 截止、末标记生产、新代协议及 clock-group 退休；原规格双平台实流、失败退出和单路/动态输出回归。

清理完成：3项临时文件逐项删除，本机/目标无rk466临时项，构建PID已结束。
