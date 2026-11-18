# r264：协议布局与已准入存储写入

目标：闭合workspace所需的权威字节几何，禁止通过未计费临时vector探测长度。统一协议原实现的校验、布局和span写入，现有全部调用点迁移，删除旧分配接口；本切片尚不宣称workspace已统一计费。

依据：[GStreamer GstRTPBuffer](https://gstreamer.freedesktop.org/documentation/rtplib/gstrtpbuffer.html)分离calc_packet_len和map/字段访问；[RFC3550](https://www.rfc-editor.org/rfc/rfc3550.html)第5、6.4–6.6节定义RTP扩展/填充及SR/SDES/BYE；[RFC2250](https://www.rfc-editor.org/rfc/rfc2250.html)第2节规定MP2T整188字节负载及90kHz时钟。本项目复用既有解析/时钟/序列算法，只改变存储边界，不引入GStreamer依赖或改变协议策略。

合同：prepare成功路径不分配，返回私有构造的只读布局，携带借用输入span/CNAME及真实尺寸；调用方须保持借用存储存活且不变直到同步writeInto结束，不发布或跨线程保留布局。writeInto要求精确目标尺寸，尺寸错误不写入；协议字段、负载统计、截断、扩展与padding校验均保持。失败仍可构造现有ErrorInfo字符串，因此不声称错误路径完全无分配。

RTP重写保留输入长度及CSRC/extension/padding；MP2T布局由完整TS输入和既有clock mapper形成；RTCP使用真实CNAME长度算32位对齐，借用稳定CNAME消除隐式string拷贝。当前调用点仍显式分配普通vector，下一步才与四块事务及外层/内层workspace合并一次准入；必须记录双份payload峰值，不暗中假定零拷贝。

线程及失败：无新线程/队列/重试；调用仍在原协议锁或同步mux回调内；未改变投影、提交、发送deadline或平台adapter。跨平台复用同一协议实现，先Windows原规格实流，RKMPP仍在后续门禁。冻结后双独立审查、Release全量重建及realtime失源/重入复验；临时产物按清单清除。

## 本轮完成情况

协议prepare/writeInto及所有调用点已迁移，旧分配接口和无调用异常桥接已删除。双独立静态审查均局部PASS；Release全量重建成功。三路120秒加所选音视频源完整120秒重入复验结束，VLC原生快照确认三格、失源全黑、重入、再次全黑；源和caller_stop退出均0。指标与原始文字证据见 realtime-composition-r264-validation.md，17个临时产物已清理。该结果不代表整体验收通过。

下一完整边界是一轮materialize直到全部wire batches返回：无分配首遍必须复用partition的deadline/数量/字节边界算法，统一计算外层MP2T、内层rewrite、RTCP、元数据、现有四块、partition backing及最终batch对象存储。需先完成最终batch控制块权威geometry和接管协议；同笔准入后只拆分lease，投影后不增加可重试分配。所有保留的payload副本同时计峰值，不能只将内层vector替换后声称闭合。当前serial保持，RKMPP和整体门禁仍未完成。
