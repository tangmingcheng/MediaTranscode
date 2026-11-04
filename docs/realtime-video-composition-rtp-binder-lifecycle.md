# RTP 时钟绑定器断流边界调查

## 当前证据（2026-10-05，r91）

四路 V+V+V+AV、音源索引3，原120秒源、HEVC VBR 8000/8000/12000kbps、1280×720/30、MPEG-TS/RTP。source2 于00:00:50.296完成purge，00:01:00.296其node46 binder因初始时钟期限失败，CLI自然exit1。命令、指标、退出与清理见[运行记录](realtime-video-composition-wiring-runtime.md)。不能将该结果归因音源索引或宣布完整VBR通过。

## 已确认的代码缺口

- RawRtpInputNode::depacketize完成AU后以空sourceTiming包装原始包；当前包不能提供RTP时钟代际身份。
- MediaRtpPacketClockBinderNode::acceptClock收到非Locked状态只取消已锁投影；invalidateClockProjection清lookahead，但不清acquiring队列或deadline。
- bufferAcquiring对任一无锁包建立同一初始期限；onProcess在读取clock事件之前检查该期限。它没有区分首次准入失败和已激活源的持续重采集。
- RTP clock事件与packet走独立有界channel。下游源purge完成并不直接证明binder的内部队列及尚未绑定包已排空；注册与所有权边界需完整核对。

这些代码足以解释错误路径存在，但r91未记录触发期限的具体包代际，不能声称已用运行证据证明该包必为旧代。不能只加大timeout、忽略错误或清一个容器便称修复；旧包绑定新clock风险需同时闭合。

## 成熟实现对照与修复约束

[GStreamer事件设计](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)通过flush先停止接收并清持有数据，再同步恢复，保证恢复后的缓冲属于新边界；[rtpjitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)对等待与队列驻留设界。可复用原则是明确源边界、有界等待、owner串行清理，不能直接照搬其默认时长或把全局flush施加到合屏输出域。

下一实现前须确定：原始AU权威代际/身份事实如何随包传递；binder与输入channel的owner-thread purge或等价有序边界；首次准入仍失败、已激活源到期仅退回等待新证据的planner合同；旧包丢弃/新代重入/首次deadline的可观测证据。不得新建平台媒体链或修改外部FFmpeg。该路径由Windows与RKMPP共享，修复后先用Windows相同四源与单源AV规格验证，不能只证明一次不报错。

状态：根因合同调查中，尚未实施修复。最终须双独立源码复审、Release全量重建、r91原规格失败链及共享单源回归；完整合屏门禁保持未通过。

独立审查A：当前合同FAIL；binder清理还遗漏timestamp aligner，且未注册为generation participant。建议对照FFmpeg官方ffplay的queue serial：flush推进serial，消费入口拒绝旧serial；清队列和入口代际检查须同时存在。raw tracker generation与group generation不能猜测相等，须核验映射。下次修改冻结后仍需A/B两个未参与实现者同时明确PASS。

## r92 原始包事实切片（2026-10-05）

已新增内部MediaRtpPacketProvenance：实际SSRC、tracker ingressGeneration及与clock/event共用的ingressSequence；RawRtp完成AU时保存，FFmpegPacketBuffer内联持有，factory拒绝冲突provenance和零sequence。tracker初代0合法，不与groupGeneration非零约束混用。BYE和clock失效在RawRtp owner线程复用discardUnassembledPackets清reorder、pending items、depacketizer及未完成AU预留，避免旧片段跨边界被重标。已完成AU保留原事实，不能据此宣称它们已从channel清理。

首轮审查FAIL发现初代0误拒和旧reorder留存；均已修正。该切片不改变binder期限、未完成source barrier或消费者代际分类，r91仍未修复。7文件已冻结等待双独立复审与最终Release全量重建。首次构建exit0仅证明修正前源码可编译；不做媒体验收通过声明。对象metadata总字节预算仍属原有未闭合门禁，新增事实无独立堆分配但不能因此忽略对象体积。

r92最终结果：A/B两个独立非实现者对上述7文件均明确PASS；修正后VS2026 Release clean-first/all-target全量重建exit0，两个CLI及Beta库均链接成功。本轮未运行local或realtime媒体，不将构建成功当作r91修复通过。记录后删除两份本轮构建日志，无新增测试脚本/抓包/媒体残留。后续优先实现raw owner frontier与binder清理/分类，不扩大外部依赖或新增公共字段。

## r93 边界证据补齐（2026-10-05）

双独立调查发现：网络 interruptReceive 是持久 StopRequested；Windows adapter 持有跨 receive lease，不能直接 reset 后恢复。可复用有限 read timeout，但尚需 planner 验证其小于 purge acknowledgement timeout。另，非协议触发的重采集会推进 playback target，而 protocol groupGeneration 未必推进；不得把两者无条件当作同域。timestamp aligner 与 projector 当前均无状态，先前“需重置 aligner”的建议不适用当前实现。

