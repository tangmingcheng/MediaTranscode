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

阶段一恢复屏障之后的切入点经独立设计审查确认：在解码后的规范化帧与共享输出编码之间建立源域→输出域映射。`MediaRealtimeAvSyncRuntimePlan`、runtime registration及transition planner明确归属；单源purge只清其输入/解码/源侧缓存，共享encode、scheduler、mux和RTP序号保留在输出域。lineage须区分真实源贡献与生成黑帧/静音，不伪造源AU。聚合节点复用master clock和执行器deadline，以整数帧号/样本号推导连续时间，按有限候选选帧，到期缺失则生成planner确认格式/硬件驻留的黑帧或静音；下游背压时禁止积累无界tick。不能只让startup clock继续发tick，因为现有整链generation仍会重置输出。该设计尚未实现，不代表阶段一已通过。

r13真实恢复已锁新代并生成协议计划，随后暴露AAC不支持flush而mapper被重置的契约冲突。下一完整切片须同步修改source/output域产品、runtime归属注册、builder边界及音视频贡献模型；AAC codec、编码FIFO、packet mapper和唯一output origin从输出启动到全局终止连续保留。音频按prepared readback规划连续整数样本区间，缺口显式GeneratedSilence，恢复只接入尚未提交区间；现有interval accumulator复用区间算法，校验output generation，source generation移至贡献记录。仅删encoder purge注册、补静音却不处理视频、重写PTS或忽略旧包均不构成完成。源失锁不终止输出，只有显式停止或所有源可信结束才排空编码和协议。

- [ ] 通过既有 DAG 节点组合输入、解码、缩放、聚合、硬件合成、编码与协议输出。
- [ ] 聚合线程独占选帧状态，每次处理有界批次；慢源不阻塞输出，所有权和资源凭证使用 RAII。
- [ ] 音频解码、重采样与编码，连续样本时间轴补静音并在恢复时对齐。
- [ ] CLI/C API 共用控制入口；独立合屏配置保持既有 C ABI、枚举与生命周期语义。
- [ ] 增加逐源帧龄、缺口区间、代次、合成输出与 A/V 漂移观测。
- [ ] Windows→RKMPP 逐项真实验收，覆盖布局、转码、RC、断流、恢复、背压与停止。

## 当前实施顺序：源处理与持续输出分离

1. segment 显式返回源处理/输出处理成员；registration 校验互斥、完整覆盖和角色归属，runtime 消费同一产品。此步保留原整体 transition，不宣称恢复已修复。
2. 连续输出 generation/origin 与真实源贡献分开；音频复用整数区间运算，视频记录各源贡献；静音/黑帧不伪造源 AU。贡献条数、样本、字节分别形成有界契约。
3. 在规范化帧与共享编码之间接入聚合节点；单 owner、既有 master clock/deadline、有限候选与单 pending 事务，到期缺口生成黑帧/静音，背压不积累无界 tick。
4. 聚合边界完整后切换逐源恢复权限与唯一输出权限；AAC context、FIFO、packet mapper、scheduler、mux 与发送序号保持连续，源恢复仅清本源未提交候选。
5. 同规格真实源断流/重入验证后接入 2–4 路组合，完成 Windows→RKMPP 矩阵与双独立审查。

音频量子取 prepared encoder frameSizeSamples，不能写死编码器帧长。黑帧的格式、色彩范围和硬件上传必须经实际 capability/readback 规划；已有 packet-layout probe 的清零帧不能等同于黑帧能力证明。上述边界对应 [GstAggregator 的输入队列与 GAP/flush 生命周期](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)，不是新增平台专用媒体链路。

## 交付门禁

同一分支 `feat/realtime-video-composition` 完成 commit/push；每个完整通过的真实链路立即独立提交，标题与报告列出协议、编码转换、分辨率、帧率、码率及 RC。冻结后两个未参与实现的独立智能体同时明确 PASS，再提交 PR 并由新智能体审核。更新架构、质量评分和中文完成记录，UTF-8/CRLF，不纳入临时测试或既有未跟踪产物。

进度与证据见 [实施记录](realtime-video-composition-progress.md)。

## 2026-09-21：编码提交事务与画布事实

编码器接受帧后无法撤销，贡献元数据须在 send 前完成验证和分配，成功后仅执行不分配的提交；EAGAIN 保留待发送帧并先接收，再重新准备候选。沿用现有 owner/lineage lock，不改变包时间轴与恢复语义。随后使用 Release 和原规格真实链路检查回归，双独立审查后交付。

已核实版本的 AAC 驻留推导及 CUDA 黑帧路径分别记录在[编码驻留调查](realtime-video-composition-encoder-retention.md)和[能力调查](realtime-video-composition-capability.md)。未知编码器支持范围不得因版本白名单而隐式收缩；黑帧尚需权威 SPS/VUI 范围、生产帧池预算与 aggregate 消费闭环。以上不替代阶段一/二原验收门禁。

## 2026-09-21：下一实施边界

持续聚合、域绑定与复用段的组合图已落代码，尚无组合入口；r22单源原规格结束后仍无进展超时，记录见[聚合接线](realtime-video-composition-aggregate.md)。下一步按以下依赖补全，不复制多个整链单源输出计划充当合屏：

1. 从现有preflight抽出逐源事实准备，验证真实SAR；将固定grid cell与等比内容矩形区分，常黑画布只拷贝内容矩形。
2. 抽取源处理/时钟和唯一输出规划，共用实际prepared编码器、设备与帧池；黑场、tile copy及分配证据前移到DAG构建前。
3. 现有资源compiler按明确owner选择事实，统一核算源输入/decoder/filter、聚合候选/贡献/canvas及唯一输出/协议；不能直接相加多个单源总账。
4. 内部生命周期产品已区分旧单源失败语义与持续合屏缺流，未发布代再次失锁、owner-thread purge、到期等待新证据及代次仲裁已通过源码双审。仍须由composition planner选择Preserve并通过真实源链路验证，首次准入及purge ack超时保持失败。r23仍走Shared单源路径，不能作为此模式运行通过证据，见[源生命周期记录](realtime-video-composition-source-lifecycle.md)。
5. 接通已批准的inputs/grid/audioSource公共配置与共用controller，补纯视频非音源、逐源缺口观测及Windows→RKMPP完整验收。迟到输出须有planner有界策略，单pending不等于历史追赶工作有界。

逐源纯规划公共切片已完成源码双审和Release全量构建：decoder/filter候选、评分和源执行契约由新source与旧single共用；显式首帧协商仅提供graph配置/readback。r24原规格单源仍在源结束后无进展退出，完整门禁未通过。下一步继续真实输入lease、decoder callback/首帧lineage交接和逐owner资源准入，不把候选或synthetic probe当作准备成功。见[逐源规划记录](realtime-video-composition-source-planning.md)。
