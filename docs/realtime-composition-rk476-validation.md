# rk476：prepared source 同步组绑定

已实现注册期一次性弱引用绑定，检查冻结状态、相同master clock与members；videoRaw唯一触发，音频不重复。源锁内只读取不可变属性，后续仲裁须在锁外取得。optional weak保留绑定事实，目标过期也不得重绑；实际sizeof及共享分配计费覆盖新增字段。Windows/RKMPP复用同一实现。未启用BYE，不能计为生产关闭完成。

依据既有sender-bye-drain调查及GStreamer串行EOS生命周期：https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html 。本切片仅建立所有权关系，不改变媒体算法。两位独立审查source_drain_review_a/b均SOURCE PASS。

目标8核增量构建PID3003472，回退保留out/rollback/rk476/source-before.tar与原CLI；结果待确认。没有运行本轮CLI/FFmpeg/VLC，无新增实流通过项。

下一闭环：RTP owner在首个截止后batch项前或无未处理batch时报告capture边界；source冻结Opening/Closing，保留原batch及ordinal，协议按真实时间排空，然后原FIFO提交EndGeneration。purge等待两成员精确退休消费回执后才开放新代；不能仅将InvalidateMember替换为Preserve。

构建完成：wait返回0，CLI13106696字节，SHA256 a8b56e1d82f866989d0161db0c893b25ad423c3fb4b689bee35b59da25340b95。清理本机out/acceptance/rk476-source.tar及远端同名tar、rk476-build.log；保留回退目录。不宣称实流通过。潜在风险仍包括未接通capture截止/关闭/退休屏障、rk475发送期限失败和rk473失败退出等待。
