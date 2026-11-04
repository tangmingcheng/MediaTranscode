# MediaTranscode Quality Score

### 2026-10-16 r236–r238：CUDA隔离与关联诊断完成，完整门禁仍75/FAIL

CUDA canvas私有nonblocking stream、只读黑底ready事实、默认stream host完成屏障与RAII已实现；r236约73秒物化超期，r237约21秒scheduler迟交，均完整归档且清理。reservation三谓词与单head缺流等待诊断经双独立审查、Release全量重建；r238运行约319秒无runtime错误，四次120秒源exit0，caller_stop0，payload230047预约=释放且归零。VLC原生快照覆盖三格、首次全黑与重入；播放器暂停来源不明，末次失源黑屏未证实，未记完整通过或根因修复。详见r236/r237/r238验证记录。

独立质量复核维持75/100。C API已经实现（含已批准输入事实和音频目标码率），其完整矩阵及旧ABI回归尚未闭合，不再标作未实现。确定缺口仍为非cadence SourcePresentation入口、四源HEVC输入重排、内容级AV/静音、完整Windows与后续RKMPP。下一实施先做会话级可消费保证准备额度并接实际allocator，区分compressed AU、decoded metadata和runtime像素收费；不得用准备预算除逻辑像素、均分预算或删除门禁。发送期限问题保留，后续原规格验证同时采集同AU等待与播放状态。无外部FFmpeg改动、无推送合并部署。


### 2026-10-14 r190–r193：启动显示点合同及source分段，整体保持75/FAIL

完成typed startup跨度、Running顺序和真实帧证据校验、Source-AU代际门、解码/帧处理配置与builder分段。双独立静态审查及冻结Release全量构建通过。r193三路H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps及120秒重入获双独立有限回归PASS：caller_stop0、drain265ms、RTP171670连续、两流DTS递增、完整AV解码0。六张VLC原生快照覆盖三格更新、首次失源黑屏与左下重入；末黑图在CLI停止后取得，不宣称第二次失源停前黑图。pressure47546、VLC迟到/converter告警保留。详细命令指标见realtime-composition-output-drain.md。

SourcePresentation生产仍关闭。下一步将typed assembly接到前置唯一decoder和启动缓存，完成filter/aggregate显示血缘及准备资源计费，再验收HEVC重排。当前结果不关闭内容AV、旧deadline或RKMPP门禁。


### 2026-10-14 r189：冻结解码显示点投影，整体保持75/FAIL

AU保存原SR校准锚、斜率和共同epoch，probe/runtime对实际decoded PTS共用投影，不补源duration。修复大tick先浮点相减及绝对NTP时间相消；整数差溢出显式失败，不宣称全int64范围纳秒精确。冻结源码双独立PASS，最终Release全量成功。

共享canonical三源H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps及120秒选源重入完成：四次源exit0、CallerStop0、drain258ms、runtime归零；172578 RTP连续，两流DTS递增、完整AV解码成功。五张VLC原生快照确认三格更新/黑屏/左下重入/末黑。pressureFailures47572、VLC10迟到和5converter告警保留；实流证据双独立有限回归PASS；29件本轮临时产物已清理，7个媒体PID均不存在。

下一项仍是同一decoder前移到startup，真实显示点跨度满足原正preroll、音频真实sample coverage与共同激活、aggregate持有/失效清理及surface重叠预算。production SourcePresentation仍拒绝；不能以本次canonical回归替代HEVC重排、完整资源门禁、内容AV、旧deadline及RKMPP验收。


### 2026-10-13 r187/r188：源时序与连续准备接线，整体保持75/FAIL

r187已完成同一probe的Prepared身份路径实现、全量Release与现有canonical三源有限回归；四张VLC原生快照、全AV解码及退出结果已双审归档，26件临时产物清理。新身份目标尚未生产启用。

r188将cadence/presentation事实贯穿source facts和候选planner，区分普通映射帧与重排见证；准备filter允许明确省略未知frame_rate。连续准备admission与runtime retention分离，FIFO及包装对象按实际分配收费、带owner移交。冻结源码双独立审查通过，全量Release构建成功；三源及重入真实链路、五张VLC原生快照、175575包RTP连续性、两流DTS及完整AV解码获双独立有限回归PASS。CLI为CallerStop0，排空255ms；pressureFailures47557及VLC迟到、converter告警仍保留。

下一完整垂直接线仍为同一generation gate→decoder→显示点startup→filter→aggregate，并保留音频共同激活、purge和实际存储边界。当前typed runtime gate明确拒绝未规划的presentation合同；不能用已有三源回归替代HEVC重排。完整资源准入、内容AV、旧deadline根因与RKMPP仍开放；Windows完成后才验证RKMPP。


### 2026-10-13 r185/r186：准备引用与身份记录分配推进，保持75/FAIL

快照引用改实际PMR准入；registry统一显式Runtime/Prepared资源，State/map/lease/opaque owner按owner生命周期分配和退休。各切片双独立源码PASS，最终Release全量成功。三源H264 720p30→HEVC 720p30/8Mbps CBR＋AAC192kbps MPEGTS/RTP及选源120秒重入，四次源exit0、CLI CallerStop0、drain261ms、runtime归零；174884 RTP连续，两流DTS递增、完整AV解码成功，双独立有限回归PASS。VLC原生5快照确认三格/全黑/左下重入/全黑；25迟到、converter及色彩警告保留。

Prepared registry尚未接入身份probe；完整准备存储、未知cadence接线、四源HEVC重排、内容AV、旧发送deadline及RKMPP仍未完成。下一步直接接入同一probe的typed身份与有界证据，再接准备/运行时合同，不能继续把三源回归当完整合屏完成。27件临时产物记录后清理；保留指定源和未提交改动。

### 2026-10-13 r184：分阶段诊断与VLC原生快照有限回归PASS，保持75/FAIL

已补有界mux出队/首次pop/attempt/发布观测，正常WouldBlock不打印失败。双源码PASS、最终Release全量成功；原规格三源120秒及AV重入120秒，CallerStop exit0、排空254ms、runtime归零，线端174953 RTP连续、DTS递增、完整AV解码成功，双独立有限回归PASS。五张VLC原生实时快照覆盖三格更新、全失源黑、左下重入与末黑；28件临时产物和7个媒体PID已清理复核，指定源保留。VLC2迟到及颜色警告仍存在。

本轮没有复现r183 deadline失败，诊断失败分支仍无真实样本，不判根因已修复。后续保持原规格取得可关联失败证据，禁止放宽期限；并继续完整准备存储准入、source身份/四源HEVC、内容AV，Windows通过后才RKMPP。完整合屏仍未完成。


### r183最终复验结果（2026-10-13）

最终Release成功、双独立源码PASS。相同2GiB额度已建图并输出，共享预约graph_bound高水位881967494B；真实链路于09:28:18.086发生datagram materialization超deadline4.891855ms，CLI exit1，完整验收FAIL。原三源和AV重入均120秒exit0；runtime资源归零仅为失败清理，AAC尾帧未排空。155283输出RTP连续、DTS递增、已捕获片段完整解码成功不能覆盖尾部。双独立运行审查FAIL，保持75/FAIL。用户要求后仅采用VLC原生视频快照；本次实际取得的是录制回放快照，不冒充实时图。32件临时产物及9个相关进程已复核无残留；指定源124427809B保留。后续沿绝对发送期限首次失败调用链调查，不能放宽期限；完整准备硬界、四源HEVC、内容AV和RKMPP仍待完成。详见realtime-composition-output-drain.md。

### 2026-10-13 r182/r183：共享账本源码通过，真实资源门禁仍FAIL，保持75/FAIL

r182原规格2GiB预检失败，暴露completed-AU probe重复协议预约；失败记录及清理完整，不记成功验收。r183复用同一planner/decoder执行器，按实际输入产品去重，双独立源码PASS。最终构建及原规格真实复验尚未完成；探测前准入、容量计费、HEVC身份、内容AV与RKMPP风险保留。

### 2026-10-12 r181：AU计费结算有限回归，保持75/FAIL

双源码PASS、最终Release成功；原规格三源及AV重入、CallerStop259ms、runtime归零、线端与完整AV解码证据成立。全会话共享根及准备owner寿命修正尚未完成，不能宣称内存硬界；VLC6迟到、颜色警告、source身份、四源HEVC、内容AV及RKMPP仍开放，不提升评分。

### 2026-10-12 r180：作用域纠正源码PASS，完整验收FAIL，保持75/FAIL

Release708项成功，双独立源码审查通过。真实三源/重入画面与现存线端片段检查成立；正常退出、drain和资源归零证据缺失，不算成功验收。29件临时产物已清理。全会话2048 MiB准备存储为用户委托选定的验收部署值，统一预算实现仍待完成；source身份、四源HEVC、内容AV及RKMPP门禁不变。


### 2026-10-12 r179：准备wire作用域合同FAIL，保持75/FAIL

计数/锁序源码双PASS，Release成功，但原规格三源启动容量失败；独立复审确认probeSize被扩大到整个continuous准备阶段缺少语义依据。新增代码仍为WIP，不计成功。需纠正阶段容量合同后同规格复验；四源HEVC、内容AV及RKMPP仍未闭合。


### 2026-10-12 r178：生产重放context隔离，保持75/FAIL

重开与跨等待准备owner经双源码PASS及最终Release，三路实际重开、重入/排空/线端有限双审PASS。NeedMoreEvidence运行覆盖、source AU身份/帧startup、四源HEVC、画质与VLC迟到、内容AV和RKMPP未闭合，不提升评分。

### 2026-10-12 r177：显式源呈现点合同，保持75/FAIL

共享RTP绑定、typed AU身份与信用owner修复经双源码PASS和最终Release；原规格canonical重入/排空/线端有限双审PASS。source生产拓扑未启用，准备资源及probe隔离未闭合，首轮三格画面缺证据、VLC13迟到、内容AV和RKMPP仍开放，不提升评分。

### 2026-10-12 r176：共享解码产品支持，保持75/FAIL

双源码审查与最终Release通过；三源及重入120秒、排空254ms与资源归零、线端连续性和完整AV解码有限双审PASS。新源AU路线尚未启用，VLC11条迟到、内容AV、四源重排和RKMPP门禁未闭合，不提升评分。

### 2026-10-12 r175：身份合同拆分与旧路径有限双审PASS，保持75/FAIL

源AU身份与完整canonical产品分离，registry生命周期复用，经两独立源码审查与全量Release。三源及选源重入均120秒，drain256ms、payload归零、线端序号/DTS/完整AV解码有限回归通过；两独立运行复审明确范围。新未知duration重排链尚未接线，早期缺格/VLC告警、内容AV、既有deadline及RKMPP仍开放，不能提升整体评分。详见合屏计划r174/r175。


