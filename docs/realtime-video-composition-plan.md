# 固定实时视频合屏实施计划

## 目标与约束

在现有生产 DAG 内实现固定 2～4 路 RTP/RTCP 合屏，并同时提供 realtime CLI 和 C API。两路左右分屏，三/四路四宫格，按输入顺序排列，等比留黑边；指定一路音频。各源独立实时播放，不保证跨源拍摄时刻一致。单格断流黑屏，音源断流静音，恢复重新对齐；全部断流保持输出，显式停止或全部可信结束后关闭。启动前所有必需源必须完成权威探测。

验收输入统一使用既有 120 秒连续源，不低于 1280×720、30 fps、约 8 Mbps；合屏输出 MPEG-TS/RTP、1280×720、30 fps、目标 8 Mbps，覆盖 H.264/HEVC 与 CBR/VBR。Windows 通过后再验收 RKMPP。首期不包含动态输入、动态布局、自定义矩形、混音。

唯一新增外部参数为用户已同意的输入列表、固定网格和指定音源；编码意图与输出配置复用现有模型。内部时序、容量、同步和硬件产品必须由 planner 从真实证据推导。不得新增平台专用媒体链路、运行时 fallback、测试体系或样例常量。

## 工业实现依据

- [GStreamer 同步](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)：公共时钟与源时间到 running-time 的映射。
- [GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)：每输入队列、单聚合线程、带 PTS/duration 的 GAP 与事件顺序。
- [FFmpeg framesync](https://ffmpeg.org/ffmpeg-filters.html#framesync)：多输入选帧与显式结束策略；不能代替网络失活、重连与资源准入。
- [Rockchip 滤镜](https://github.com/nyanmisaka/ffmpeg-rockchip/wiki/Filter)：平台合成候选能力，必须与实际部署版本核对。

## 阶段一：多源基础

- [ ] 核实实际 Windows/RKMPP 合成依赖、帧池与完成语义，记录能力门禁。
- [ ] 取得现有真实 CLI 的功能缺失/单源约束证据，完成全量基线构建。
- [ ] 按 planner 源域组织 runtime binding、时钟、启动与恢复；保持最终输出单权威。
- [ ] 建立独立输出时间轴与有界多源贡献记录，避免任一路重连重置输出代次。
- [ ] 规划带时间区间的源缺口与恢复，内部故障仍明确失败。
- [ ] 汇总 decoder surfaces、滤镜持帧、合成输出、暂存、编码缓存、队列与线程准入预算。
- [ ] 回归现有单源 VideoOnly、AudioVideo 与动态视频多输出生产链路。

## 阶段二：合屏交付

- [ ] 通过既有 DAG 节点组合输入、解码、缩放、聚合、硬件合成、编码与协议输出。
- [ ] 聚合线程独占选帧状态，每次处理有界批次；慢源不阻塞输出，所有权和资源凭证使用 RAII。
- [ ] 音频解码、重采样与编码，连续样本时间轴补静音并在恢复时对齐。
- [ ] CLI/C API 共用控制入口；独立合屏配置保持既有 C ABI、枚举与生命周期语义。
- [ ] 增加逐源帧龄、缺口区间、代次、合成输出与 A/V 漂移观测。
- [ ] Windows→RKMPP 逐项真实验收，覆盖布局、转码、RC、断流、恢复、背压与停止。

## 交付门禁

同一分支 `feat/realtime-video-composition` 完成 commit/push；每个完整通过的真实链路立即独立提交，标题与报告列出协议、编码转换、分辨率、帧率、码率及 RC。冻结后两个未参与实现的独立智能体同时明确 PASS，再提交 PR 并由新智能体审核。更新架构、质量评分和中文完成记录，UTF-8/CRLF，不纳入临时测试或既有未跟踪产物。

进度与证据见 [实施记录](realtime-video-composition-progress.md)。
