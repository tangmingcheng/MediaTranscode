# 合屏生产纵向接线

基线6dc12d85。此前内部组件没有被多源生产入口调用；本轮以真正AV+AV合屏链路为首次闭环，随后在同一链补齐V与原2～4源/恢复/Windows→RKMPP门禁，不缩减最终目标。

## 接线和职责

realtime请求增加已批准inputs/grid/audioSource，CLI重复--inputs承载每源既有输入参数，--grid fixed、--audio-source零基索引；不增内部容量或时序参数。共享run控制器仅分流准备阶段，之后仍使用同一compile/register/start/wait/completion/report路径。

N源准备必须并发启动：现begin同步等探测，不能串行for调用冒充同步监听。各源沿同一绝对deadline、stop token和move-only owner封存；任一失败取消并join其余。输入/decoder/firstframe/SAR/滤镜事实真实移交，source planner实际调用。唯一输出encoder候选在实际设备上探测，最终pool与canvas按同一topology和readback准备，不能调用N份整链输出计划。

aggregate planner从源/输出事实推导候选、音频samples/fragment界与metadata。候选/source/pending/scratch使用精确allocator backing，不能按vector size假设capacity。采用[GStreamer聚合](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)的单owner和有界输入语义，固定网格参考[compositor](https://gstreamer.freedesktop.org/documentation/compositor/index.html)的每输入几何；不照搬其默认帧率/尺寸或等待策略。

## 总准入与背压

旧credit ledger所有初始producer共用一份account，持输入等待输出credit可能互锁。沿[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)独立池/协商容量/最后引用归还语义，统一显式accounts和(node,stream,payload)归属，composition按allocation owner分区；single/dynamic显式保留原shared account，原潜在进展风险不因此消失。

预算由真实residence、rate、unit和fixed storage形成，独立核对最终图retention与最低进展工作集。禁止求和N份单源ledger、按required抬预算，或只放开aggregate白名单。decoder/filter实际保留须有权威事实，不猜一帧。引擎payload及已计固定存储不等于整个进程/driver内存硬界。

## 当前状态

截至r70，真实双源已通过预检进入生产DAG，选择CUDA双解码/scale_cuda及hevc_nvenc；当前失败为TS视频extradata缺失，实际输出包仍为0。已将TS header策略前移到共享planner并加入实际参数集预检，源码双审PASS，Release全量重建/实流复验进行中；详见[实际运行记录](realtime-video-composition-wiring-runtime.md)。以下保留必要的失败与修复依据。首轮Release全量重建配置成功，编译因MediaRealtimeVideoParametersPlanner缺少请求类型声明失败（build exit2、脚本exit1）；已补直接头文件引用。第二轮全量构建编译core并完成local产物链接（未运行local），在合屏准备文件第285行触发MSVC14.51.36231内部错误C1001（p2/main.cpp:262、Access violation），构建退出1，未触及300秒超时；按[微软C1001说明](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/fatal-error-c1001?view=msvc-170)简化表达式并保持Release优化后重建。未运行local测试。参数解析诊断：`& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-entry-red --inputs "--input-type rtp" --grid fixed --audio-source 0`，旧Release自然exit2：unsupported argument: --inputs，无媒体和临时产物。

完成接线后统一Release全量300秒重建，仅测试realtime；真实多源与现有单源回归分别记录，不能以解析/单源结果代替合屏通过。每次冻结修复须双独立代码审查；当前授权不含push/PR发布、合并或部署。完整合屏当前FAIL、42/100。

审查A曾指出并已关闭：原池授权问题与完整颜色合同缺失，不能仅比较range而把不同matrix/primaries/transfer直接合入同一YUV画布。已从真实首帧与滤镜结果建立内部typed颜色事实，统一同设备输出探测、生产encoder与canvas元数据；有效range缺少权威事实或颜色不兼容且无已规划转换时拒绝；一致UNSPEC标签保留原语义。

指定源静态事实探测（不是local CLI验收）：`& "D:\mabs\local64\bin-video\ffprobe.exe" -v error -select_streams v:0 -show_entries stream=codec_name,width,height,r_frame_rate,sample_aspect_ratio,color_range,color_space,color_transfer,color_primaries -of json "D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4"`，exit0；返回H.264、1280×720、30/1、SAR1:1，四项颜色字段均未返回。不能据分辨率猜BT.709；需真实解码首帧核对，后续按像素合屏语义分别核对有效range与其余标签，不把标签缺失自动判为失败。原源保持不变，无探测临时文件。
审查B补充边界：参考[FFmpeg xstack](https://ffmpeg.org/ffmpeg-filters.html#xstack)和[vf_stack.c](https://ffmpeg.org/doxygen/trunk/vf__stack_8c_source.html)，同像素域几何缩放/平面复制允许各源一致的UNSPEC primaries/transfer/matrix原样传播，不能宣称颜色归一化或猜BT.709。已知冲突、known/unknown混合拒绝；缺源黑画面仍须由SPS/codec规范确定有效range与黑电平。先前一律拒绝UNSPEC的审查建议过度，已纠正，原验收规格不变。

## 颜色与编译表达式修正

已改为先打开唯一device，再准备各真实decoder/filtered tile，由内部颜色planner核对完整range/primaries/transfer/matrix及相同实际software pixel format，之后在同device执行唯一输出probe。SPS完整VUI解析保留颜色三元组；未描述时按H.264/H.265规范发布UNSPEC，不按尺寸猜测。raw descriptor把完整SPS事实交给decoder，实际首帧仍须核对。输出SPS、生产encoder readback、黑模板、canvas发布帧及运行期每块tile均对照同一颜色合同；输出SAR来自grid planner。

实现参考[FFmpeg AVFrame](https://ffmpeg.org/doxygen/trunk/structAVFrame.html)的独立颜色字段与[xstack](https://ffmpeg.org/ffmpeg-filters.html#xstack)同像素格式组合边界。所有源相同UNSPEC仅表示标签原样保留，不证明颜色已知一致或完成颜色管理；已知/未知混合、已知冲突、未经规划的像素域转换均拒绝。此项尚待固定120秒源真实链路验证；不得换源或降低规格。

C1001调用已拆为具名outputGroupKey、typed outputRequest和outputSynchronization，保留groupKey引用寿命与Release优化。仅完成源码静态核对，未在子agent运行构建/媒体；最终由主流程全量重建与双审决定。纯V及Windows→RKMPP完整恢复门禁仍未通过。

第三轮Release全量构建配置/编译均exit0，C1001未复现；随后按审查A将候选枚举/稳定优先级排序移至MediaVideoOutputPlanner，app仅执行规划顺序。最终冻结源码已获独立A/B明确PASS，第四轮Release全量构建已验证最后三文件（配置/编译exit0，产物14:33:13、5555200字节）。源码审查通过不代表任何真实流通过，完整合屏仍FAIL42。

## 2026-10-04 prepared快照所有权调查

r64的真实阻塞发生在完整捕获payload被再次复制到probe lease、并与原payload共同计入同一个硬预算。采用[GStreamer GstBuffer/GstParentBufferMeta](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbuffer.html)及[FFmpeg AVBuffer](https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html)的引用共享模式：只读视图持有原owner，重复视图不复制payload；描述符按实际捕获count精确预留，原payload预算不撤销。

适用边界：现有prepared队列在preflight仅尾部追加，不修改已捕获vector；stop/abort不清队列；消费仅在beginReplay后。现有sealPreflight拒绝probeActive，beginReplay要求sealed。因此显式owner引用与probe lease生命周期门禁可防止销毁/消费，capture线程继续接收，不增加线程或阻塞UDP，不改变总容量及失败语义。Windows/RKMPP复用同一buffer与lease；该修改影响单输入的潜在调用面，须Windows同规格realtime回归后再RKMPP。实施前独立审查该边界，实施后再双独立复审与真实链路；该共享快照修复已完成双审与构建，并由r65越过原重复payload错误；准备期总容量耗尽风险仍待关闭。
## 2026-10-04 r65启动合同接线调查

MPEG-TS协议planner根据已接纳mux产品计算activationOutputLead = transportDecodeLead + startupEmissionPreroll；合屏却在协议规划前将outputSync.startup.outputLeadNs交给各源，违反builder要求的同一最终启动提前量。单输入MediaRealtimeAvSyncRuntimePlanner已先规划output，再将outputPlan.activationOutputLead交给source，合屏应复用这一顺序，不修改时序算法或放宽校验。

输出规划依赖的maximumInputAudioSamples应来自直接上游aggregate：其buildAudio每次严格产出resolved output codecFrameSamples，builder同样用该大小核对encoder FIFO。当前传入source resampler最大块属于错误的生产者事实，也造成不必要的先source后output依赖。改为使用prepared encoder readback形成的codecFrameSamples，先完成输出规划，再以最终activationOutputLead规划各源。没有新线程、队列或平台分支；沿用既有资源/失败门禁。依据[FFmpeg AVCodecContext frame_size](https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html)的音频帧大小事实与[GStreamer Aggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)聚合输出时序职责，生产者块界限按真正相邻节点确定。Windows同规格真实链路复验，后续RKMPP共用该接线。
## r67 实际编码器证据接线

probe建立的emission authority为opened-encoder-context，生产实例inspect使用retained-output-encoder；二者代表不同实例阶段，结构默认全等包含authority。app在bind时误传probe产品，导致实际实例已通过inspect上界验证后，仍必然在证据全等处拒绝。按[FFmpeg AVCodecContext](https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html)打开实例读回原则，将MediaPreparedVideoEncoder返回的实际emission与已有音频事实组成绑定证据；不改authority文本、不放宽全等、不改变已冻结probe规划。生产实例仍须通过不超过已接纳envelope的检查，binder再次inspect实际实例并核对精确证据，随后执行原pool/ledger门禁。
## r68 输出域形状合同修正

真实双源链路在输出绑定后被公共形状校验拒绝：output-only域被要求读取源端pipeline.filter_active。该字段由完整单源pipeline写入；encoder-only builder不拥有源滤镜决策，不能伪造false。沿[GStreamer Aggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)每输入pad与独立聚合输出的职责划分，校验按既有typed域角色执行：source域根据preparation owner核对滤镜，shared域仍强制读取完整pipeline产品，output域按sourceCount=0拒绝任何源滤镜，并继续要求一个输出encoder及完整注册/连接检查。

本次仅调整公共形状validator的字段适用域，无新算法、线程、存储、默认参数或平台分支；既有背压、所有权与失败语义保持。Windows/RKMPP共用此validator；须同规格Windows realtime验证，并保留单源shared域回归门禁。r68已记录失败并清理本轮4个日志与精确PID，无临时产物残留。

## r69 输出采样率事实所属域

MPEG-TS输出validator仍读取audioServo.outputSampleRate，但ContinuousOutput的同步合同明确禁止source audioServo，故该路径必然拒绝。按GStreamer Aggregator输入处理与输出协商的职责分离，输出采样率必须取实际相邻aggregate的resolved audio产品；shared域仍取既有servo产品。无aggregate产品或非输出域不提供采样率，原AAC索引范围、采样率精确匹配及mux参数全等继续强制执行。不能向output域添加虚假servo或跳过AAC校验。builder已核对aggregate.audio与实际outputRuntime.audioOutput一致。本次无新增线程/容量/平台分支；共享validator须Windows realtime回归，之后再RKMPP。

## r70 编码器头部策略前移

r70已进入生产DAG；FileMux首先materialize视频参数失败，未进入音频materialize。FFmpeg NVENC本地源码nvenc.c2337仅在GLOBAL_HEADER时调用nvenc_setup_extradata，后者调用nvEncGetSequenceParams填充参数集；[官方NVENC源码](https://ffmpeg.org/doxygen/trunk/nvenc_8c_source.html)和[AV_CODEC_FLAG_GLOBAL_HEADER](https://ffmpeg.org/doxygen/trunk/avcodec_8h.html)证明打开前配置要求。这里强制extradata来自本项目Project MPEG-TS元数据合同，不声称所有FFmpeg TS mux都要求该flag。

将原单源A/V及纯V TS后置globalHeader=true统一移入既有MediaRealtimeVideoParametersPlanner，显式接收输出layout，缺失/不支持布局失败。composition候选probe、单源probe与最终视频参数均复用，确保TS探测与生产采用同一header策略；Separate RTP保留按最终selected codec选择头部的原逻辑，避免requested codec为空、继承真实源时改变行为。无新增外部输入或平台分支，Windows/RKMPP共用此计划。

composition binder在启动DAG前复用FFmpegCodecParametersMaterializer与现有TS stream materializer，对实际retained encoder核对完整extradata/参数集/layout，不仅检查flag或非空；运行时校验保留。无新线程、队列或持久缓存；参数快照采用既有RAII，在预检作用域释放。任何失败仍拒绝启动，不修改外部FFmpeg，不在节点补参数。

## r71 音频旧代次消费边界

真实双源120秒均自然exit0，RTCP BYE使源进入1→2 purge；source0的已排队旧音频随后被decode/resample严格observe拒绝。参考[FFmpeg ffplay decoder_decode_frame与audio_decode_frame](https://ffmpeg.org/doxygen/trunk/ffplay_8c_source.html)：serial变更flush codec，队列/消费端丢弃旧serial。当前视频节点已有同样的classifyObservation→DropStale，音频分类器也已定义该策略，但四媒体入口仍直接严格observe/validate。

复用现有音频分类器，在decode/encode、startup trim和resample节点媒体入口消费旧generation，不生成输出。trim分类早于origin写入；resample早于mapper/pendingInput/swr，不能在mapper返回success后继续处理旧帧。分类及状态变更处于同一既有lineage锁内；purge保持owner-thread执行，先清codec/缓存再推进generation。边队列残留逐项消费，每个onProcess只处理原有有限工作量，不扩容、无新线程，不跨源重置输出。zero/future仍失败，observe/validate原有严格合同不变。

这是共享Windows/RKMPP音频节点修复，后续需Windows原规格all-loss/recovery及单源realtime回归；未通过前不进入RKMPP。r71的2个最终逻辑对象、VLC晚帧和长期内存仍属待验证项。graphPayloadPressureFailures经源码核实是新登记WouldBlock waiter的累计背压事件，硬容量超界另行报错；不能把该计数当成不可恢复错误，也不能用0worker错误替代饥饿/时序验证。

## r72 校正命令代次隔离

四媒体入口修正后，r72在同规格120秒源BYE触发purge时，由AudioResample的correction边报错。purge在同一lineage锁内重置executor至nextGeneration并清内部pending，但边队列旧命令仍可能抵达。按上一节ffplay serial隔离原则，consumeCorrection在类型校验后、enqueue前复用同一classifyObservation；旧代命令只消费并记录node/command generation/executor generation/sequence，不推进补偿窗口。zero/future失败；当代命令仍进入原enqueue的完整顺序、窗口、容量检查。原canAccept背压、EOF/flush及purge不变，无全队列丢弃或新阈值。此项适用于共享Windows/RKMPP路径，须原规格Windows实流复验；r72不能宣称all-loss通过。

## r73/r74 当前证据与准备期阻塞

r73实际运行双路与逐源重入约7分钟：四次120秒源均自然exit0，VLC采样覆盖双实画、全黑、左实右黑、恢复双实、左黑右实、再次全黑，CLI错误/停滞为0。音频播放静音/恢复、可信EOF/明确stop和长稳仍未完成；测试后按精确PID清理，不称自然退出或完整PASS。命令和指标见[运行记录](realtime-video-composition-wiring-runtime.md)。

r74四路输入启动相差约5秒，最早源在CUDA decoder探测时以retained4936021B/总5000000B拒绝描述符分配，未进DAG。这不是payload重复拷贝，也不是分辨率滤镜启动失败。当前所有capture observation完成后才进行selected audio、output device和逐源首帧准备，期间保留完整历史；探测租约只复制描述符、释放不跨wait/后续源，但合法描述符与历史payload共用总预算，不能靠换启动次序宣称已修复。

[FFmpeg probesize](https://ffmpeg.org/ffmpeg-formats.html#Format-Options)是流信息分析字节上限，不能单独证明多路完整硬件准备的历史保留需求。[GStreamer states](https://gstreamer.freedesktop.org/documentation/additional/design/states.html)将不依赖流信令的设备准备与开始捕获分阶段；可调查提前准备和逐源处理，但仅并行化不能证明容量闭环。后续必须先明确准备保留、消费和回放的一致合同及权威预算，保持取消/join/owner寿命、DAG复用和硬边界；不提高样例probe-size，不暂停UDP或自行丢弃历史包，不修改外部FFmpeg。r66/r74准备问题仍未修复。

独立源码核对：3/4路AV布局与容器路径存在，但r74不能作为四路运行证据；非选定纯V在input/source planner与builder仍强制音频，C API虽共用controller但公开配置和mapper尚未有composition入口。只允许新增已批准inputs/grid/audioSource，保持旧C ABI，不能把上述缺口标为完成。

## r74 探测借用范围设计

采用[GstBufferList get](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbufferlist.html#gst_buffer_list_get)的列表存活且条目未删除时借用原则，移除probe的N项描述符复制。租约在原owner锁内固定frontier count并取得独占probe pin；逐项读取在owner锁内取得channel/span/arrival值，锁外处理。原deque在preflight仅追加，payload不改，pin禁止seal/replay，owner引用防止析构；move/析构只释放一次pin。读取拒绝越界、失效owner、捕获/预算错误。没有新线程、队列、阈值或平台分支；每项处理仍原有bounded probe，取消/截止检查保持。

没有描述符堆分配后不再为它申请字节，但原payload保留与累计观察预算、capture满载行为保持；这是消除真实额外分配，不是忽略现有分配或扩大5MB限额。r74 FAIL不撤销；等待所有源导致的完整历史容量及UDP连续性仍需四路同规格验证，不能先宣称全部容量根因修复。Windows/RKMPP共用buffer/lease，先Windows再RKMPP。设计已独立审查后实施，冻结再双审。

## 非选中纯视频输入：一致合同设计（r76 并行调查）

范围仍为既定混合输入：选中音源必须AV，其他源可VideoOnly；统一输出继续AV，不增加对外参数。依据[GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)按真实sink pad管理队列、事件与GAP、由聚合线程形成输出的职责分离，源成员与输出格式独立。现有底层单成员RTP clock/startup/activation可以复用；当前上层固定AV，必须贯通而非删除校验。

实施顺序：输入音频配置事实→每源members/request/preflight；assembly.audio显式optional；SourceContribution＋RTP允许VideoOnly并严格拒绝残留音频产品；源planner不运行不存在的audio pipeline/correction；现有input segment/configurator按成员组合gate/canonical/branch，不建立另一媒体链；aggregate只要求选中源音频、按实际音频端口规划discard；shape、资源准入、prepared释放逐一核对同一成员合同。Shared及ContinuousOutput原AV边界保持。

线程继续由现有PerNodeWorker和单aggregate执行器管理，不加线程模型；队列/持帧/准备内存继续由原事实规划器与最终资源compiler按实际节点计算，不添加容量公式。无音频即无音频队列/identity/rate/EOF等待。prepared所有权经现有seal/release绑定；缺必需音频、成员不一致或能力不足在DAG前失败。Windows/RKMPP共享这些节点与合同，仅平台adapter不同。

观测门禁：同规格AV＋V真实CLI原始失败证据驱动实现；Windows V断流/恢复、选中音源恢复、全失活持续AV与停止语义，资源/漂移及完整画面音频检查后再RKMPP。冻结后两位未实施智能体按成员逐项复审；当前仅设计方向独立PASS，不代表接线或验收完成。

### 每源成员准备切片（r77失败后）

新增MediaRealtimeCompositionInputPlanner从完整audioRtp字段集合判断声明成员：任一音频字段存在即调用既有registry校验，完全缺失才是VideoOnly，选中音源必须AV。typed input plan由同一容器提供给并行preflight与planSourceFacts；容器在worker join前存活，取消/失败保留原RAII事务。五文件双独立源码PASS，首次构建命名冲突已修正并由两者重新复审；第二次Release全量构建成功。r78实流越过原探测失败，在planRtpInput双成员PT/clock校验失败，未进入DAG。此切片只移除错误的全局成员传播，不代表端到端VideoOnly已经接通。

进一步代码核验纠正早期调查的复用边界：MediaPreparedRtpStartupPlanner目前只接收audio引用，MediaPreparedInputRetentionPlan.audio非optional；ledger admitInputRetention和startup plan/makePolicy均强制AV。可复用的是每stream planStream算法及底层单成员clock/activation，不能称已有完整VideoOnly startup产品。下一切片必须将真实缺失音频贯通retention、ledger、startup、assembly及其全部consumer，现有AV校验保留，禁止虚构零音频预算或移除校验来通过。

### r79前：RTP成员与启动保留产品准备切片

r78暴露的双成员时钟要求改为显式计划关联：保留PlannedStreamPair=1，追加PlannedSingleStream=2；后者只允许VideoOnly＋PreserveActivatedOutput源，不改变单源共享输出原入口。没有音频时拒绝残留音频配置，不创建audio identity/PT/rate。raw transport同时核对members与关联模式，两个临时输入时钟都显式携带members。

MediaPreparedInputRetentionPlan.audio改为optional，prepared audio采用有生命周期约束的借用指针；raw.audio、depacketizer、envelope、prepared owner存在性必须一致。单流复用原planStream整数溢出检查和有限准入算法。ledger要求producer音频与retention存在性一致，按实际流累加；空集合的0贡献不是伪造音频容量。startup按显式members消费可选额度：AV保留原预算/trim，RTP V没有音频额度和trim事实；generic/demux finalizePrepared保持AV门禁。本切片不增加线程/队列算法/平台adapter，仍由原owner管理封存输入。

下游domain validator、source runtime/assembly、资源collector与builder仍严格AV，尚未改为单成员，因此本切片不能等同完整混合合屏。冻结12文件审查与Release全构建后，用真实AV回归检查共享路径，并重跑混合源记录下一断点。

### r81前：源域同步与可选音频装配合同

沿r80真实失败继续贯通SourceContribution：RTP VideoOnly采用既有单成员时钟与生命周期，音频输出采样率在源同步请求中显式optional，音频assembly同样optional；仅实际AV运行audio pipeline/correction planner，纯V不产生音频identity/rate/servo。既有AV校验提取为独立servo helper并保留条件；无音频必须严格无音频startup/RTP/servo产品。SharedSourceOutput与ContinuousOutput仍只允许AV，未扩无音频输出或非RTP单成员场景。

RTP transport输入校验复用同一逐流比较逻辑，AV仍验证两流全部原字段；V验证实际视频并拒绝残留音频事实。源generation participants复用原算法，RTP无音频不登记audio correction参与者。源edge planner接收显式members，字节上界按实际成员求和，仅安装真实release边；未使用的通用edge recipe不形成资源分配。

现有聚合器、source资源collector与共享builder消费者已加audio optional存在性校验，当前仍严格AV；下一切片继续按成员装配实际图。此为类型合同迁移，不把AV门禁改成默认音频，也不声称全混合合屏已完成。原线程/队列/所有权模型、AV时序策略和平台adapter保持，双独立审查与Release全构建、真实链路复验依次进行。

### r81后：聚合输入成员合同

真实FAIL定位于聚合planner无条件要求每源audio assembly/correction/identity。依据GstAggregator按真实sink pad管理输入（https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html），audioIdentity改为optional；选中音源强制有音频，非选中AV才有discardedAudioPort，纯V两者皆无。保留每源视频候选上界、输出AV格式、选中音频队列及所有权算法，不改线程/平台adapter。validator核对身份与端口存在性，运行时只消费选中且已验证音频身份。后续builder和资源collector的AV门禁仍需完整按成员迁移，不能把本合同切片称为端到端实现。

### r84前：混合源实际DAG装配

依据r82在CompositionGraphBuilder的真实失败及GstAggregator真实sink pad原则，9文件贯通实际members：RTP clock group/binder、generation gate/canonical、startup coordinator/release端口、音频source分支均仅AV存在；V使用既有单成员clock/startup/Drop release。启动节点已有V选项拒绝机制，上层不再写任何音频选项。prepared sealed audio、source target和typed input members存在性全等，索引先校验。资源collector按实际输入/producer推导，V拒音频媒体边；AV仍重算correction并精确比对，保留video/decoder/ingress/startup/working-set所有上界。CommonCore按1/2实际成员核对节点，输出AV execution product不再误计入V源；aggregate身份/端口与源域startup身份匹配。复用原线程/队列/owner/平台adapter，没有新外部参数或默认音频。9文件冻结交双独立审查，Release全构建和混合实流待执行；不预判通过。

r84复审与真实注册揭示两处遗漏：V clock group禁止inter-stream skew选项，factory却拒绝Drop。两者修正后，进一步双审确认Extractor的start/commit/reanchor/control/initial reserve/bound control六处事务仍固定双成员。现均按已规划branch成员数提交stack batch的实际span；V仅video，AV仍双批原子，video preparation单批不变；start核对枚举/实际channel，stage核对activation成员，不放宽通用atomic事务拒null约束。无新线程、持久分配、队列上界或算法；这是同一真实pad成员合同在输出事务中的落实。12文件重新冻结双审与全量Release构建。r85第一次构建成功但不含最后Extractor修改，不能作为最终产物验证，正在第二次全构建。