### 2026-10-10 r160：聚合执行隔离及重入有限PASS，保持75/FAIL

独立音视频DAG owner接线、ready/activation、双purge及资源账户经双源码PASS和全量Release0。原三源与所选AV重入均完整120秒，现场恢复、wire序号/DTS/AV解码及payload归零有限通过；canvas100.767ms时音频built全程最大26.6717ms。scheduler仍90.6241ms、尾部未排空，VLC有late告警；不宣称端到端保证。完整内容AV、矩阵和RKMPP仍开放，详见合屏计划及C入口记录。

r157：现场三格两个时刻更新经双独立复核有限PASS；原规格运行sender期限超时FAIL，完整评分仍75/FAIL。资源回收不等于尾部排空，详见合屏计划与C入口记录。

### 2026-10-10 r156：目标stream同步等待定位，保持75/FAIL

有界CUDA阶段诊断双审/全量Release通过；原规格三源运行及线端解码、主动停止回收有限通过。canvas70.1879ms中确认destination_wait41.7732ms，但CPU量化0不等GPU耗时；未复现不代表修复r155超期。完整drain、内容AV、现场/完整矩阵/RKMPP仍未闭合。19件产物清理，详见[C入口与CLI记录](docs/realtime-video-composition-c-api-proposal.md)。

### 2026-10-10 r154/r155：首错修复，原规格超期仍FAIL，保持75/FAIL

共享完成链首错保留两文件双审/Release通过；Nsight原失败明确Thread32Next，不再被Cancelled覆盖。无分析器三源回归复现audio发送窗口耗尽；同次canvas144.6856ms占音频生成间隔144.9279ms主体，双审证据定位通过但GPU/线程等待原因未明。原错误完整上报，资源归零不是完整drain，现场/内容AV/矩阵/RKMPP未闭合，评分不提高。详见[C入口与CLI记录](docs/realtime-video-composition-c-api-proposal.md)。

### 2026-10-10 r152：CLI线端与回收有限双审PASS，保持75/FAIL

原规格三源120秒，CLI主动停止0、payload回收归零；wire序号/DTS/AV解码及三格抽帧有限双审PASS。现场仍被屏保阻塞，完整drain/内容AV/旧间歇deadline/完整矩阵及RKMPP未关闭。新增104.0526ms音频生成间隔内canvas调用86.2322ms墙钟关联，不能直接归因GPU。20件产物清理完成，详见[C入口与CLI记录](docs/realtime-video-composition-c-api-proposal.md)。

### 2026-10-10 r148：C入口生命周期有限双审PASS，保持75/FAIL

修复prepared输出ID与READY/Retired缺接线；原规格三源真实C调用方配置释放后运行、终态输出集合/SDP回收验证通过，双独立源码与运行证据限定PASS。现场已恢复桌面，完整截图取得黑场，活动画面截图受DPI裁切；wire完整三格与解码正常。不关闭旧间歇deadline、尾部drain、内容AV、旧ABI/旧路径/CLI回归、完整矩阵或RKMPP，评分不提高。24件产物及空目录已清理，见[C入口记录](docs/realtime-video-composition-c-api-proposal.md)。

### 2026-10-10 r145：三源同时重入有限双审PASS，保持75/FAIL

原规格三源及三源重入六次均完整120秒，CLI caller_stop0；wire序号/DTS/AV解码正常，四个出口抽帧确认三格→黑→三格→黑。现场截图因Screen-saver/Default桌面差异未闭合；旧期限失败未复现但未修复，内容AV、完整drain、完整矩阵、C API实调和RKMPP仍开放。两个独立审查者限定PASS，评分不提高。31件产物记录后清理，详见[实流记录](docs/realtime-video-composition-wiring-runtime.md)。

### 2026-10-10 r144：诊断及线端有限双审PASS，保持75/FAIL

新增聚合有界阶段计时双独立源码PASS，Release全量及三源HEVC CBR8Mbps／720p30／AAC192kbps RTP→MPEG-TS/RTP、所选AV完整120秒重入运行结束。两独立运行复核限定PASS；本轮未重现r143超期，不能记为已修复。线端RTP序号、DTS与完整AV解码正常；VLC录制另有DTS警告且截图失败。payload归零，但关闭AAC2帧、encoded差1包和mux pending188B尚非完整drain。画面、内容AV、间歇期限根因、完整矩阵、C API真实调用方、Windows后RKMPP仍未完成。评分不提高，详见[实流记录](docs/realtime-video-composition-wiring-runtime.md)。

### 2026-10-09 r134：三源HEVC CBR门禁FAIL；C API获授权实施

当前Release三源V/V/AV、原120秒、HEVC CBR8Mbps/720p30、AAC192kbps→MPEG-TS/RTP于输出scheduler约334ms晚到时报缺source coordinator，CLIexit1；三源均完整3600帧exit0，重入未执行、截图被遮挡。连续输出错误复用显示/源恢复策略，不能补source coordinator、调阈值或丢HEVC参考包规避。需typed encoded-AU调度合同复用现emit=dispatch-transportLead及下游真实deadline，查实际迟到来源后原规格复验。12项临时产物清理完成（两占用文件重试后成功），六PID无残留，原源保留。

用户已明确批准独立C合屏入口与音频协议事实，以及后续最小audio_bitrate_bps目标字段（正值、精确kbps、无默认，不称CBR）。Beta接线实施中，旧ABI及全部原验收门禁保持。整体75/FAIL不变，Windows完整通过前不进入RKMPP；无外部FFmpeg改动、无提交/推送/部署。


### 2026-10-09 r132/r133：快速SSRC替换回归已修复并有限双审PASS

r132原规格快速切换68.812ms复现准备期身份限制沿用runtime导致整图Unsupported/exit1；不是外部FFmpeg问题。3文件修复由planner下发既有生命周期模式，runtime仅允许授权SSRC代际恢复，准备期SSRC及全时PT/媒体契约保持严格；双独立源码PASS、Release全量exit0。

r133原120秒源、双路RTP H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP：切换58.694ms更快，代2恢复，纯视频及新AV各完整120秒exit0，输出RTP/TS/PTS连续。切换静音5.007506秒后非静音114.493106秒；仅VLC录制60秒抽帧证明双格恢复，现场恢复截图未取得，不能记现场视觉门禁通过。caller_stop exit0、payload115446预约=释放归零；encoded差1包/AAC关闭残留2帧，非完全drain。两独立运行复核均限定PASS，不能称无缝恢复。

r132失败13件、r133实际20件产物已记录后清理，各六PID无残留，原源124427809B保留。整体75/FAIL保持：内容A/V、现场恢复画面、完整矩阵、可信EOF、C API和Windows后RKMPP未闭合。下一项在既有完整矩阵验证中补现场恢复证据，继续优先Windows；未提交、推送、部署或修改外部FFmpeg。


### 2026-10-09 r131：所选AV源结束后重入，有限双审PASS

原120秒双源RTP H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP；初始AV、纯视频及重入AV均完整120秒/3600帧exit0。双格→右黑左画面→全黑→仅右格恢复有完整VLC证据，generation2持续工作；27.395556秒失源静音后非静音恢复约118.453秒。输出同SSRC、RTP/TS与PTS连续，caller_stop exit0、payload162868预约=释放归零；encoded差1包及AAC关闭时残留2帧，不计完全drain。两名独立复核均限定PASS：匹配BYE及失活/purge后重入；未覆盖快速SSRC切换、内容A/V、完整矩阵、可信EOF、C API或RKMPP，整体75/FAIL不变。

22项产物记录后逐项清理，六媒体PID无残留，原源124427809B保留。下一项为旧会话尚未失效时快速发送器替换的真实失败取证；不以本轮成功代替该边界，不盲删身份校验。命令和细项见runtime r131。无外部FFmpeg修复，无提交/推送/部署。


### 2026-10-09 r130：连续准备接收已实接，原规格启动静音回归通过

准备与runtime复用同一协议/时钟会话、AU产物及原接收线程，实际连续预算进入最终ledger。首次双审五项FAIL已修复，第二轮两者PASS；首次编译缺头文件已修复并双复审PASS，随后Release全量699目标exit0。

r130b双源原120秒H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP：两源3600帧exit0，逐源等待100/200ms仅为本次命令值。输入/输出RTP线端连续，TS计数及PTS步长正常，离线未再检测此前约3秒启动静音；双格与全黑完整VLC画面有效。caller_stop exit0，payload104992预约=释放且结束归零，encoded差1包不记完全drain。WS峰241356800B/private峰725454848B；内部漂移155..156ns不代表内容同步。

线端无缺包不能单独证明应用消费全部包；本轮无恢复，完整矩阵、内容A/V、可信EOF、C API及Windows→RKMPP仍未闭合，整体门禁维持FAIL，不调整既有75分。实际命令、PID、失败尝试及数据见runtime r130b；未更改外部FFmpeg，未提交/推送/部署。


## 2026-10-08 r123：共享时钟会话提取通过，完整合屏仍 FAIL

