# rk465：BYE 失效前成员事实

状态：协议三文件双独立 SOURCE PASS；RKMPP 8 核增量退出 0。尚未启用有序 BYE 关闭，不是实流验收通过。

匹配已观察媒体 SSRC 的 BYE 时，在 invalidatePrepared 递增代次与清状态之前复制旧 ingress generation、SSRC、捕获时间为 MediaRtpSenderDeparture。事实由 prepared observation 持有且不引用数据报；后续关闭 owner 可复制这些标量，但整个含借用状态的 prepared transaction 仍不得跨等待保存。

commit 先校验 owner/revision，再核对 departure 与 SenderLeft invalidation 双向配对、旧成员及递增后的代次；矛盾在 mutation 前报错。非匹配 BYE 不生成事实。原协议失效与 assembly 清理行为保持，新事实尚未用于拦截生产提交。无新增持久状态、队列或堆分配。

依据原已审关闭设计与 RFC 3550 的 BYE 成员身份；source_drain_review_a、source_drain_review_b 均 SOURCE PASS，已核 materialize 与 clock-session rejection 不破坏配对，git diff --check 通过。

构建 PID 2627223，wait=0。CLI 13075000 字节，SHA256=e51ec753cb051bdf38dd78a10b5fb17d4d4668fbfe5fc30b471dad3f86e0213b。原 CLI、三文件源码与清单保留在目标 out/rollback/rk465。

未运行 CLI/FFmpeg/VLC 实流或 local CLI，Windows FFmpeg 未修改。临时清单：本机 out/acceptance/rk465-source.tar；目标同名 tar、rk465-build.log。结果归档后逐项清理，保留回退与构建产物。

剩余主线：prepared source 消费离开事实，保留旧协议状态与冻结映射；RTP owner 截止、raw 有序末标记及新代交接；原规格双平台实流、发送失败退出与旧单路/动态输出回归。

34/34项构建完成；清理完成：3项临时文件逐项删除，本机/目标无rk465临时项，构建PID已结束。
