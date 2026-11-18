# rk477：RTP owner捕获边界事实

新增每stream单调exclusive-before用户态观察水位：所有observedAt严格小于水位的已捕获RTP项均完成顺序交付，当前项不包含在内。consume基本参数验证后、非bootstrap RTP分支在原source锁内记录原观察时间；idle只由无未处理batch/finalDatagram的RTP owner在resumeDrain等待前、接收WouldBlock后报告。RTCP和普通tick不报告。允许相等，拒绝倒退，不改变ordinal/credit/协议状态。

Windows完成项/recv和Linux recvmmsg均在用户态处理接收完成后采样；不能当作kernel到达或完整AU排空证明。记录跨代连续，purge不清零。新增optional scalar由sizeof(Stream)计费，无新队列、无新增公共字段。本片尚无消费者，不新增每包通知；未来Closing接线必须一起补充截止判定、原batch内gate和所需唤醒。

工业依据：GStreamer EOS与buffer串行推进（https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html）；RFC3550 BYE表示成员离开，不等于接收端已排空（https://www.rfc-editor.org/rfc/rfc3550.html#section-6.6）。本片是接收边界事实，不替代EOS、代次身份或末尾AU判定。

source_drain_review_a/b独立DESIGN PASS及SOURCE PASS，均核对bootstrap handoff、Windows staged条目、Linux批次及背压。RKMPP8核增量构建PID3026791，回退out/rollback/rk477包含原源码和CLI；构建结果待确认。没有运行本轮媒体链路，不新增实流验收成功项。Windows本机FFmpeg未改。

构建结果：wait返回0，CLI13111040字节，SHA256 0cbb1c3e2c44c134074795863e1ce2dc102433ab9b4d4255cebe39f3bd320fd6。本机out/acceptance/rk477-source.tar及远端同名tar、rk477-build.log按清单清理，保留回退。没有媒体运行结论；待完整Closing闭环原规格实流及Windows共享回归。发送截止与失败退出风险仍未关闭。
