# r245：TS 封装后容量契约接线方案（待实现）

## 当前源码证据

- MediaVideoEncoderReadback 已保存 extraData；MediaVideoEncoderPreparer::inspect 对照生产与探测的 emission、randomAccess，但尚未核对 TS 封装几何。
- MediaVideoEncoderCapabilityProbe::publishPacketLayout 发布 layout/randomAccess，未向 stage plan 发布参数集几何。
- MediaRealtimeCompositionPreparer 先生成 output runtime 和 topology（约250–273），后按拓扑存储契约打开生产编码器（约286）。不能将后打开的生产 context 偷换成此前规划事实，不能通过第二套编码链路解决循环依赖。
- 最终 MediaWireTrafficEnvelopePlanner 只收到 emission 和 protocol plan；运行到 ProjectMpegTsMuxSessionAdapter::tryOpen 才调用 MediaTsFfmpegStreamConfigMaterializer::video。
- TS pending 每次只装一个 AU，maintenance 使用独立 cursor；该事实排除跨 AU 合批，但不证明封装后容量。
- MediaTsVideoAccessUnitFramer 随机访问可追加解析后的参数集，短 length-prefix 可扩成四字节 start code。参数来自真实 codec configuration，不能取样例长度或经验余量。
- TS packetizer 已调用 MediaTsTransportPacketBuilder::payload；该组件另有 payloadPacketCount，后续应复用其 adaptation/discontinuity 几何，不能继续只用 raw bytes / 184 的独立公式。

## 工业依据与边界

