# rk470：HardwareTransfer 源代次生命周期

状态：六文件修复后双独立 SOURCE PASS，RKMPP 8 核增量构建 8/8、退出 0；无实流验收结论。

按既有 GStreamer 串行 EOS/缓存排空与 flush 分离原则（https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html），Transfer 沿原 frame FIFO 转发 EndGeneration，随后等待 owner purge，不能把它当作 AVFrame 或永久 EOF。

registrar 根据真实 source ownership 注入 group 与 preparation；普通无源组路径不臆造同步身份。准备阶段按同组、同代、不可变 releaseIdentity 允许 Feeding/OutputReady 内部传输，最终 VideoPreparation 仍守激活提交门。OutputReady 没有撤销此前 feed，不新增无唤醒依据的等待或队列。已激活路径必须匹配发布 epoch 与 ReleaseCommitted。

实际提交持 generation arbitration；过期媒体取消，未来代/清理中保持原 pending。代内结束还必须匹配 closing 与视频 sealed frontier。owner 在原 process 开头处理 purge，清 input holder 和基类 pending，再确认完成；原 FIFO 中明确退休的结束标记在 retained 授权前丢弃，新代数据保留。factory 注册、transition required child 与 composition 实际 sizeof 计费同步更新。

第一次双人审查均指出 purge 后原 FIFO 旧标记会误触身份错误，判 FAIL；增加入口精确退休分类后，两位审查者均 PASS。构建 PID 2820389。回退目录为目标 out/rollback/rk470，保留原 CLI 和六文件源码。

临时清单：本机 out/acceptance/rk470-source.tar；目标同名 tar、rk470-build.log。源码解包出现约 0.21 秒未来时间戳提示，未改变媒体时钟或测试参数。

尚未完成：接收端生产关闭状态机、capture 截止、双成员退休消费门及原规格实流、Windows 共享链路回归。未修改外部依赖，未运行 local CLI。

构建结果：CLI 13106000 字节；SHA256=5211e47b724b40bae546aaf436b6996db1d4f74613d3c06eabc842d72a20fe02。

清理复核：本机本轮tar删除，目标本轮tar/log删除且rk470临时查询为空；构建PID已不存在，回退产物保留。