RTCP parser/tracker/schedule单owner封装双审与Release通过；原120秒H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验caller_stop exit0、payload归零，private峰717029376B。停读7.717秒、2710缺包在线端完整、启动静音3.115941秒，内容A/V及画面证据不足；prepared接管/持续接收和完整门禁未完成，保持75/100。[实流记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-08 r122：稳定引用基础通过，完整合屏仍 FAIL

immutable entry、PMR snapshot及实际控制块计费双独立审查PASS，Release及原120秒H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验完成。private峰714657792B（较r121减少约363MB），caller_stop exit0、payload归零；停读7.689秒、2691缺包在线端完整、启动静音3.115102秒，内容A/V UNKNOWN。保守allocation cap增加，持续接收/协议交接尚未实现，完整矩阵和Windows→RKMPP等门禁未通过，保持75/100。[实流记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-08 r121：最终准备存储账本接入，完整合屏仍 FAIL

共享allocation身份唯一计费、runtime arena重叠与节点归属筛选已接入，两个独立源码审查PASS；Release全量构建及双路realtime运行成功。原120秒H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP仍停读7.705秒，2691缺包在线端完整，启动静音3.138231秒、private峰1077166080B。caller_stop exit0、payload93426预约=释放/0B/0对象为局部证据；内容A/V、恢复、完整矩阵、可信EOF、C API及Windows→RKMPP仍未通过，保持75/100。见[实流记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-08 r120：容量分离双审通过，完整合屏仍 FAIL

Release及独立容量源码双审通过；原120秒双路H264 VBR 720p30实流仍停读7.633秒，2611缺包在线端完整，启动静音3.254467秒。CallerStop exit0与payload归零不能替代完整门禁；private峰约1.07GB，固定描述符成本待处理。内容A/V及完整矩阵、ledger、持续接收交接和RKMPP未通过，保持75/100。见[真实记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-08 r118：构建通过，容量契约审查 FAIL

PMR 接线完成编译，但同值 wire/allocation 上限会缩短原探测窗口，独立审查 P1 尚未关闭；最终 ledger 重叠计费亦未接入。本轮未跑实流，不构成运行修复验证，保持既有 75/100。见[准备交接记录](docs/realtime-rtp-preparation-handoff.md)。

## 2026-10-08 r117：完整验收仍FAIL

接收scratch复用/截断拒绝改动双审与Release全量构建通过；真实双路H264 VBR 720p30仍停读10.089秒、应用缺4867包而线端完整，启动静音0.817982秒，后续发送deadline失败。payload最终归零不构成长稳或停止通过；VLC画面/内容同步证据不足，PMR未接入。保持既有75/100，不因局部改动重新评分。见[本轮完整记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-05 r114：持续接收与真实容量设计调查

已形成[准备接收交接设计](docs/realtime-rtp-preparation-handoff.md)：全源共享设备/几何和全图准入阻止直接提前运行单源DAG；选择能力证据与live发布分离方向，继续复用原协议与startup节点。新确认MediaUdpSocket最大容量vector经resize后仍保留capacity，prepared仅size记账；r113的4173×65507=273356631B为元素容量下界，不是heap测量。Linux prepared旧接收还缺截断检测，runtime adapter保护不能替代。

真实容量/租约预算、协议状态连续移交、首次live准入/接管三个具体决策尚未闭合，完整设计未PASS，不先写rolling策略或宣称修复。无新增公共参数/外部依赖，本轮尚无功能代码变更或新实流测试，r113证据和完整FAIL/75分保持。

## 2026-10-05 r113：启动缺口已定位至本机接收覆盖

单文件prepared capacity_stop/sealed诊断，修正快照竞态及Result访问后双独立PASS、Release全量重建成功。原双路H264 VBR8–12Mbps、720p30、AAC192，同原120秒源及约11秒错开启动，抓input/output304326包/drop0。实际prepared 5MB预算耗尽后停读8.534秒再seal；应用缺3557包在线端启动期对应序列全部存在，排除发送/抓包点缺失。停读尾后已覆盖约5MB，随后出现缺口，与有限socket耗尽高度相容；没有kernel计数，不唯一归因OS。主动purge晚于gap，非首次缺口原因。

启动静音再现2.270998秒；诊断成立不等于修复。CallerStop exit0、payload92095平衡归零，19件产物清理，原源保留。下一项为有限探测与持续live接收阶段、pin/驻留共同预算、稳定退休身份及单owner交接合同；不能直接弹出probe借用deque或猜测扩容。完整功能FAIL/75分及其他门禁保持，未推送/合并/部署；详细命令和因果边界见runtime记录。

## 2026-10-05 r112：双路 H.264 VBR 与启动交接取证

原120秒V左/AV右、选定index1，错开约11秒并让右源完整120秒重入；H264 VBR目标/最低8Mbps、最高12Mbps，1280×720/30与AAC CBR192kbps→MPEG-TS/RTP。172858包/drop0，RTP/TS/PTS/PCR连续；五帧支持双路、仅左、黑场、仅右恢复、黑场及DAR留边。CallerStop exit0，payload170757平衡归零。双独立有限PASS，27件产物已清理。

完整音频门禁仍FAIL：额外启动静音0.085873–2.527324秒（原源同阈值无静音），对应输入video序号10048→13450、时钟失效和重新锁定。独立代码调查确认并行capture在非probe阶段预算耗尽可无错误停止drain，runtime先replay后live；未记录本轮预算命中/停止时刻且未抓input，不能据此认定丢包根因。下一步保持相同源规格及启动间隔，关联入站包、capture停止/预算、seal、replay排空及首live序号。禁止猜测扩容或调整规格。内容同步UNKNOWN、r105因果/剩余矩阵/长稳/C API/RKMPP仍待完成，保留75/100。r111共享单路完整结束仍ProgressTimeout/exit1。见runtime完整命令和结果。

## 2026-10-05 r110–r111：有限证据更新

双独立复审：r110 HEVC CBR四源原120秒及AV重入120秒，原始182888包连续、CallerStop资源归零；内容重复多解，内容A/V为UNKNOWN，重入PID证据不足。r111共享单路原120秒运行与资源回收成立，但源结束后仍ProgressTimeout/exit1，优雅结束FAIL。保留75/100及完整合屏FAIL，不凭有限运行重评分。RC/布局矩阵、内容同步、长稳、可信EOF/C API、r105因果闭环和RKMPP继续未过。详见[运行记录](docs/realtime-video-composition-wiring-runtime.md)。


## 2026-10-05 r109：NVENC输出缓冲契约已实现

内部Immediate产品由planner选择，生产与所有prepared/probe路径共用官方delay=0/zerolatency=1应用及计划值回读；无新增外部参数、依赖修改或规格降低。修正主probe漏校验与FFmpeg只读查询const接口问题后双独立源码PASS，Release全量重建成功。原四路120秒及选定AV重入120秒均完成；CallerStop exit0，payload202418预留=释放、最终0B/0对象。

原始线端180秒覆盖黑场→重入→黑场：94048 RTP包/drop0、无RTP序号及TS CC缺口，视频5400包步长3000、音频7751包步长2089/2090，PTS/DTS无回退。四个离线画面采样支持四格/黑场/右下恢复/黑场；不代表内容级A/V同步、首轮失源边界或长稳。r105未复现但根因未闭合；完整Windows矩阵、共享单路回归、音频内容同步、长稳、C API及RKMPP仍未完成。保留此前75/100及完整功能FAIL，不以局部修复重评分。详见docs/realtime-video-composition-wiring-runtime.md。

## 2026-10-05 r104–r106：发送窗口失败与恢复证据

r104/r105原规格Windows四路RTP/RTCP→H264 CBR 8000kbps、1280×720/30＋AAC CBR192kbps→MPEG-TS/RTP失败。r105同一音频AU在emit前17.922139ms已到scheduler，却因缺video等待，mux接收时晚于dispatch133.956661ms；此前maintenance超过dispatch11.655693ms。当前合同没有缺流下一AU的权威DTS下界，不能猜测帧率或放宽窗口规避失败。

r106仅增加成功提交时刻诊断，双独立源码PASS，Release全量684任务成功。四路完整120秒→全黑→选定AV完整120秒恢复右下格→全黑→Ctrl+C正常exit0；payload0B/0对象、203332预留=释放。接收324.185644秒录制支持画面恢复和失源静音，但VLC重新封装启动PTS有一次回退，原始线端连续性、选定音频内容与端到端同步仍待验证。r104/r105未修复，完整功能FAIL；保留独立75/100，不据单次未复现重评分。21件临时产物已逐项清理，命令、指标与证据边界见docs/realtime-video-composition-wiring-runtime.md。未运行local CLI/RKMPP，未提交、推送、合并或部署。

## 2026-10-05 r100–r103停止门禁进展

信号适配器四文件双审PASS、Release重建成功；r103四路120秒后黑场持续，再定向Ctrl+C，CLI CallerStop/exit0、worker0、payload0字节/0对象、120996预留=释放，双独立证据复核PASS。r102合屏准备取消为Cancelled/exit1；r100宿主终止和r101预检超时不算通过。取消不保证AAC尾帧排空，活动媒体停止、可信EOF、完整clock退休、线端音频/同步、长稳、C API和RKMPP仍缺。保留原独立75/100与完整功能FAIL，不凭局部停止证据重评分。详见[真实记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-05 r98/r99恢复期限证据更新

保留此前独立评分75/100，完整功能仍FAIL，不以局部修复重新评分。r98两文件双独立源码PASS、Release重建成功；原四源、缺音频SR重入及正常SR恢复均完整120秒，候选超时改为有界丢弃、全黑输出持续，正常时钟后恢复右下格。r99共享单路运行正常，结束仍为既有ProgressTimeout；payload最终归零。非协议代际映射、完整clock退休、线端音频/同步、长稳、停止/C API及RKMPP仍未关闭。命令与局限见[真实记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-04 混合 AV＋VideoOnly 合屏阶段复核

独立审查者A沿用原六维20/20/20/20/10/10，当前保守评分 **75/100**；这是合屏就绪度，完整功能 **FAIL（未完成）**。历史66/72及42分记录保留，不覆盖全仓评分。

| 维度 | 满分 | A | 当前证据与缺口 |
|---|---:|---:|---|
| DAG复用与多源绑定 | 20 | 18 | r75四路AV、r85双路AV＋V复用生产DAG；C API映射与3/4路混合仍缺。 |
| 多源时钟与合成来源 | 20 | 16 | 实际成员、独立输出与V generation2合同成立；内部servo不证明端到端A/V同步。 |
| 逐源断流和恢复 | 20 | 15 | r85全黑、V/AV分别原120秒重入及再次结束有画面证据；静音/音频恢复实测、明确stop仍缺。 |
| 线程、背压和资源边界 | 20 | 14 | 成员化预算及双审、运行0errors/stalls；四路驻留未回基线、长稳及完整回收终态待证。 |
| 双平台合成能力证据 | 10 | 5 | Windows四路AV及双路混合有限验证；三路、3/4混合及RKMPP未通过。 |
| 合屏观测与真实验收 | 10 | 7 | 命令、PID、自然源退出、画面与资源记录可追溯；协议/编码/RC矩阵、端到端音频质量仍缺。 |
| **合计** | **100** | **75** | **完整功能FAIL** |

r85四次源均完整120秒自然exit0，108658输出包、errors/workerErrors/stalls均0；CLI测试后清理不记自然结束，有限内存采样不证明长期稳定。r84 factory失败及此前审查遗漏保留；最终Extractor六事务成员修复已源码双审，但源码PASS不替代验收。r86单源共享AV源结束后无进展超时自然exit1，完整回归FAIL；最终payload归零、预留/释放平衡。未据此提高评分。详见[真实记录](docs/realtime-video-composition-wiring-runtime.md)。

## 2026-10-04 双路合屏恢复与四路准备门禁

两名未参与实现的智能体按下方原六维独立重评分，分差保留；本轮源码修复双审PASS，完整功能均FAIL（未完成）。本项为合屏就绪度，不覆盖历史全仓评分。

| 维度 | 满分 | A | B | 当前缺口 |
|---|---:|---:|---:|---|
| DAG复用与多源绑定 | 20 | 16 | 17 | 双路AV生产DAG运行；纯V非音源、C API映射未接通，四路准备失败。 |
| 多源时钟与合成来源 | 20 | 14 | 16 | 输出域/来源合同与重入时钟有证据；持续播放A/V质量待证。 |
| 逐源断流和恢复 | 20 | 13 | 15 | r73黑场与逐源视频重入成立；静音/音频恢复、可信EOF及正常stop待证。 |
| 线程、背压和资源边界 | 20 | 13 | 12 | r74准备预算耗尽；长稳及完整reset后的归还终态待证。 |
| 双平台合成能力证据 | 10 | 4 | 5 | Windows双路已运行；三/四路及RKMPP未通过。 |
| 合屏观测与真实验收 | 10 | 6 | 7 | 已有命令、采样画面、drift及资源记录；完整规模/协议/RC矩阵未完成。 |
| **合计** | **100** | **66** | **72** | **完整功能FAIL** |

r73约7分钟、四次120秒源均自然exit0，双路/全黑/逐源恢复画面已观察且核心0errors；截图不证明持续无卡顿或音频恢复。CLI测试后清理不记自然结束。r74四路尚未建图，早源完整历史与合法probe描述符挤占5MB总预算，不是payload重复拷贝或CUDA滤镜分辨率错误；该准备边界仍待修复。两位均确认报告保留上述失败与证据限制。详见[真实运行记录](docs/realtime-video-composition-wiring-runtime.md#r73双路-rtp-h264aac--mpeg-ts-over-rtp-hevcaac1280720-30fps-8000kbps-cbr2026-10-04)。

## 2026-09-28 输出池驻留双审

基线662c4d25后11源码，最终拓扑推导canvas循环surface/发布lease和固定黑底，共用pool算术及encoder回读校验。双独立Standards/阶段Spec PASS；数量合同不授权分配，准备预算、aggregate多源账本、公共入口及双平台门禁仍缺。六维10/8/6/12/2/4，总42/100，完整FAIL。见[驻留记录](docs/realtime-video-composition-pool-retention.md)。

## 2026-09-28 最终逻辑拓扑与绑定双审

基线2b3e7b24后16源码，move-only最终拓扑和物理绑定分离，canvas几何/存储分型，资源选项共用。首审几何/候选界前置校验缺口已修，双独立Standards/阶段Spec PASS。准备授权、aggregate多源总账及公共入口仍缺；六维10/8/6/12/2/4，总42/100，完整FAIL。见[本轮记录](docs/realtime-video-composition-topology.md)。

## 2026-09-28 真实接线依赖核查

基线a7836de5源码核查发现实际encoder/canvas与最终图资源编译存在准备依赖环，单链峰值账本及aggregate保留合同不足以支撑多源总准入。下一步先分离最终逻辑拓扑、总合同与强制物理绑定；本轮仅文档诊断，无新运行证据。六维10/8/6/12/2/4，总42/100，完整FAIL。见[接线核查](docs/realtime-video-composition-wiring-gate.md)。

## 2026-09-28 源runtime装配双审

基线c930cf66后16源码，源同步域与A/V源runtime装配共用既有政策、源clock/correction/边界和transition。首审漏校验的transport一致性、音频索引、线程合同已修正，A/B最终Standards/阶段Spec PASS；B撤回首次不充分PASS。多源公共调用、纯视频源、总准入及双平台真实门禁未完成，六维10/8/6/12/2/4，总42/100，完整FAIL。见[源runtime装配](docs/realtime-video-composition-source-runtime.md)。

## 2026-09-28 源校正容量双审

基线4e7bd5e7后8源码，两位独立最终Standards/阶段Spec PASS。旧共享链复用源容量，组合图重算源校正产品；首审命令mailbox事实映射缺陷已同步修正为实际frame事务边。校正可达性余量不是全源资源总账；完整source runtime、资源事务、多源入口和双平台门禁仍未完成，六维10/8/6/12/2/4，总42/100，完整FAIL。详见[源校正记录](docs/realtime-video-composition-source-correction.md)。

## 2026-09-28 源时钟规划双审

基线400ffc4a后10源码双独立复审Standards/阶段Spec PASS；源timing与canonical assembly一次规划并由旧生产链直接消费。首审独立入口demux非法代次/阈值缺口已通过共用原源clock validator关闭。容量校正、多源准备、唯一资源事务及双平台门禁仍缺，六维10/8/6/12/2/4，总42/100，完整FAIL。详见[源时钟记录](docs/realtime-video-composition-source-clock.md)。

## 2026-09-28 独立输出装配双审

基线d86523ac后11源码双独立复审Standards/阶段Spec PASS。首审发现timing与真实协议产品缺少交叉校验，已复用resolveOutput完整比较并复审关闭。旧生产caller消费独立输出装配；源在途界、唯一资源事务、多源入口及双平台门禁仍未完成。六维10/8/6/12/2/4，总42/100，完整FAIL。见[装配记录](docs/realtime-video-composition-output-assembly.md)。

## 2026-09-28 输出同步政策双审

基线087baf55后3源码双独立Standards/阶段Spec PASS，旧RTP/TS/demux与copy/transcode值映射保持；输出政策有真实生产调用者。历史经验阈值工业依据仍缺，完整runtime装配/资源/多源入口及双平台门禁仍未完成。六维10/8/6/12/2/4，总42/100，完整FAIL。见[输出同步规划](docs/realtime-video-composition-output-sync.md)。

## 2026-09-28 输出runtime双审

基线3a539650后11源码双独立Standards/阶段Spec均PASS。独立输出产品解除源runtime继承，借用视图共用三个发送/调度segment；旧链等价、引用寿命及校验保留已核对。完整输出planner、资源事务与多源入口尚未接通，六维10/8/6/12/2/4，总分42/100，完整FAIL。详见[输出runtime](docs/realtime-video-composition-output-runtime.md)。
## 2026-09-28 独立音频编码段双审

基线5ae7ea9a后5源码，两位独立审查者均阶段Standards/Spec **PASS**。纯输出不再经过源mapper或要求源索引，旧节点/端口/配置复用；已核对借用寿命、lineage/FIFO/group与无源resolver路径。

六维仍 **10/8/6/12/2/4，共42/100**；完整合屏/验收/交付 **FAIL（未完成）**。输出runtime产品、资源事务、多源入口和双平台门禁仍缺，见[音频编码段](docs/realtime-video-composition-audio-output.md)。

## 2026-09-28 输出音频FIFO双审

基线c5878f36后6源码，两名独立审查者均阶段Standards/Spec **PASS**。源校正上界归入校正planner，FIFO消费纯输入块事实；旧公式和写入前硬界保持，aggregate合同构图前精确核验。

六维仍 **10/8/6/12/2/4，共42/100**；完整合屏/验收/交付 **FAIL（未完成）**。载荷界不证明allocator/RSS硬界，源/输出产品形成、资源事务和多源公共入口仍缺，见[FIFO规划](docs/realtime-video-composition-audio-fifo.md)。

## 2026-09-28 域角色校验双审

基线8099ca3a后13源码，两名未参与实现者均阶段Standards/Spec **PASS**。核对旧单源路径、源/输出合同、NTP与epoch权限、optional消费安全；没有新增公共参数或平台分叉。

六维仍 **10/8/6/12/2/4，共42/100**；完整合屏/验收/交付 **FAIL（未完成）**。新域产品形成、纯视频源、唯一资源/跨源总账与公共入口仍缺，见[域校验记录](docs/realtime-video-composition-domain-validation.md)。

## 2026-09-28 源恢复参与者双审

基线958b1a68后4源码，两名未参与实现者均阶段Standards/Spec **PASS**。核对真实source builder与runtime注册，精确源集合排除输出encoder/sender，旧共享集合顺序保持；合屏builder在构图前核验完整合同。

六维仍 **10/8/6/12/2/4，共42/100**；完整合屏/验收/交付 **FAIL（未完成）**。源runtime及多源生产入口、唯一资源/跨源总账和双平台门禁仍缺，见[源恢复参与者](docs/realtime-video-composition-source-transition.md)。

## 2026-09-28 协议输出规划双审

基线e9aa5fed后3源码，两名未参与实现者均阶段Standards/Spec **PASS**。输出协议规划已不依赖源decoder/clock，单源复用同一入口，保留源assembly/correction/transition/FIFO。未新增协议算法或公共参数。

六维仍 **10/8/6/12/2/4，共42/100**；完整合屏功能/交付 **FAIL（未完成）**。唯一资源准备、源/输出runtime、多源入口及双平台门禁仍缺；构建和真实结果见[协议输出规划](docs/realtime-video-composition-protocol-output-planning.md)。

## 2026-09-28 输出编码planner双审

基线`6ef4a6c9`后5源码，两名未参与实现者均给阶段Standards/Spec **PASS**。输出open、执行策略与VBV补全共用同一planner；完整链复用源/输出两个内部数据合同，保留单源不可变请求，移除独立输出未消费的源请求字段。

六维仍 **10/8/6/12/2/4，共42/100**；完整合屏/交付 **FAIL（未完成）**。缺实际唯一输出资源准备与多源事务、跨源总准入、纯视频及双平台真实门禁，验证边界见[输出编码规划](docs/realtime-video-composition-output-planner.md)。

## 2026-09-28 独立编码能力双审

基线`10978e42`后7条源码路径（含删除与新建），两名未参与实现者均给阶段Standards/Spec **PASS**。encoder候选复用原profile，旧整链消费同枚举；消耗性能力探测内部打开并销毁context，三个调用者复用同实现，设备/池引用符合FFmpeg生命周期。

六维仍 **10/8/6/12/2/4，共42/100**；完整合屏及交付 **FAIL（未完成）**。候选/能力探测不等于实际保留唯一输出资源；完整协调器、源/输出runtime、总准入、纯视频与双平台多源门禁未通过，见[独立编码能力准备](docs/realtime-video-composition-encoder-capability.md)。

## 2026-09-28 编码器帧输入双独立审查

基线`e6f411d9`后7个源码，两名未参与实现者均给出阶段Standards/Spec **PASS**；原始帧事实、planner尺寸/cadence、SAR/色彩保持和一次性能力探测生命周期符合现有FFmpeg接口语义。没有新增算法、平台链或公共参数。

六维维持 **10/8/6/12/2/4，共42/100**。完整合屏及交付 **FAIL（未完成）**：唯一输出候选/设备准备、完整事务协调器、纯视频源及Windows→RKMPP多源矩阵仍缺。源码审查不证明本轮构建、动态输出或媒体链路通过，验证结果见[编码器帧输入记录](docs/realtime-video-composition-encoder-input.md)。

## 2026-09-28 唯一输出合同独立审查 B

基线`6ffe4c3e`后冻结11个src文件：Standards/限定阶段源码 Spec **PASS**。首次构建发现optional<bool>初始化C2440，修复为图前存在性检查后解引用，false合法；新冻结源码复审通过，构建结果另行记录。合屏输出消费encoder-only产品，旧完整branch共用encoder映射、端口及lineage；datagram planner消费上游已解析emission，保持原wire规划及失败传播。未运行构建或媒体，不能预判主执行验证结果。

六维维持 **10/8/6/12/2/4，共42/100**。完整功能与交付/合并 **FAIL（未完成）**：唯一encoder候选准备、source/output runtime拆分、全preflight、跨源总准入、纯视频源入口及Windows→RKMPP多源验收仍缺；旧Shared无进展/4对象及AAC重入未证明修复。

## 2026-09-28 源端合同与准备画布独立审查 B

基线 `9c0d53a9` 后33个src文件：Standards/限定阶段源码 Spec **PASS**。source-only输入、视频与音频配置复用原segment；画布使用真实pool执行黑场及tile copy，plan/pool绑定和mutex单次claim贯通。错误路径RAII及Shared/local配置映射经源码核对；未运行构建或媒体。

六维维持 **10/8/6/12/2/4，共42/100**。完整合屏及交付 **FAIL（未完成）**：完整preflight caller、跨源总账、非选定纯视频、公共入口及Windows→RKMPP多源验收仍缺。局部surface上界不等于driver/RSS边界；旧Shared无进展/4对象和AAC重入未证明修复。

## 2026-09-28 准备解码器交接独立复审 B

基线 `7a7312ac` 后22个src文件：Standards/限定阶段源码 Spec **PASS**。复审确认首帧释放后flush同一decoder、单次claim、storage lease随context deleter穿过takeContext；参数副本及side data纳入逻辑额度，claim只读snapshot。设备必要性按typed帧合同检查，DRM PRIME不再被无条件hw_device_ctx门槛拒绝；共用retention校验与已有单源lineage保持。

六维维持 **10/8/6/12/2/4，共42/100**。完整功能与交付/合并仍 **FAIL（未完成）**：协调器尚未接入交接，缺跨源总账、画布/设备真实证明及Windows→RKMPP多源验收。flush为void，不等于RKMPP恢复证据；Shared无进展/4对象及AAC重入未证明修复。本审查不运行构建或媒体。
## 2026-09-28 RTP 首帧与硬件帧合同独立复审 B

基线 `f148bdca` 后18个src文件：Standards/限定阶段源码 Spec **PASS**，完整合屏与交付/合并 **FAIL**。只读快照消费复用 RTP/decoder 状态机，准备预算按并存关系形成独立逻辑额度；原截止时间、取消与采集结束显式传播。公共硬件帧校验由首帧和 output capability 复用，核对实际 format/sw_format、设备引用及分配尺寸；首帧另核对打开 decoder 的设备身份。

六维维持 **10/8/6/12/2/4，共42/100**。首帧消费者尚未接入完整准备事务，缺跨源 owner 总账及 Windows→RKMPP 多源运行证据，不能升分。逻辑存储预算不等于 allocator RSS 或全部 FFmpeg/driver 分配；Shared 无进展超时、4个逻辑对象残留与 AAC 重入问题仍未证明修复。本次只作源码复审，没有运行构建或媒体验收。
## 2026-09-21 decoder owner 与只读 RTP 探测租约独立评分 B

基线 `3a40b842` 后13个src文件的独立B最终复审：Standards/限定阶段源码 Spec PASS；完整合屏与交付/合并 FAIL。共用 decoder open 的 callback owner 随context稳定转移；只读快照同时计费原数据和副本，seal与租约互斥且不消耗生产replay。新lease尚未接入真实源prepare，不能用旧Shared单源回归外推其运行正确性。

六维维持 **10/8/6/12/2/4，共42/100**。仍缺实际AU/首帧与decoder交接、唯一canvas/output准备及总账、多源/双平台验收。probe占额时capture容量截止已显式传播粘性错误并阻止seal；释放lease不清除失败或重启reader，后续probe仍不能无限等待新数据。资源范围保持engine-managed硬边界与外部device/driver observed-only区分。本轮Release/真实120秒结果由对应实施记录独立给出，源码审查不预先判定运行通过。

## 2026-09-21 逐源视频规划独立评分 B

基线 `ffbf31a7` 后10文件冻结增量双审 Standards/阶段源码 Spec PASS；Release全量636项成功且构建前后源码哈希一致。独立B核对 [r24记录](docs/realtime-video-composition-source-planning.md)：指定120秒源自然exit0/3600帧，VLC首段有画面；七组purge ack后CLI仍无进展自然exit1，4个逻辑对象残留。运行的是Shared单源，并未验证新source-only入口或Preserve合屏。

| 六维 | 得分/满分 | 本阶段提升判定 |
|---|---:|---|
| DAG复用与多源绑定 | 10/20 | 公共源候选/执行契约已抽取，真实准备交接及入口未完成，不提升。 |
| 多源时钟与合成来源 | 8/20 | 仅单源generation1漂移证据，不提升。 |
| 逐源断流和恢复 | 6/20 | 无generation2恢复媒体，旧失败仍复现，不提升。 |
| 线程、背压和资源边界 | 12/20 | 无新增队列，仍缺按域总账且逻辑对象未归零，不提升。 |
| 双平台合成能力证据 | 2/10 | Windows单源协商/执行可用，缺RKMPP及多源合屏证据，不提升。 |
| 合屏观测与真实验收 | 4/10 | 117条漂移raw最大4793ns只覆盖首代；完整验收FAIL，不提升。 |
| **合计** | **42/100** | **完整合屏仍未就绪。** |

保留风险：graph config/readback不证明真实首帧执行、同设备身份或transfer；还需准备对象所有权交接、物理画布及资源准入。433个CPU样本整机平均1.049801%不能外推多路吞吐或长期稳定。文档明确记录失败与五文件/三PID清理，没有把源码PASS、播放截图或逻辑残留夸大为运行通过或物理泄漏。

## 2026-09-21 源生命周期与共用准备步骤独立评分 B

基线 `81e30778` 后冻结增量经独立双审，Standards/本阶段源码 Spec PASS；最终 Release 全量构建成功，61 个源码文件构建前后哈希一致。独立审查 B 核对 [r23 命令、结果与清理记录](docs/realtime-video-composition-source-lifecycle.md)：120 秒单源、3600 帧及 VLC 首段画面不代表合屏验收；源结束后七组 ack，CLI 仍因无进展自然退出1，保留4个逻辑对象。本阶段没有足以提高合屏就绪度的运行证据。

| 维度 | 满分 | 得分 | 本阶段证据与提升判定 |
|---|---:|---:|---|
| DAG 复用与多源绑定 | 20 | 10 | 准备步骤复用已抽取；多源 preflight/入口未闭合，不提升。 |
| 多源时钟与合成来源 | 20 | 8 | 权威代次与迟到证据分类源码通过；r23仅generation1，不提升。 |
| 逐源断流和恢复 | 20 | 6 | Preserve、真实purge及重试屏障源码通过；新模式未运行，旧路径仍失败，不提升。 |
| 线程、背压和资源边界 | 20 | 12 | 已修复续接/发布竞争；全局资源账未完成且逻辑对象未归零，不提升。 |
| 双平台合成能力证据 | 10 | 2 | 仅Windows单源回归，无本阶段RKMPP合屏实证，不提升。 |
| 合屏观测与真实验收 | 10 | 4 | 117条漂移最大4793 ns仅覆盖单源首代；无持续黑帧/静音及恢复证明，不提升。 |
| **合计** | **100** | **42** | **完整合屏仍 FAIL，尚未就绪。** |

风险与待优化：完成独立输出预检查、物理画布验证、按域资源准入及公共入口；覆盖多源反复失锁、真实恢复、长期内存和Windows→RKMPP完整链路。4个逻辑对象残留不能直接推断进程退出后的物理泄漏；源码审查不替代运行成熟度。

## 2026-09-21 合屏域与聚合接线独立审查 B

独立审查B对`2ff6bab0`之后当前合屏WIP给出**源码安全保留PASS、完整合屏FAIL**：交叉复核确认的RGA隐式plane布局P2已修复，完整布局及RGB24/BGR24字节序冻结复审通过。域注册、连续贡献、黑帧/静音及CUDA/RGA源码已对照FFmpeg/GStreamer；preflight、全局资源准入、CLI/C API和多源Windows→RKMPP验收仍未闭环。r22属于单源回归，不能替代合屏证明。沿用六维10/8/6/12/2/4，合计**42/100**，不提高分数；B结论不代替A或最终PR审查。

## 2026-09-21 编码提交事务复审

两名独立审查者 composition_transaction_review_a/b 均 Standards/局部事务 Spec PASS，未发现新增阻塞；合屏就绪度维持42/100（10/8/6/12/2/4）。Release全量构建成功；r21首段120秒有VLC画面，重入AAC timeline失败，CLI自然exit1、逻辑资源归零。完整合屏FAIL。提交事务前置不能替代独立输出生命周期、mapper驻留硬界、黑帧/静音及多源验收。[命令与结果](docs/realtime-video-composition-transaction.md)。

## 2026-09-21 输出身份与 FIFO 边界复审

两名未参与实现的智能体对源/输出身份、canonical 音频贡献保存、完整 timeline 校验及同步 FIFO 上界给出 Standards/局部 Spec PASS，直接 include 和同步组 contract 修复后复审结论保持。Release全量成功；r20重入AAC时间轴失败，CLI自然退出1、逻辑资源归零。完整合屏 FAIL，就绪度仍为 42/100（10/8/6/12/2/4）；尚缺独立输出生命周期、连续聚合、黑帧/静音、多源与双平台运行证据。FIFO PCM payload 界不覆盖 metadata/codec 物理内存，贡献尚未穿过调度到协议层。构建与实流结果见[本轮记录](docs/realtime-video-composition-output-identity.md)，源码 PASS 不替代运行验收。

## 2026-09-16 显式处理归属与 r16–r18 复审

两名未参与实现的审查者均Standards/局部Spec PASS，未发现新增P1/P2；合屏就绪度维持42/100（10/8/6/12/2/4）。segment显式归属、互斥覆盖校验与按角色准备注入已贯通，仍执行旧整体transition。Release全量重试成功；r16无源超时退出且最终保留4个逻辑对象，r17测试启动时序失败不计验收，r18完整双120秒源重入仍报AAC时间轴错误，CLI自然退出1且逻辑资源归零。完整合屏FAIL，连续输出、贡献模型、黑场/静音和跨平台验收仍缺失，不能据结构重构提高分数。[命令与证据](docs/realtime-video-composition-processing-ownership.md)。


## 2026-09-16 协议代次交接增量

两名独立审查者对owner线程清理、异步purge、协议/SDP注册及RTP事件顺序源码给出Standards/局部Spec PASS；r13七组ack并恢复源锁2、新MPEG-TS计划，随后AAC不支持flush导致编码lineage冲突，且漂移pending长期持epoch锁阻塞退出。短授权修复再次双审PASS，r14同规格Debug实流保留AAC失败并自然退出1、逻辑资源归零；完整恢复FAIL。旧r13挂起进程已按用户明确授权清理，r15 Release全量构建通过，同规格重入仍报AAC错误，但CLI自然退出1、逻辑资源归零，并实际取消53个未提交数据报；完整恢复仍FAIL。本轮产物与进程已清理。合屏就绪度维持42/100（10/8/6/12/2/4），未以基础修复提高评分；独立输出域、黑场/静音、多源与Windows→RKMPP验收仍未完成。[证据与命令](docs/realtime-video-composition-protocol-handoff.md)。

## 2026-09-16 purge屏障与恢复控制增量

两名独立审查者复审10文件源码增量，Standards/局部Spec均PASS：修复背压重试跳过代次仲裁、purge完成缺少域唤醒、startup重复失效/控制消息滞留，以及传输计划遗漏purge注册。r9/r10定位的错误已越过；r11全部5组ack并恢复源锁2，最终仍因MPEG-TS构造器保留旧计划而退出1。完整恢复FAIL，详见[purge屏障记录](docs/realtime-video-composition-purge-barrier.md)。就绪度仍42/100；剩余协议代次交接、sender线程归属清理、输出时间轴、黑场/静音及跨平台验收不能由局部PASS替代。

## 2026-09-16 源失活代次增量

两位独立审查对validator三阶段、重复失效幂等、old/next投影及耗尽失败均给出Standards/局部Spec PASS。r8实际解析两次匹配源BYE，next=2/old=1稳定，无原malformed discontinuity；120秒有编码与VLC画面，随后仍无进展超时。完整交付FAIL，六维就绪度保持10/8/6/12/2/4，共42/100；不外推完整恢复或跨平台。剩余恢复获取期限、purge并发、尾部和物理内存风险见[专项记录](docs/realtime-video-composition-source-generation.md)。

## 2026-09-15 阶段一基础修改复审

两位未参与实现者对阶段一及2026-09-16输入保留增量均给出 Standards PASS / 当前 WIP 源码 Spec PASS。本轮贯通输入启动 retention、字节/对象信用、binder与重复帧凭证生命周期及独立启动批次发布容量；r5、r6真实链路FAIL，修复后的r7已进入持续编码与VLC播放，最终结果见[输入保留记录](docs/realtime-video-composition-input-retention.md)。独立复审建议维持下表六维尺度42/100：仍是单源基础，多源绑定、公共合成域、逐源黑屏/静音与恢复及双平台合屏验收尚未完成。逻辑credit不代表clone side data等全部物理分配边界；控制进展、完整结束和Windows→RKMPP回归仍待闭环，不因局部修复提高评分。

## 2026-09-15 固定实时合屏就绪度（实施前）

基线 `d590cc3b`；独立智能体仅进行源码专项评估，未运行合屏。下表衡量本次 2～4 路 RTP/RTCP 合屏的就绪度，不替换历史项目质量分，也不表示验收通过。

| 维度 | 满分 | 得分 | 缺口 |
|---|---:|---:|---|
| DAG 复用与多源绑定 | 20 | 10 | 单源运行时基数与绑定 |
| 多源时钟与合成来源 | 20 | 8 | 公共输出域、贡献记录 |
| 逐源断流和恢复 | 20 | 6 | 黑屏、静音、局部恢复产品 |
| 线程、背压和资源边界 | 20 | 12 | 多路总体准入与逐分配计量 |
| 双平台合成能力证据 | 10 | 2 | 实际合成及吞吐未验证 |
| 合屏观测与真实验收 | 10 | 4 | 合屏专属观测及运行证据 |
| **合计** | **100** | **42** | **尚未就绪** |

后续能力查询确认两平台滤镜存在；Windows CUDA 异常返回值传播属于静态风险，并无当前启动失败的复现证据，不能认定必须先修依赖。实施前评分保留，不据滤镜存在提高分数。详见 [能力门禁](docs/realtime-video-composition-capability.md) 和 [实施记录](docs/realtime-video-composition-progress.md)。

## 2026-09-10 Beta profile与示例交付增量

代码冻结f41a6554（核心543f8565），真实120秒RKMPP Profile01及清晰版C示例由两名未参与实现者独立复核，Standards/Spec均PASS。首次0b71a44c构建失败暴露实时请求字段/转换遗漏，旧源码PASS已撤回；修正后全量构建及真实参数集验证成功。保留该过程，不能仅靠源码口头确认放行。

| 维度 | 满分 | A | B | 依据 |
|---|---:|---:|---:|---|
| 工业实现依据 | 25 | 24 | 24 | 复用FFmpeg AVOption及既有编码契约，实际RKMPP选项对照。 |
| Planner契约 | 20 | 19 | 19 | 初始/动态请求完整传递，profile纳入编码组相等。 |
| 生命周期与约束 | 25 | 23 | 23 | 同步字符串所有权、独立分支拒绝/退役，清晰示例不改变核心。 |
| 平台边界 | 10 | 8 | 8 | profile依用户要求先RKMPP，Windows新profile未测。 |
| 真实验收 | 20 | 17 | 18 | 同规格120秒、收发曲线/序号、SPS/PPS/PTL及四VLC画面；分差保留。 |
| **合计** | **100** | **91** | **92** | **本新增范围PASS；不覆盖或改写下方历史93/93评分。** |

风险/待优化：同包头文件与库必须配套重编；其他后端profile未由本次验证，物理TX时间、多小时资源趋势和驱动释放边界仍保留。本评分为增量证据审查，不是全仓重新扫描。

## 2026-09-10 动态视频多输出验收补齐

用户明确门禁为发送无超契约突发、动态增删正常、VLC正常解码。冻结98742fd6既往源码双审PASS；Windows同冻结证据按明确边界复核PASS，RKMPP03固定120秒真实链路三项PASS，独立成功提交471dc0c9/76c175b0。两位未参与实现者windows_scope_review_a/b独立复算本轮原始证据，均明确PASS。

| 维度 | 满分 | A | B | 更新依据 |
|---|---:|---:|---:|---|
| 工业实现依据 | 25 | 24 | 24 | 沿用冻结源码审查的工业方案对照。 |
| Planner契约 | 20 | 19 | 19 | 真实readback、时间基、组契约及容量产品不变。 |
| 生命周期与约束 | 25 | 23 | 23 | 沿用源码审查；RK实流68294次申请释放相等、payload归零。 |
| 平台边界 | 10 | 9 | 9 | 新依赖实际进程maps、哈希及共享DAG的RK实流已确认。 |
| 真实验收 | 20 | 18 | 18 | Windows/RKMPP三项完成；不外推长期或所有布局。 |
| **合计** | **100** | **93** | **93** | **本次范围PASS；证据增量评分，不是重审全库。** |

保留风险/待优化：软件出口与物理TX时刻有证据边界，接收端存在到包聚集；驱动内部最终释放仍无时限保证，MPP退出提示保留；controller职责拆分、READONLY潜在COW、长期资源趋势及未来音视频共同激活/同步属于后续演进，不扩成本轮验收。

## 2026-09-10 动态视频多输出增量审查（验收补齐前历史记录）

两名未参与实现的独立审查者 A/B 及新 PR 审查者对冻结 `98742fd6` 源码均 PASS，完整交付均 FAIL。下表是本冻结增量评分，不替换历史全仓评分；分差保留，不由实现者平均或提高。

| 维度 | 满分 | A | B | 新PR审查 | 依据与边界 |
|---|---:|---:|---:|---:|---|
| 工业实现依据 | 25 | 24 | 24 | 23 | GStreamer动态管线、FFmpeg tee/解码边界/私有包关闭、NVENC/HEVC随机访问及版本限定RKMPP/RGA。 |
| Planner契约 | 20 | 19 | 19 | 19 | 真实时间基、prepared readback、原事务期限、完整编码契约复用及统一依赖身份。 |
| 生命周期与约束 | 25 | 23 | 23 | 22 | 停止屏障、异步回收、真实引用计账；RK关闭修复有源码证据，待新实流归零。 |
| 平台边界 | 10 | 8 | 8 | 7 | 共享DAG，新依赖全量构建及七库哈希；新CLI实际映射与完整RK链路仍待验证。 |
| 真实验收 | 20 | 12 | 11 | 10 | Windows22/23/24动态及长GOP通过；最新25晚帧FAIL，不能以历史结果替代。 |
| **合计** | **100** | **86** | **85** | **81** | **源码审查PASS，完整交付FAIL。** |

优先风险：Windows晚帧根因未闭环；新RK链路和依赖关闭实流未通过；不可取消驱动的最终释放无时限保证。待优化：controller职责拆分、次级Release失败可观测性、READONLY引用可能触发COW、多小时资源趋势、未来音视频共同激活与持续漂移验收。

> Baseline scope: `codex/rkmpp-zero-copy`, scored 2026-08-17. Task5 evidence was appended on 2026-08-26; numeric scores remain frozen until the required two independent reviewers complete the current branch review.

| Dimension | Max | Score | Evidence / debt |
|---|---:|---:|---|
| Architecture and responsibility boundaries | 12 | 11 | Planner, builder, runtime validation and protocol output remain separated; the realtime surface is still broad. |
| Typed plans and planner authority | 12 | 12 | Backend, frame, transfer, filter, packetization and RTP ingress decisions are typed, serialized exactly and fail closed. |
| DAG shape and lifecycle | 10 | 8 | Strict shapes, bounded queues, RAII and source-loss propagation are preserved; bare RTP has no authoritative finite-source completion and RKMPP teardown remains noisy. |
| Zero-copy correctness | 14 | 14 | DRM PRIME format, planes, backing, dimensions and lineage are validated; original-size identity and RGA-only replacement are observed. |
| Protocol interoperability | 10 | 9 | H.264/HEVC separate RTP and H.265 SDP are real-media validated; broader receiver coverage remains. |
| A/V synchronization | 10 | 9 | Shared audio planner owns copy/transcode; synchronized output uses sample-domain correction with stable drift and zero drops. |
| Performance and resource bounds | 10 | 9 | RK VideoOnly averages 3.20% machine CPU with bounded 54 MB RSS; batched ingress reduced input calls and CPU, but long thermal soak is absent. |
| Diagnostics and error semantics | 8 | 8 | CPU/RSS, worker names, drift, DRM/RGA/transfer counters and final cause are explicit without suppressing target errors. |
| Cross-platform isolation | 7 | 5 | Shared code clean-builds on Windows; the Windows batched receive adapter and same-spec realtime acceptance remain frozen, not complete. |
| Maintainability and documentation | 7 | 6 | Planner materialization, adapter factory, plan decoder and receiver are separated; the overall realtime diff remains broad. |
| **Total** | **100** | **90** | **A-: RK VideoOnly is usable and bounded; lifecycle, Windows ingress and soak gates prevent a general production-ready claim.** |

## Task5 Datagram 发送控制证据增量（未重评分）

- 三类实时输出已统一为 protocol materializer、公共 service-scope `DatagramShaper` 和公共 nonblocking sender；file output 的 shape validator 明确排除 datagram 节点。
- `PreparedEncoderEmissionEnvelope`、`WireTrafficEnvelope`、graph/network resource ledger、PMTU 与 transport timing 产品均由 planner 形成；caller 不再提供 realtime queue、packet size、pacing rate、path MTU、receiver decode lead、batch/backlog/endpoint/socket/correlation 或 startup preroll。
- Windows VS2026 Release clean-first 已成功；Windows CUDA/NVENC 的 H.264 1280×720@30 raw RTP → HEVC 1920×1080@25 CBR 6 Mbps MP2T/RTP 完成固定源 120 秒复验。公共 queue-time drain 采用 WebRTC 公式并受部署容量硬上限约束，queue clock/commit accounting 的两类 TOCTOU 已通过临时 RED→GREEN 验证且临时测试未入库。
- Windows 最终复验提交 64516 datagrams；RTP loss/order、TS continuity/TEI/AFC、WouldBlock、deadline、pressure、partial/ambiguous submit 与 GCRA violation 均为 0，VLC 未记录 late/corrupt/discontinuity/decode error。TX timestamp 未被 adapter tracked，按 report policy 如实记为 untracked，不能据此声称 wire completion。
- 本增量不提高现有分数：最新公共队列改动的 RKMPP 同规格复验、56 链路矩阵、两名独立 reviewer 同时 PASS 和 Task6 最终验收仍未完成；Windows 平均单核 CPU 23.131% 按用户要求暂缓优化并保留为风险。

## Remaining priorities

1. Define an authoritative bare-RTP finite-source completion contract without converting source-clock loss into success.
2. Implement the planned Windows completion-driven batch adapter and repeat the same 2K realtime chain.
3. Resolve RKMPP teardown diagnostics and run multi-hour RKMPP/RGA thermal and repeated-source soak.
4. Complete AudioVideo batched ingress after the VideoOnly gate without creating a separate media path.

## 2026-09-04 RKMPP 持续运行修复专项评分

范围：`a5597326` 至本轮的 23 个生产文件；历史全仓评分保留，本表不代表全库重评。四项 CBR/VBR、H.264/HEVC 互转均完成约 248 秒 RKMPP 实流验收；冻结 dccf95da 已由两名独立智能体交叉审查并均明确 PASS，均建议专项评分维持 90/100。

| 维度 | 得分/满分 | 依据与未完成项 |
|---|---:|---|
| 工业实现依据 | 22/25 | RKMPP 真实异步接口、既有 LOW_DELAY 契约、RFC 1363 逐包债务；非完整 BQL/CoDel 算法移植。 |
| Planner 契约 | 19/20 | 批次、容量、轮询周期由既有事实规划；没有新对外参数。 |
| 生命周期与约束 | 22/25 | prefix 顺序提交、失败 abandon/poison、提交后唤醒闭合；100 ms 仅约束 wire 准入后的驻留。 |
| 平台边界 | 8/10 | 共享 DAG 复用；按用户要求不再进行 Windows 实测，不声明后续改动已覆盖。 |
| 真实验收证据 | 19/20 | 四项 RKMPP CBR/VBR 互转均以本地 2K30 高规格源持续约 248 秒；源期间核心不停，发送服务曲线超额 1356 B，RTP/TS 错误为 0，接收 RTP 零丢失，目标机 RKMPP 和 VLC 默认 D3D11VA 硬解通过。四项分别形成独立测试提交。 |
| **专项合计** | **90/100** | **本地高规格源完整链路 PASS；长期与平台覆盖风险保留。** |

残余风险：Linux TX timestamp telemetry 当前未跟踪；H.264→HEVC CBR 的 Windows Npcap 抓包晚于源开始约 59 秒，但有效连续窗口仍为 196.285 秒；裸 RTP 源自然结束后仍以 source-clock expiry 退出码 1 收敛；248 秒验收不能替代多小时硬件 soak。另有未物化尾部等待不计入 wire residence、LOW_DELAY 阻塞取包在硬件失去响应时可能阻塞 worker 的设计风险。详见 `docs/rk-a559-external-rtp-validation.md`。

追加损伤范围：run30 在输入 lo 设置 20% 丢包后启动探测失败，已有重排等待 5 秒与探测字节预算 5 MB 的组合在首个缺口后先耗尽预算。上述 90 分只覆盖原持续运行修复及四项零损伤验收，不代表 20% 损伤容忍能力；详见 docs/completed/2026-09-07-rk-a559-input-loss20-validation.md。

run31 正常启动后再注入 20% 输入丢包同样 FAIL：有效编码输出停止后触发原 12 秒进展超时，完整 IDR 缺失，VLC 有明显晚帧。该损伤运行项单独保留，不纳入原零损伤 PASS。

## 2026-09-07 输入丢包恢复审查状态

冻结 `3529c232` 的两名独立审查者均判标准与规格 **FAIL**：损伤关键 AU 准入不充分、FU/AU 追加容量未落实、PCR 维护批次无累计硬边界。run40 已证明同会话恢复、发送节奏及收发零丢包，尚未完成连续 VLC 解码补证；上述历史 90 分不适用于恢复增量。三项局部修复正在构建，待真实复测及两位重新明确 PASS 后按既有五维专项体系重评分。详细依据见 [审查记录](docs/completed/2026-09-07-rk-a559-recovery-review-3529c232.md)。
- 当前恢复代码 2d597ba6 的 run43 真实 RKMPP H.264→HEVC CBR 丢包恢复链路通过并已单独标记；这只补充运行证据，不替代两位独立复审。原评分不外推至本增量；此处为当时状态：复审曾因服务额度中断；服务恢复后的最终双 PASS 与专项评分见下节。

## 2026-09-07 输入丢包恢复增量独立评分

冻结生产代码 `2d597ba6`，两名未参与实现的审查者 A/B 均明确 **Standards PASS / Spec 源码 PASS**。以下为恢复增量专项评分，不替换历史全仓评分；评分时 run43 单项完整通过，其余恢复矩阵尚待补齐。

| 维度 | 满分 | A | B | 扣分依据与边界 |
| --- | ---: | ---: | ---: | --- |
| 工业实现依据 | 25 | 22 | 24 | 已逐项对照 WebRTC、GStreamer、FFmpeg、部署 RKMPP/RGA；没有反馈或重传事实，保守 AU 边界可能多舍弃一帧。 |
| Planner 契约 | 20 | 19 | 20 | 沿用既有时限、容量和能力事实，无新增调用参数；损伤启动仍受原探测预算门禁约束。 |
| 生命周期与约束 | 25 | 23 | 22 | 四项源码阻塞已关闭；裸 RTP 有限源退出语义、硬件失去响应与长期反复恢复风险保留。 |
| 平台边界 | 10 | 8 | 8 | 复用生产 DAG；按用户要求未进行 Windows 转码实测，不声称共享改动已获得全平台验证。 |
| 真实验收证据 | 20 | 16 | 14 | run43 完整通过；其余恢复矩阵、WouldBlock 特定分支运行覆盖及旧内核偶发突发仍需证据。 |
| **合计** | **100** | **88** | **88** | **源码双 PASS；不表示完整恢复矩阵或所有发送环境已通过。** |

审查后新增 run44 HEVC→H.264 CBR 恢复完整 PASS，独立提交 `5197f73a`；数值维持审查时评分，不自行加分。run41/run42 的旧内核突发根因尚未闭合。

后续证据：run45 H.264→HEVC VBR 源视频链路 PASS（EOF 后维护尾部未覆盖）；run46 HEVC→H.264 VBR 同会话恢复、191.14 秒有效 GPU 连续硬解且收发一致，但内核入队后突发 FAIL。分数不自行调整，完整矩阵仍未闭合。另有基线既存 Linux TX timestamp 编译条件与有限观测 ID 契约缺陷，尚未修改；它们属于诊断能力债务，不能据此解释或宣称修复内核聚集。

## 2026-09-08 实际源接收容量增量独立评分

冻结 `6fb07f19...73ad5fbd`，两名未参与实现者 A/B 均明确 Standards PASS / Spec（容量修复源码）PASS，分别评分如下；不替换历史全仓评分。

| 维度 | 满分 | A | B | 依据与边界 |
| --- | ---: | ---: | ---: | --- |
| 工业实现依据 | 25 | 24 | 24 | FFmpeg完整UDP接收、GStreamer超预期MTU处理；复用协议容量，固定大槽降低小包缓存效率。 |
| Planner 契约 | 20 | 20 | 20 | prepared transport权威容量，原预算推导槽数，无新增公开参数或fallback。 |
| 生命周期与约束 | 25 | 23 | 23 | 原RAII和截断拒绝保留；64槽深乱序与反复损伤组合尚待实测。 |
| 平台边界 | 10 | 8 | 8 | 共用planner/storage/DAG，Windows源码核对，按用户要求未做Windows转码实测。 |
| 真实验收证据 | 20 | 13 | 13 | run48输入240.06秒、硬解228.36秒、140575 RTP一致、大包无截断；内核突发FAIL，新槽丢包回归待做。 |
| **合计** | **100** | **88** | **88** | **容量修复双源码PASS，完整无突发门禁未通过。** |

用户明确要求先出库，已按已知限制交付73ad5fbd；发布行为不代表完整验收通过。详见run48与2026-09-08库交付报告。
## 2026-09-08 合并master完整增量交叉审查

冻结范围：master `d832820b` 至 `97d55142`，加BetaSession.cpp四行分类修复（SHA-256 `a0d8941ec334697cd854ef2b90b9c9f4d203d6bab8ed26784542008a52e0305e`）。两位未参与实现者A/B独立检查完整变更清单及功能/架构风险路径，均最终Standards PASS、Spec PASS、允许合并。此前历史审核缺口通过本次扩大审查补齐，不追认历史全量PASS。唯一P2出口IoFailure误归SOURCE_LOSS已修复并经两者复审。

| 维度 | 满分 | A | B | 依据与边界 |
|---|---:|---:|---:|---|
| 工业实现依据 | 25 | 23 | 24 | 接收、恢复、发送、硬件及错误分类有权威对照，不宣称公网拥塞控制。 |
| Planner契约 | 20 | 19 | 19 | 事实/readback、类型化容量及装配贯通；历史诊断容量债务保留。 |
| 生命周期与约束 | 25 | 22 | 22 | RAII、顺序/prefix提交、背压及错误传播；长期恢复和硬件失响应未穷尽。 |
| 平台边界 | 10 | 8 | 8 | 共享DAG与平台adapter分离，本轮无Windows转码实测。 |
| 真实验收证据 | 20 | 16 | 15 | RKMPP四路RC/编码方向损伤恢复闭合，未外推全部A/V、布局或多小时负载。 |
| **合计** | **100** | **88** | **88** | **本次合并范围PASS，不是全平台生产成熟度认证。** |

run53/55/56/57的CBR/VBR双向恢复均通过，核心实际入队发送满足既定门禁；历史驱动聚集仍是部署风险。run57输入捕获自身遗漏已单列。Beta红绿分类诊断与8核构建成功，CLI哈希不变，未重复四路媒体测试。详见2026-09-08-rk-beta-classification-review.md及各验收报告。

本轮canvas图像payload准备增加租约、失败回滚及原deadline/stop检查；不覆盖完整资源域或公共多源入口，合屏仍42/100、FAIL。源码独立审查评价及真实realtime结果见[准备记录](docs/realtime-video-composition-canvas-preparation.md)。

逐producer事实接入共享编译路径，新增实际scope/输出键/覆盖校验；尚无完整多源总账或合屏运行证明，维持42/100、FAIL。详见[阶段记录](docs/realtime-video-composition-producer-facts.md)。

逐源资源事实已接同一topology构建/绑定，核对全图成员覆盖、逐输入envelope/ingress及补偿期音频上界；完整驻留总账尚缺，合屏仍FAIL42。见[逐源资源记录](docs/realtime-video-composition-source-resources.md)。

准入调查确认maximumMetadataBytes缺planner写入者，元素数不能证明容器capacity；实施顺序调整为真实输入准备与共享metadata有限存储先闭环，再切换统一owner总账。见[准入门禁](docs/realtime-video-composition-admission-contract.md)。本轮仅调查，未新增构建/媒体通过项，完整FAIL42。

共享canonical元数据已迁移为精确元素只读owner，源链和aggregate共同factory接入，identity按内容比较并持有寿命；不等于分配前全局准入，完整FAIL42。见[存储记录](docs/realtime-video-composition-metadata-storage.md)。

raw RTP输入准备已独立为capture/sealed两阶段owner事务，原realtime preflight直接复用；startup仍从同一sealed输入权威规划。尚未接N源控制器与全局准入，完整FAIL42。见[输入准备记录](docs/realtime-video-composition-source-preflight.md)。

## 2026-09-29 纯视频源成员合同调查

固定双流假设贯穿clock、startup、release与epoch；成员模型方向可采用，但独立startup时钟调度缺权威推导，完整设计和实现仍FAIL。两名独立审查对本轮调查Standards/Spec均PASS；没有新构建或媒体通过项。六维10/8/6/12/2/4，总42/100。见[调查与实施门禁](docs/realtime-video-composition-source-membership.md)。

## 2026-09-29 成员型激活产品双审

32源码首切片Standards/Spec双独立PASS，成员和origin不可分割传递、精确重锚及generation分类接入。Release重建通过；r51既有A/V实流自然exit1/no-progress，清理后4逻辑对象，纯视频/多源仍缺。六维10/8/6/12/2/4，总42/100，完整FAIL。见[实流证据](docs/realtime-video-composition-activation-members.md)。

## 2026-09-29 startup独立截止调度

源startup已解除音频servo周期依赖：复用worker单次绝对期限等待、原有限屏障和生命周期清理，期限与poll共用真实起点及planner原阈值。独立设计及源码双审PASS，Release全量重建通过；r52 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps仍自然exit1/no-progress，4逻辑对象未平衡。无到期分支实流证据，纯视频装配、多源入口/全局准入、AAC重入和双平台合屏门禁仍缺，完整FAIL42/100。见[实现和实流记录](docs/realtime-video-composition-startup-deadline.md)。

## 2026-09-29 RTP源时钟成员合同

计划成员贯穿RTP clock group、validator、snapshot及binder/projector/adapter，AV音频calibration与成员严格一致；单成员复用同一身份/时效/generation状态机，保留PlannedStreamPair现有CNAME政策。独立设计/源码双审及Release全量重建通过；r53 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps仍自然exit1/no-progress，4逻辑对象未平衡，非成功验收。纯视频完整startup/builder/resource、多源入口与总准入、AAC重入及双平台合屏仍未完成，六维10/8/6/12/2/4、42/100，完整FAIL。见[实现与证据](docs/realtime-video-composition-clock-members.md)。

## 2026-09-29 startup成员合同

startup config携带members与完整可选音频产品，单成员复用keyframe/正preroll窗口、代际与deadline状态机；factory端口/options、node有限barrier/EOF、release origin及最终账本startup容量按实际成员处理，start/release对照注册计划。源码双审与Release全量重建通过；r54 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps仍自然exit1/no-progress、4逻辑对象未平衡。完整V源prepared/planner/builder接线、多源总准入/入口及恢复和双平台验收仍缺；六维10/8/6/12/2/4、42/100，完整FAIL。见[实现和实流记录](docs/realtime-video-composition-startup-members.md)。

## 2026-09-29 scheduler退休数据消费

scheduler在purge全部确认后通过代际仲裁与session锁消费退休媒体，复用单一分类器和既有有界head；ContinuousOutput按显式initial-only合同保持独立输出。源码双审及Release全量重建通过。r55→r56同规格RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps对照：20项退休媒体被释放，两个10/10满队列消失，编码包入/出均39971；CLI仍no-progress自然exit1、4逻辑对象未归零，完整验收FAIL。可信终止/恢复、AAC重入、V源整链、多源总准入/入口及双平台合屏仍缺，六维10/8/6/12/2/4=42/100。见[实现与实流对照](docs/realtime-video-composition-scheduler-retirement.md)。

## 2026-09-29 停止资源生命周期

resolver stop/abort释放自持快照与codec/device，mux abort复用已有会话释放，重采集配置保留策略不变。3源码双独立审查及Release全量重建通过；r57 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps（AAC CBR192kbps/44.1kHz双声道）仍自然exit1/no-progress，4对象/0bytes、71849/71845。resolver现存槽无credit，最终报告实时读取ledger，不能把4对象归因resolver或等同元数据。未修复完整退出/恢复，V源整链、多源总准入/入口及Windows→RKMPP仍缺，六维10/8/6/12/2/4=42/100、完整FAIL。见[资源释放与失败证据](docs/realtime-video-composition-resource-lifecycle.md)。

## 2026-09-29 stop失败收尾闭环

r58逐节点账本证实VideoEncode abort释放4项，旧final在作用域RAII reset前采集，不能称退出后持久泄漏。有效stop错误现在复用abort完成回收后原样返回；3源码双审及Release全量通过。r59 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR8Mbps、1280×720、30fps（AAC CBR192kbps/44.1kHz双声道）最终0objects/0bytes、71875/71875平衡，但CLI仍no-progress自然exit1，完整验收FAIL。暂不再把已验证的报告时序问题作为4对象未解阻塞；继续可信终止/恢复、AAC重入、V源整链、多源总准入/入口及Windows→RKMPP门禁，六维10/8/6/12/2/4=42/100。见[根因与同规格对照](docs/realtime-video-composition-credit-retention.md)。

## 2026-09-29 无效终止窗口依赖清理

删除从音频servo派生但没有运行时消费者的terminalDrainWindow，保留实际确认超时与终止状态机。源码双独立审查与Release全量重建通过；r60原规格realtime结果见[记录](docs/realtime-video-composition-transition-contract.md)。这是纯V接线的依赖清理，完整prepared/planner/builder、多源控制器与总准入、恢复和双平台门禁仍未完成；六维10/8/6/12/2/4=42/100，完整FAIL。

2026-10-08 r124/r125：保持75/FAIL。每源事实规划与诊断双审/Release通过；r124初始clock超时、r125未复现但停读7.671秒/2692包缺口/启动静音3.161043秒。实流完整门禁仍未通过，见runtime记录。

2026-10-08 r126：保持75/FAIL。共享协议session抽取双审/Release通过；同规格实流caller_stop0、账本94764归零，但停读7.645秒/2676包缺口/启动静音3.114921秒，完整门禁未通过。新增逐源等待预算已获授权，接线另验。

2026-10-08 r127：保持75/FAIL。逐源等待预算双审复审/Release通过，节点100/200/200ms实测、caller_stop0及账本归零；停读7.668秒/2691包缺口/启动静音3.115578秒仍在。完整Windows→RKMPP门禁未通过。

2026-10-08 r128：独立评分复核保持75/FAIL。累计快照/容量候选修复后双审及Release通过；音频缺证据等待负例到有限容量结束失败。原规格两源120秒完成、caller_stop0/96931归零，但停读7.694秒、2691线端完整缺包、启动静音3.107302秒仍在；内容A/V UNKNOWN、持续接收/恢复/完整矩阵/可信EOF/C API/Windows→RKMPP未完成。private峰707592192B仅本轮观测，非长期上界。详见runtime r128。

2026-10-08 r129：独立评分复核保持75/FAIL。共享时钟组核心及完整mapper校验复用双审/Release通过；实流caller_stop0、payload132796归零，仍停读7.779秒/2749线端完整缺包/启动静音2.998549秒，内容A/V UNKNOWN。全丢失截图覆盖无效，不计视觉通过；连续准备交接与完整恢复/矩阵/C API/Windows→RKMPP门禁仍缺。19项临时产物记录后清理，详见runtime r129。

### r164 证据补充（总分及FAIL结论不变）

独立双审通过关联诊断源码；真实三源/重入完整源、正常drain254ms、CLI0、sender零backlog及payload全回收已取得证据。抓包尾部未覆盖，r163运行deadline缺陷未解决，不能提升为完整合屏PASS或进入RKMPP。诊断和完整结果见 docs/realtime-composition-output-drain.md。

## r167 有限门禁复审（2026-10-12）

两独立审查均PASS本次重入、排空及线端完整性；整体仍75/FAIL。原规格Windows三源及完整AV重入、正常停止、零资源残留、全量输出序号/DTS/解码已验。VLC20条迟到丢弃、原偶发deadline、内容AV、旧realtime AV回归、矩阵及RKMPP仍未闭环。证据见docs/realtime-composition-output-drain.md。