在实施完整屏障前，先对 r91 缺失的实际包身份取证。binder 仅在首次期限建立、到期和已有锁失效三个边界记录原始包 SSRC/generation/sequence、旧锁 calibration、精确 source transition；不改超时、丢包、失败或恢复行为，不增加常驻队列。依照 GStreamer flush 的 owner 串行边界与 ffplay serial 消费规则调查，但不以源码类比代替原规格运行事实。此诊断不是修复完成；待双审、Release 全量重建与同规格 realtime 复现。

r93结果：诊断单文件A/B均PASS，Release全量重建exit0。原规格四源各完整120秒自然exit0；两次四格画面更新、全失活全黑持续输出；末位AV再完整120秒重入（右下格恢复、其余黑色）后自然exit0，再次全黑持续。未出现expired，但r91原触发包未知，不能宣布确定修复。诊断证明新音频rawGeneration1与恢复protocolGeneration2不相等。231条内部漂移raw phase为0～156ns，未做线端音频质量验证。WS两次全黑约284MB/311MB，总内存回收和长稳仍未通过。命令、指标、自然/清理退出区别、14件产物清理见运行记录。下一步仍需完整owner屏障、每流事实分类与已激活源等待合同；不能仅凭此运行将完整门禁改为PASS。

## r94 实施范围：原始媒体 owner 屏障

复用 GStreamer flush 的 owner 串行清理与 ffplay 入队serial/消费分类原则：将实际Raw和RTP binder纳入同一源CanonicalLineage参与者；Raw在基类pending transfer处理前清已完成AU、部分AU、已取得batch及prepared replay媒体，发布本流精确purge元组对应的最后生产序号。binder先等待本流frontier，清自身持有媒体及期限，随后丢弃sequence不大于cutoff的channel残留。固定每实际成员一个槽，无历史队列或新线程；发布后使用既有purge进度唤醒，ACK不等待新SR或keyframe。

协议clock/event事实继续按原顺序传播，不修改protocol groupGeneration、不冒充playback代际，也不清除健康tracker。Raw网络等待复用现有有限timeout，planner要求其严格小于acknowledgement timeout；不调用持久interrupt/reset。该约束不声称硬实时调度保证。包身份消费将按实际calibration的SSRC/raw generation核验，不能混用group generation。首次准入与已激活源缺clock期限、全部clock分支的退休边界仍须单独闭合；本切片不宣称完整恢复合同完成。

r94首轮审查FAIL的三项已修正：Raw按producer撤销ledger未领取waiter（granted但未领取的额度归还，已发出的lease不受影响，沿既有promoteOne唤醒后继）；当前batch只标记RTP退休，保留原cursor/lease，正常循环按原节奏消费RTCP，新batch/耗尽/reset均清标记；binder恢复在读取新clock前校验既有期限，只有已取得所有等待包可用的时钟事实才清期限，不允许迟到首次Locked绕过失败。首轮Release构建exit0只对应修正前，18文件修正版重新冻结双审和全量重建。

18文件修正版双审PASS且Release重建exit0，但r94真实预检暴露Raw的rtp.sync_group未登记于CommonCore shape合同，CLI自然exit1。已补齐精确RawRtpInput/kind-key合同，仍检查必填、域一致及未知字段拒绝。19文件最终切片再次由A/B独立复审均明确PASS；正在重新全量构建和原规格验证，源码PASS不等于验收通过。

### 2026-10-05 r95/r96：屏障运行证据与共享链路回归

19文件最终双审PASS、Release全量重建exit0。r95原四源V+V+V+AV及末位AV重入均完整120秒自然exit0；HEVC VBR 8000/8000/12000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP，四格→全黑→右下恢复→全黑，两次owner/binder逐流cutoff一致、无expired、运行错误0。r96同规格单路HEVC CBR8000kbps播放正常，源exit0，BYE后CLI经既有ProgressTimeout自然exit1；payload最终0B/0对象、71903预留=释放，不能标为优雅结束成功。独立复核确认无证据将该既有退出归因本次修改。r95/r96共20件临时产物及精确进程已清理。

剩余门禁：全部clock分支退休、已激活源的等待期限、端到端音频/同步、内存长稳、显式停止/可信EOF、C API及Windows通过后的RKMPP；本轮有限证据不等于合屏完成，不提高质量评分，不推送/合并/部署。

## r97前合同复核：修正过宽假设

A/B独立只读调查确认：startup clock与LockedPacketGate已有generation arbitration及退休分类，不能声称所有旧snapshot必然倒灌；fanout/adapter单分支FIFO亦不等同倒序。实际缺口是binder acceptClock未在更新本地锁前做退休分类，且接受时有效不保证后续投影/pending输出仍有效。协议groupGeneration、raw tracker代际与playback代际不能直接比较或重标；非协议重采集推进playback而协议可能不变，必须由上游合同闭合映射。

