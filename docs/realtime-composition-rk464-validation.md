# rk464：RTP binder 代内关闭

状态：三文件双独立 SOURCE PASS；最终 RKMPP 8 核增量 5 项退出 0。生产 raw/BYE 尚未发出新控制，本轮不是尾媒体或四屏实流通过。

binder 对 EndGeneration 先以现有 source arbitration 判定退休，再核 frozen clock-group→playback 关系、成员 ingress generation/SSRC 和 sealed frontier；控制在 raw packet 边明确属于 clock-group 域，普通 packet provenance 仍属于 ingress 域。raw 端未来必须从被冻结的关闭授权得到该身份，不能猜测各代次数字相等。

复用原 pending publication、acquiring 和 timing product 的尾包处理，前序提交后发布同一标记，发布也持有 source generation authority；结束后等待原 purge，purge 取消标记并复位。SourcePresentation 不建立 lookahead、不推断末 duration；interval 沿既有 repeat_last_observed_positive_delta 合同，不宣称真实末帧终点。

初审发现两项：clock fanout 可先清 projection、source 退休尚未到达，导致合法关闭误报；新增 binder sizeof 与 PreparedRtpSourceStoragePlanner 已有预算重复。已改为 closing 映射不匹配时在原 packet FIFO 等待，不先取出标记；移除重复计费，新增字段由已有完整 sizeof 自动覆盖。source_drain_review_a、source_drain_review_b 复审均 SOURCE PASS，git diff --check 通过。

首次构建 PID 2591933，9/9、wait=0，但当时源码审查未通过，不能作为最终结果。修复后构建 PID 2605258，5/5、wait=0。最终 CLI 13070904 字节，SHA256=3186ff73356dbb54e16ae2618954d9a47c8e999dfaf11c86b4e2e78360698659。

原 CLI、源码及 incoming/existing 清单保留在目标 out/rollback/rk464；回退需移除清单差集的新文件再恢复。传输包含 fixed storage planner 的恢复版本，最终该文件相对仓库无修改。

没有运行 CLI/FFmpeg/VLC 媒体链路或 local CLI，未修改 Windows FFmpeg。临时清单：本机 out/acceptance/rk464-source.tar；目标同名 tar、rk464-build.log。保留上述结果后逐项清理，保留回退与正式构建。

剩余：生产 BYE 旧代协议状态保留、RTP owner 截止、raw 标记发布、新代交接及其原规格实流；发送失败退出、Windows 共享链路与原单路/动态输出回归尚未闭合。

清理完成：3项临时文件按清单删除，本机/目标无rk464临时项，两次构建PID均已结束。
