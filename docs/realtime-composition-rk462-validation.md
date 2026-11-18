# rk462：聚合器代内排空确认

状态：七文件双独立 SOURCE PASS；RKMPP 8 核增量 12 项完成、退出 0。生产 BYE 尚未接通，不构成尾音完整性或四屏媒体验收通过。

视频、所选音频及丢弃音频入口核对精确关闭身份；候选与同代 pending 真正提交后才确认成员排空，只有视频 owner 完成整源关闭。末显示帧必须有同代 lineage 与已提交 sequence 证据。EndGeneration 不永久封闭输入；Retired 只停止读取并等待原 purge，不发送 drained ack。clear/purge 复位状态，新增状态由实际 owner sizeof 计费。

修复修改中三个入口仍使用旧 helper 接口的问题，以及退休判据先读旧 reacquisition、后读新 epoch 的快照竞态。source_drain_review_a、source_drain_review_b 均明确 SOURCE PASS；按 GStreamer EOS/Flush 分离原则核对有序排空与取消语义，git diff --check 通过。

目标构建 PID 2524126，wait=0，12/12 完成。CLI 13066296 字节，SHA256=129c0823ef2b3d5842028861a64e05f8d8f0000a09bb43618866b04ede4df1a0。原 CLI、原有源码、incoming/existing 清单保存在目标 out/rollback/rk462。回退时须按两个清单差集移除本轮新增文件，再恢复源码和 CLI。

未运行 CLI/FFmpeg/VLC 媒体链路或 local CLI；未修改 Windows FFmpeg。剩余：接收截止及新代交接、完整双平台实流、发送失败退出和原单路/动态输出回归。

本轮临时清单：本机 out/acceptance/rk462-source.tar；目标同名 tar、out/acceptance/rk462-build.log。归档后按清单删除，保留回退与构建产物。

清理完成：3项临时文件已按清单删除，目标构建PID已结束，本机与目标无rk462临时项。