[FFmpeg bitstream filters](https://ffmpeg.org/ffmpeg-bitstream-filters.html#h264_005fmp4toannexb) 明确 length-prefixed 与 Annex B 转换及 MPEG-TS 使用场景；dump_extra 描述向关键帧追加 extradata。这里复用本仓库已验证的解析与封装实现来规划增量，不改外部 FFmpeg，不引入独立媒体处理链路。文档仅支持格式转换会改变载荷这一原则，不替代本项目大小上界证明。

## 实施次序

1. 将现有 FFmpeg stream-config materializer 的可复用部分放到符合 planner/runtime 共用职责的位置；保留唯一解析实现，不复制 avcC/hvcC/AnnexB parser。
2. 探测阶段发布内部 typed 封装事实：codec/layout、解析后参数集总字节、长度字段与有效 NAL 最小头部约束及来源。不得新增用户输入容量。
3. planner 用现有 raw AU envelope 和真实配置推导 framed AU、PES/TS packet 和 RTP/RTCP 原子事务上界；同时更新部署初算与最终 transport，避免前后准入使用不同几何。
4. 生产 encoder 打开后从 retained context 读取相同事实，验证未超过已准入契约；运行时 codec metadata 与 generation 必须绑定该契约。缺失事实在图执行前报错，不允许下游扩大容量或猜测参数。
5. 所有存储 geometry 按最终 framed 产品计费；保持现有线程和前缀提交状态机、背压及取消语义，继续区分 wire credit 与仍存活的数组。此契约不单独证明多 producer 的同时存储量。
6. 两位独立审查者按 parser/packet-builder 实现逐项核对，再全量 Release、原规格 realtime/VLC快照；Windows门禁通过后才进入RKMPP。

## 尚未判定

这是待实施方案，未宣称 framing 在 r244 真实流中超限，未把它归因为 r239 serial-only 拒绝。不得因静态契约缺口修改源规格、丢包或绕过期限。r244 的真实运行证据和未完成门禁保持有效。


## 独立调查后收敛的复用点

主线已再次读取确认：MediaRealtimeCompositionResourceBinder.cpp:78–85 在绑定 retained encoder 时已经用 FFmpegCodecParametersMaterializer::fromContext 和同一个 MediaTsFfmpegStreamConfigMaterializer::video 解析真实配置，目前仅检查成功后丢弃。优先复用这一事实入口，不再添加第二套 readback 或解析通路。

最终绑定发布前由 planner 完成/核验 framed、wire、network 契约，并一致更新已有 graph plan-source 的编码事实。只修改 options.outputRuntime 副本而保留旧 transport metadata 不成立。初次规划若仍需候选参数集，应保留实际 probe 结果并在此核对；不能假定 probe 返回值已有完整 extraData。该设计接线尚未实现。
## 构图冻结约束收敛

现场核验：MediaScheduledMpegTsOutputSegmentBuilder:186将protocol/emission编码进plan-source；MediaDatagramOutputExecutionSegmentBuilder:69将transport template另行编码；binder最终又将options.outputRuntime产品放入runtime binding。这不是一个可随意替换的单份对象。

因此实施主路径收敛为“探测期保留真实封装事实→构图前planner完成容量准入→retained encoder绑定期核验不超出”，不采用绑定后扩大容量和重写一部分节点选项的方案。上一节的绑定期完成规划仅作为调查候选，不能据此实施构图后补偿。候选事实不充分应在构图前失败；生产事实超限应在发布前失败。

MediaTsTransportPacketBuilder::payloadPacketCount:94–110已体现首包randomAccess或discontinuity时182字节、其余184字节。现有入口接受真实segments span；planner不能构造指向不存在大缓冲的假span。实现时应抽取该函数已有纯尺寸计算为共同核心，由运行时segments求和与planner权威字节上界共同调用，保持唯一包数算法。
独立路径审查认可上述构图前规划/绑定只校验顺序，补充两项实施约束：几何事实必须通过共享stage/output准备证据传递，覆盖普通realtime TS与合屏，不能只放composition局部变量；capability probe可能借助等价software surface探测layout，辅助context的参数集不能自动视为真实hardware context的权威上界。必须采集实际候选context，或明确证明已有等价readback覆盖封装几何，否则构图前失败。该审查仅认可路径，不是实现PASS。
## 实施进度：共享尺寸核心

已在MediaTsTransportPacketBuilder增加纯logicalSize计数入口，span入口保留原总长度溢出及空输入检查后转调；payload直接使用已校验reader.size，不重复遍历segments。首包182/184及后续184规则仅保留一份，运行时字节构造算法未改变。零长度仍报错，SIZE_MAX采用商余数避免向上取整加法溢出。

A/B两位未参与实现的独立审查者实际读取当前两文件，均限定PASS。Release clean-first/all-target会话1165 exit0，configure/build exit0，716步骤完成。缓存、Ninja图及两个CLI产物均核验；local未执行。realtime6065152字节，2026-10-16 13:55:09，SHA256 89DF56BB18C1BE68376B61E46A309D95A87A57BCF02EB624192CC2339924D9CF。未重复执行实流，本次修改未改变组包算法，最新实流证据仍为r244，不能冒称本产物已通过实流。planner尚未使用新入口，framed-AU与参数集契约尚未接通；不宣称r245完成或实流通过。
## 实施进度：协议层唯一视频配置解析器

新增MediaTsVideoConfigParser，输入缩为既有MediaTsVideoElementaryStreamContract和真实字节span；将原avcC/hvcC/AnnexB辅助解析代码迁入protocol/mpegts，未复制算法。FFmpeg adapter继续校验媒体类型/codec/非空extradata并委托parser，音频处理保持原实现。planner后续无需依赖nodes/mux，也无需构造完整mux plan才能解析视频参数集。

本切片未修改codec metadata发布、候选事实或容量契约，尚不宣称封装上界已接通。两位独立审查者实际读取新源码和adapter diff，均限定PASS。Release clean-first/all-target会话24147 exit0，configure/build exit0，717步骤完成；缓存、Ninja及两CLI产物已核验。realtime6066176字节，2026-10-16 14:01:25，SHA256 7C050985F78395788756EB460A3B7E67154208692207869D8626BD1AF1AD8310。local未执行；本产物未运行实流，最新实流仍为r244。
## 实施进度：候选参数快照传递

复用FFmpegCodecParametersMaterializer::fromContext及FFmpegCodecParametersSnapshot取得实际候选context的独立参数副本，stage持有shared_ptr<const snapshot>，PreparedEmissionResolver随现有事实共享传递。不复制参数集解析，不以辅助software surface context代替实际context。控制块分配失败返回AllocationFailed，成功后才发布preparedEmission。

新增字段是内部事实；旧四字段aggregate尾字段为空，尤其actualOutputEmission不得误认持有候选snapshot。后续最终检查仍从retained context读真实配置。当前未强制非空extradata、未改变TS planner资格门禁、未证明封装上界。两位独立审查者实际读四文件和snapshot所有权实现，均限定PASS。Release clean-first/all-target会话98379 exit0，configure/build exit0，717步骤完成；缓存/Ninja/两CLI已核验。realtime6067712字节，2026-10-16 14:07:07，SHA256 3BDC814ED6A6DE008039AEA9ABDC55CB9852E19586C8FD2FA6CF485A6809354D。local未执行；新产物未运行实流。
## 封装数量维度门禁

独立审查和主线源码核验确认：tsStreamDemand（WireTrafficEnvelopePlanner:352–367）及tsStreamAdmission（DeploymentPlanner:79–94）都对整个burst只加一次PES/ADTS头并整体取整。多AU必须逐AU计头、参数集及TS尾包舍入，不能仅修atomic字段后声称整个wire envelope完成。

maximumBurstPayloadBytes由VBV bytes加一次peak/cadence产生；maximumEncoderRetainedFrames由max_b_frames+1独立产生（EmissionPreflightAdapter:169–195）。后者不是同一窗口AU数量的已证明上界。native RTP已有这种使用，也仅记为同类静态风险，不能作为正确依据，更不能据此宣布r239根因。

实施前需明确单AU、burst窗口AU数/随机访问AU数、单位时间封装开销三个维度，并让部署初算与最终wire planner复用同一产品。允许从实际释放/调度契约证明同窗口数量，或从协议最小合法AU结构推导严格界；不得取样例FPS、经验倍数，或要求用户填写内部AU数量。输入RTP的maximumAccessUnitsPerPush不属于输出编码器事实，禁止借用。

本次没有按未证明的retainedFrames倍数写入新容量公式；下一调查点是输出调度契约的释放窗口与编码AU的最小合法结构。已完成snapshot/parser/packet-count基础仍保留，完整framing接线尚未完成。
## 调度释放窗口调查结论

主线与独立智能体均实际读取当前代码：MediaAvOutputSchedulerNode::processMedia 606–626仅在now早于release时等待，到时即可发布；积压AU可连续释放。MediaOutputSchedule只有dispatch/emit/release时间换算，MediaEncodedAccessUnitSchedulingPlan只有提前量。MediaTsDatagramEmissionSchedule::beginAccessUnit 279–350检查单AU容量及服务期限，不给出原始burst的AU数量。因此“单active AU”不能当作“每个burst只有一个AU”。VideoEncodeNode::receivePackets 603起循环取包，直到EAGAIN/EOF或资源背压，同样不提供该数量证明。

工业依据：[RFC 9320 §4.2](https://www.rfc-editor.org/rfc/rfc9320.html#section-4.2)要求在明确观测点使用arrival curve，并说明经过延迟抖动V后应使用alpha(t+V)。这适合作为共享planner的证明框架，不意味着当前Windows调度器已经提供确定性抖动保证。

下一实现前置条件收敛为：先查明canonical dispatch序列的最小间隔/计数界，再将已有期限执行规则转成同一观测点的释放抖动界；字节曲线和AU数量曲线必须明确窗口与来源。maximumReleaseJitter目前可见于时序产品与校验，但不能仅凭字段存在认定scheduler已执行该界。需要追踪完整消费者，不能直接套入公式。此结论排除了直接借用retainedFrames、单AU状态机或配置jitter的三个不成立捷径；不增加公共参数，不改外部依赖，不宣称已修复r239。
## 编码前网格与编码后包数的边界

已沿当前源码核验：MediaAvContinuousAggregateVideo:16–20以整数frame index转换presentation/duration，100–117生成该tick的帧与lineage；AggregateNode:439–455只在成功提交后增加index。这证明编码前网格，不能独自证明编码后的包数。

MediaCodecLineageRegistry::resolveOutputEntry:251–272仅返回lineage，不消费entry；entry由lease析构删除。VideoEncodeNode:667–686对submission-order分支每包pop一个token，但COPY_OPAQUE分支不具备这个数量约束。不得为了证明一帧一包而修改共用registry为单次消费：它同时承担多输出引用的生命周期。[FFmpeg官方COPY_OPAQUE说明](https://ffmpeg.org/doxygen/trunk/group__lavc__core.html)明确帧与包一般允许M-to-N关系。

只读核验本机外部源码D:/mabs/build/ffmpeg-git/libavcodec/nvenc.c：3308–3312每个有效frame加入一个output surface；3350–3365每次receive取一个ready surface，process_output_surface:2786–2793将完整bitstream复制到一个packet；2752–2755将该frame的opaque_ref转移给packet。该adapter源码提供NVENC的数量证据入口，未修改任何外部文件，也尚未证明本机链接二进制与该源码完全一致。不能把该结论泛化为RKMPP或所有FFmpeg编码器。

音频AudioEncoderPacketLineageMapper::map保证正duration且连续，EncodedAudioCanonicalizer保持样本区间连续；这些检查尚未强制每packet等于encoder frame_size，因此不能仅凭frame_size推导已执行的packet计数界。下一步应把实际backend包化能力、编码前时间网格、输出释放窗口组成同一planner事实链，保留共享DAG；普通realtime与合屏分别提供其时间轴事实，不能新增合屏专属处理链路。本轮没有新增容量公式或执行实流。
## 单AU封装上界实现

MediaTsVideoAccessUnitFramer新增maximumFramedPayloadBytes，仅处理一个有效AU的载荷上界。对于输入上限N、长度字段L、协议NAL头最少H字节，运行时每个有效NAL至少占L+H，因此NAL数不超过floor(N/(L+H))；四字节Annex-B前缀转换的上界为N+floor(N/(L+H))*(4-L)。H264的H=1、HEVC的H=2来自现有validNalHeader检查，并抽取为同一helper；L由已有不可变contract约束为1..4。AnnexB不发生此增长。按调用者指定的注入选择，再计入真实配置中参数集总字节；运行时frame和上界共用parameterSetBytes，保留原错误信息和实际字节构造。

所有加乘在运算前验证size_t可表示性；零输入上界报错。不分配假媒体缓冲，不猜测码流NAL数，不改公共配置。参数集是否注入仍由上游mux policy决定。本接口尚未接到planner，不能视为framing/burst/wire/network完整容量产品。生产两文件已冻结，开始Release全量构建会话44135及A/B独立审查。
两位独立审查者均实际读取源码后限定PASS；Release全量会话44135 exit0，configure/build均0、717步骤。realtime产物6069248字节，2026-10-16 14:25:16，SHA256 27554F2E72C98FB4E3779CCD6CC36C5075C765D99D83C413927F6E885966EB0F；缓存、Ninja及两个CLI均核验，local未执行。实流见r245-validation，不能提前声明通过。

r245最新实流失败并完成清理，见realtime-composition-r245-validation.md。三格VLC原生快照可见，但约25秒音频到达mux超期，不能记通过；当前优先沿同一output lineage定位真实迟到，封装剩余接线仍未完成。
