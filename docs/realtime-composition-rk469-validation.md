# rk469：视频滤镜与帧率节点的代内排空

状态：七文件两位独立审查者 SOURCE PASS，RKMPP 8 核增量构建 9/9、退出 0；尚未进行真实流验收。

沿用 GStreamer 串行 EOS 的先处理缓存、再传递边界原则（https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html）。本项目 EndGeneration 只结束一代，永久 EOF ledger 不变；重新开放由既有 purge 控制。

VideoFilter 与 VideoFrameRate 接收精确代次标记，沿原有 drain 流程完成待发输出，发送标记后停止读取。已建立 filter graph 必须取得真实 sink EOF；没有图且没有待发 holder 可完成，无图却保留数据则报错。Filter 在 purge 时重建已关闭或正在关闭的图。两个节点整个 process 持现有 recursive lineage 锁，覆盖基类 deferred 输出检查到提交，防止 purge 插入；队列背压返回等待，不持锁等待容量。固定存储按真实 filterActive/ObservedCadence shape 计入 state sizeof。

首次审查 A 指出空图无限等待与 deferred 提交竞态，结论 FAIL；修复后 A、B 均明确 PASS。未将初次审查或构建当成实流成功。

临时清单：本机 out/acceptance/rk469-source.tar；目标同名 tar 和 rk469-build.log。构建 PID 2768212。回退产物保存于目标 out/rollback/rk469。

剩余：HardwareTransfer 必须接入源组、owner purge、retained control 和提交授权，factory/transition/固定费用同步更新。其后接通接收端关闭、捕获截止与双成员失效消费门，再运行完整四屏实流和双平台回归。Windows FFmpeg 未修改。

构建结果：CLI 13092960 字节，SHA256=5cc298fb015bc89a208b375357066f8f2f15a7eb6e98613bf281138a68915c6d。仅全目标编译链接，未运行 local CLI。

清理复核：本机本轮tar已删除，目标本轮tar/log已删除且rk469查询为空；构建PID已不存在。保留回退产物。