期限策略可复用plan.sourceLifecycle与preservesActivatedOutput（实际输出域激活事实）。首次准入及FailSession模式仍失败；已激活源候选驻留应有界退休并等待更新事实，不能只catch错误、清deque或提高timeout。requestReacquisition的RecoveryBudgetExhausted会推进playback代，不能拿raw/protocol代直接调用；需同时覆盖channel残留、Raw部分AU及关键帧重新准入。依据：[GStreamer jitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)的有限驻留与[flush设计](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)的owner清理；这些原则不证明本项目映射正确。

先用r97真实源重入音频缺SR诊断取证。两路都缺SR时Raw的waitingForClockEvidence会挡住AU，不足以触发binder；故仅停音频SR，视频仍提供权威证据。该项不改变媒体规格，也不替代正常验收。

### r97：已激活源等待音频SR的确定失败

四源原规格完整120秒后仅令末位AV重入音频skip_rtcp（视频SR正常），node65于11:51:08.527建立首次期限、11:51:18.527精确10秒到期。日志preserves_output1、phase2、300待绑定视频包；CLI自然exit1。明确证明r94媒体退休屏障不能替代已激活源等待合同。初始四源exit0，失败后诊断源中止exit=-1，10件产物和精确PID清理完成，实际命令及全部指标见wiring-runtime。

下一修复必须把候选媒体驻留退休与协议/playback代际关系作为一个合同：首次准入仍失败，输出已激活时仅隔离失活源并保留黑场/静音持续输出；不得借用推进playback代际的重采集请求来盲目清包，也不得只抑制错误。先明确clock消费/输出授权与非协议重采集映射，再实现有界候选退休，双审、Release重建后复验该失败条件以及正常SR原规格链路。

## r98实施设计：未绑定AU驻留丢弃

重新核验builder链：Raw完整AU→binder→generation gate→canonical→startup关键帧/preroll窗口→epoch/preparation/releaseExtractor→decode。故r97尚未绑定AU的驻留丢弃不等于generation flush，无依据强制清Raw部分同代AU或新建退休协调器；A已修正先前过宽建议。

在现有generation arbitration内确认PreserveActivatedOutput、独立输出已激活、源Acquiring且publication关闭，仅当本owner没有已绑定lookahead/pending输出且队列候选尚不能Bind时，期限到期清本owner候选并进入显式丢弃状态。持续消费AwaitClock/Retired完整AU，不再反复建立期限；真正classifyPacket=Bind且运行代分类为当前Target才退出，原startup窗口负责关键帧与preroll。原始/protocol/playback代均不改。purge与stop清状态；首次准入及共享FailSession保留原期限失败。沿用已有容量和期限产品，不新增公开字段、线程或队列。该合同证明本owner驻留与整体内存有界，不声称channel每包严格年龄，也不代替非协议代际映射门禁。

r98两文件实施后A/B独立源码审查均PASS：未绑定候选驻留丢弃入口、持续排空、Target校验后恢复、purge/reset清状态及首次失败语义均成立。fresh在这里表示真实包身份与Target calibration匹配，不是revision变大或墙钟年龄证明。非协议重采集的protocol→playback映射仍未关闭。Release正在全量重建，待r97同条件及恢复正常SR实流验证。

### r98：候选驻留修复及失败条件复验

两文件修改双独立PASS、Release全量重建exit0。原四源H264/AAC→HEVC VBR8000/8000/12000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP，各完整120秒；随后缺音频SR与恢复正常SR两个重入源也各完整120秒自然exit0。原r97失败点改为单次退休301个未绑定AU，黑场输出持续；正常SR恢复后Target2媒体重新绑定，右下格及第2代音频内部同步恢复，再次断流继续全黑。CLI运行errors/workerErrors/stalls0。17件产物及8个精确PID清理完成。

该修复不改变generation、不新增Raw退休事务，复用解码前startup关键帧门禁。仅证明已激活Acquiring阶段候选过期不再杀死会话且可恢复，未关闭非协议代际映射、clock完整退休、线端静音/音频质量/同步、长稳、停止/C API和RKMPP。共享单路新版本回归r99正在执行，未推送/合并/部署。

r99共享单路新版本回归完成：原120秒源exit0、两次VLC画面更新，BYE后CLI沿既有ProgressTimeout自然exit1；无新增候选丢弃分支，最终payload0B/0objects、71901预留=释放。完整优雅结束仍未通过。6件产物及精确进程清理完成；本轮r98/r99共23件临时产物无残留，指定源保留。r98两文件双审PASS、构建成功和缺SR失败条件修复证据成立，完整合屏仍按剩余门禁推进。
