# 解码展示启动与压缩随机访问门禁分离

rk421证据：源0已真实解码、越过payload envelope门禁，但startup等待AV_FRAME_FLAG_KEY超时。当前隔离RKMPP decoder以及调查时FFmpeg主线/ffmpeg-rockchip均无该标记赋值；不能伪造KEY或将I-picture视为IDR。

[GStreamer关键帧设计](https://gstreamer.freedesktop.org/documentation/additional/design/keyframe-force.html)明确raw frame输入没有key-unit概念；[FFmpeg framesync](https://ffmpeg.org/doxygen/trunk/framesync_8c_source.html)按显示时间及可用帧同步。采用相同层次边界：压缩AU入口必须先取得随机访问；已经通过真实decoder和独立frame producer合同的显示帧，以display span/clock/preroll准入，不再重复要求压缩KEY标志。

既有保障：MediaRtpInputProtocolSession初始化、admitAccessUnit和失连续重新requireKeyframe保持；PreparedSession代际重置、decoder flush、lineage一致性、独立帧所有权、精确音频剪裁及输出编码关键帧策略保持。不得从packet KEY猜frame KEY。两名独立设计审查均PASS，未查到验收明确要求decoded帧必须带KEY。

实施：readiness模型提供严格类型到key要求的唯一映射；planner makePolicy显式接收readiness，decoded入口直接生成完整decoded产品，不先生成Interval后覆写；plan validator及coordinator校验同一映射。demux、压缩区间及continuous output原true门禁不变。现有decoded source branch仅检查generationStartRequiresKeyFrame存在且没有encoderLineage消费者，输出encoder使用独立output策略。

无新增公共字段、线程、队列、内存容量或硬件adapter；沿现有有界索引、背压和RAII。共享Windows/RKMPP策略，须同规格验证；当前先RKMPP增量构建与realtime，Windows复验保留后续。失败清理等待也须跟踪，不以源码审查代替恢复/长时/A-V验收。

状态：四文件实现完成，两名独立审查者均明确整批SOURCE PASS，等待目标构建及真实验证。目标SSH暂时主动关闭连接，rk421远端日志待恢复后清理，不伪称已清。

边界核验：decoded=false经options编码正常传递；压缩demux/continuous output仍true。decoded无帧仍受原maximumWait acquisition deadline约束，没有新增独立5秒无帧计时器。2026-11-16目标SSH默认连接关闭，绑定192.168.96.122后8秒超时；不能宣称已构建或清完远端产物。
