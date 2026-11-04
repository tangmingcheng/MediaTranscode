# r278：MP2T直接写入最终发送存储

状态：已实施并修复借用入口；a与b2第二次冻结复审均局部PASS，最终全量Release构建57699通过。原规格实流在重入阶段失败，详见realtime-composition-r278-validation.md，不声称解决时序。

## 问题与依据
MediaMpegTsRtpWireDatagramMaterializer的普通批和协议批各建立payloads、payloadOctets、packetized三个vector，并先写完整RTP包，再由内层解析、复制到最终backing。这些临时副本不在r276的同笔准备存储准入中。

沿用GStreamer allocation的显式存储与所有权合同，以及RFC2250的完整TS包承载约束：
- https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html
- https://www.rfc-editor.org/rfc/rfc2250.html

并不引入新RTP算法。复用现有MediaMpegTsRtpPacketizer::prepare/writeInto和MediaRtpWirePacketComposer；已有packetizer检查完整188字节TS包、最大TS包数、最大数据报和时钟映射。

## 实施合同
1. 输入使用有限类型集合，覆盖既有raw RTP span、普通TS视图批、协议TS批。同步按索引读取，不建立临时plan数组，不使用开放回调框架。
2. 每次取得的TS plan按值返回，其TS字节借用原批。两遍准备期间输入、锁和materializedAt稳定；访问不得改变序号或协议状态。
3. 最终wire plan保存raw rewrite plan或经过校验的MP2T plan。MP2T的最终序号、时间戳、SSRC和PT由既有RTP会话约束绑定；不开放任意修改接口。
4. 对生成MP2T包省去原始字节重解析时，必须以类型构造合同覆盖版本、固定头、无扩展/填充、PT/SSRC、payload计数和最大字节约束。既有raw输入仍运行全部解析检查。
5. r276同笔准入负责最终plan数组和输出存储；plan类型大小改变后，workspace与planner ledger继续使用同一精确sizeof/对齐计算。
6. 保持generation检查、协议批最大前缀、RTCP顺序、时间戳单调性、deadline、nested commit、取消poison及唤醒语义。serial仍保持，不能仅凭副本消除放开多笔预约。
7. 不改变线程模型或平台adapter；借用仅持续到同步序列化完成，输出不得保留输入引用。

## 验证门槛
实施后双独立冻结审查、全量Release重建及原规格realtime链路验证；仅使用VLC原生快照。检查字节/对象计费、退出原因、持续A/V漂移及进程内存。未通过原有稳定性和失源重入门禁，不记为成功验收。

## 当前实施与审查
新增有限类型输入批，raw RTP保留原解析，MP2T计划按值返回并复用writer；两处外层三个vector已删除。workspace按variant实际大小计费。第一次全量构建80387进行中，保持源码冻结；结束后将packetizer借用参数改为reference_wrapper，拒绝临时对象绑定，再进行双复审及重建。尚未实流验证。

第一次全量Release构建80387成功，configure/build均0。随后已将两个TS输入构造改为reference_wrapper，外层显式std::cref，拒绝临时packetizer；git diff --check通过，已重新冻结并请求双复审。第二次全量重建进行中。

最终构建realtime SHA256：281D126DAA59D382863ABE5880BA81DF96F968D03CA489BFFF8B84FB153D62B4。实流三源完成120秒3600帧，VLC确认三格和失源全黑；重入阶段因7次serial WouldBlock后超过准备截止时间1775520ns而失败。15个临时文件、六PID已清理。下一步闭合剩余存储和多预约取消证明，不能将本轮局部审查扩展为整体PASS。
