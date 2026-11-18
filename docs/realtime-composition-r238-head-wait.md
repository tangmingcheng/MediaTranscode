# r238：迟交AU自身等待观测

## 依据

r237失败audio在head到达至scheduled之间耗时37.163ms；现有最长缺视频等待属于另一audio，不能解释本次迟交。采用[GStreamer逐element latency tracing](https://gstreamer.freedesktop.org/documentation/coretracers/latency.html?gi-language=c)及[逐buffer lateness](https://gstreamer.freedesktop.org/documentation/rstracers/buffer-lateness.html?gi-language=c)的关联观测原则，保留本项目canonical身份；不引入GStreamer事件或调度策略。

## 实施边界

每个既有head持有一个固定大小记录，复用scheduler既有master采样，观察对侧head缺失区间。记录最近区间、已完成累计、区间数、是否仍活跃以及恢复的对侧generation/sequence。随head销毁或代际清理而清零，不新增队列、线程、锁、公开配置或媒体容量。

仅在既有late_output_commit日志中输出本head记录及canonical sequence。原arrival、scheduled、committed时间继续用于区分选中前和提交等待。当前不区分选中前全部状态：对侧存在但排序在前、release等待和线程调度仍需单独证据。观察到恢复的时刻不是producer实际完成时刻；EOF/control恢复无canonical身份，显式输出unavailable。日志只保留最近缺流区间和累计，不保留所有区间历史。

不改变双head准入、全局封装顺序、失败期限或失败策略。共享Windows/RKMPP节点复用同一观测，先完成Windows验证。当前待双独立审查、全量构建及原规格实流，无通过结论。

双独立审查最初FAIL：新增累计运算缺少边界，原late日志异常可阻止applyCommit。已补非负/区间结束早于开始/累计与计数上限检查，显式valid字段；日志State门控并捕获异常，applyCommit位于诊断块之外。两位复审均限定PASS。回退检查不覆盖所有相邻采样。Release全量构建会话84561进行中，暂无结果；实流未验证。

全量构建84561完成exit0，configure/build均0，CMake/Ninja及两CLI实际存在；未运行local CLI。realtime SHA256=31E1DD8AF301A1411FB6F11570DE98312A29C6BC5E7A56A77FDA16F6E131BD95。当前待原规格实流复验，无运行时修复结论。
