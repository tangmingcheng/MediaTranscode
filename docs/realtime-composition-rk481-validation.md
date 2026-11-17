# rk481：RTCP服务上界修正与原规格四屏复验（进行中）

目标基线061a2d34，加8个RTCP服务上界修正文件；不含本机尚在复审的Closing改动，不修改外部FFmpeg。两位独立审查者对RTCP切片均明确SOURCE PASS，尚不代表运行验收。

依据RFC3550 §6.3.1，planner、policy校验及运行时共用精确floor整数间隔；未来报告数由真实canonical release范围和最小报告间隔推导，保留首个可能报告，不以availableAt缩短范围。媒体计费及100ms期限不变。本修正不解决OS等待超调。

首次8核增量构建PID3184343退出1：恢复基线tar保留旧时间戳，Raw目标文件未重编译，链接仍引用带purge参数的新签名。源码核实已恢复旧签名，刷新备份中5个源码时间戳后重试PID3190070；不将首次失败记为成功。

回退位于目标out/rollback/rk481，含7个原RTCP源码及基线CLI；恢复还需删除本轮新增MediaRtcpInterval.h并刷新恢复文件时间戳再增量构建。Closing原版本回退仍保留在out/rollback/rk480。

本轮临时清单：本机out/acceptance/rk481-source.tar、rk481-run.sh；远端out/acceptance同名两文件及rk481-build.log、rk481-build-retry.log。运行尚未开始。

重试26/26退出0，CLI SHA256 da522e12a64bb6de2659ebd7c6d181ec7e55d1fe1adfd44c1888f40af143ed72。Closing第四稿同时获得双SOURCE PASS，下一轮rk482叠加后进行原规格实流；rk481仅完成源码审查及构建，不宣称真实链路通过。

本轮未启动媒体进程；记录后按上述清单清理构建日志、传输tar和未执行脚本，保留回退与正式二进制。
