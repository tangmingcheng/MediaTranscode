# 纯视频源的共同流成员合同调查

基线：746eed37；只读源码调查，未改生产代码、未运行媒体验收。

## 判定

**成员模型方向可采用；完整设计与当前实现仍 FAIL。** 采用 planner 冻结的源域成员集合 `{Video}` 或 `{Video, Audio}`，共同 RTP 时钟、startup、generation、epoch 和 release 节点解释同一成员合同。不要创建纯视频专用媒体链或借用整体 VideoOnly 输出 scheduler。只把 audioPipeline 改成 optional 不成立。

## 工业模型与适用边界

- [RFC 3550 §5.2、§6.4.1、§6.5.1](https://www.rfc-editor.org/rfc/rfc3550.html)：分别编码的媒体 SHOULD 使用独立 RTP session（复用编码存在例外）；SR 的 NTP/RTP 对建立时间映射，CNAME 关联同一参与者的多个媒体源。协议没有要求纯视频必须存在音频。单成员仍须验证自己的 SSRC、SR 和既有时钟有效期；跨成员偏差/匹配只在实际有多个成员时成立。
- [GStreamer rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html)：请求的 session pads 决定开启的成员，每个 SSRC 进入 jitterbuffer，再利用 SR 同步 session。对应本仓库：planner 依据已准备的输入事实确定成员，而不是运行时超时后擅自把 A/V 降为 V。
- [GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)：live 聚合可按截止时间产出，inactive pad 与 EOS 是不同状态。对应持续合屏输出；不能把它解释为源域时钟失效时继续接受未经校准的视频，也不能以忽略 inactive pads 代替 planner 成员合同。

## 已证实的固定 A/V 假设

|层|当前证据|必须同步修改|
|---|---|---|
|源产品|MediaRealtimeCompositionSourcePlan.runtime；MediaRealtimeAvSourceRuntimePlanner.cpp:28 要求 enabled/frame audio、resolvedOutput，始终计算 audio correction；GenerationTransitionPlanner.cpp:67 同样要求 frame audio|成员来自真实 prepared stream set；音频处理产品只对音频成员存在，参加者列表不含不存在的音频节点|
|时钟规划|MediaRealtimeAvSourceClockPlanner.cpp:29–45 要求双 identity/capacity；startupClockInterval 从 audioServo.minimumUpdateIntervalNs 取值|需单独完成 startup 时钟调度设计；当前没有可直接复用的独立 RTCP observation schedule 产品，禁止为 V 填假 sample rate/servo|
|assembly|MediaRealtimeAvSyncAssemblyPlan.audio 为必选；RTP input policy PlannedStreamPair|成员合同贯穿 sync policy、assembly 与 runtime；不要靠某个空字符串推断成员|
|时钟运行|MediaRtpClockGroupValidator::snapshot 缺 m_video 或 m_audio 就 Acquiring，强制双 SR freshness、双 calibration；RtpClockGroupNode 固定双输入轮询/双 mapper|遍历计划成员；完整成员 locked 才激活；共同 epoch 为实际成员 SR source time 最小值；单流保持同样 generation/reacquisition 状态机；unexpected Audio 输入报错|
|startup|MediaAvStartupCoordinator::create 强制双容量；tryRelease 双 locked/双 coverage；WindowSelector 需要 audio candidate；allEof 返回双 EOF|只统计成员，Video 必须同样 keyframe、positive continuous preroll coverage；sourceStart 取视频候选 PTS；V 的 release.audio 为空，非假音频|
|release/epoch|MediaAvStartupEnvelopeBuffer.cpp:115/121 强制双非空；Transaction.cpp 校验 audioOrigin；EpochTransitionService::activateInitial/Next 与 ActivationCapability 必收 audioOrigin|一个带成员的激活产品：公共 epoch + 有音频时才存在的 origin；创建/重锚/事务/激活/快照均校验“成员有 Audio 当且仅当 origin 存在”|
|图与绑定|InputSegmentBuilder.addSharedNodes 固定双 gate/canonical、addSharedPorts 固定双端口；CompositionGraphBuilder.cpp:83、251–275 固定建音频处理和 discardedAudioPort|共同 builder 按成员实例化同种节点与边；缺音频不建音频边/弃音频端口；selected audio source 仍必须真实有音频；registry/processing participant 与实际图一一匹配|

还必须审查 MediaSourceClockStateFanout、RtpClockGroupBuffer、LockedPacketGate/CanonicalInput 对 locked.audio 的取用；AvStartupCoordinatorNodePreparation 的端口/option 解码；PlaybackEpochActivatedBuffer、ActivatedStartupReleaseSequencer、AvBoundReleaseExtractor 及绑定缓存的 audio origin；RuntimeBootstrap 的 correction 注册和 drift telemetry。不能仅改表中入口而保留这些消费者的强制双流解引用。

## 可实施的统一产品

1. 内部源成员值由准备事务产生且可验证，至少区分 Video 与 VideoAudio（不得改变现有公开枚举值）。selectedAudioSource 需 VideoAudio，其余源可以二者之一。源已声明音频但启动失败仍失败，不能自动变成 Video。
2. 将音频特有事实放进一个完整可选子产品：input binding、clock/identity、startup sample/byte bounds、decoder/resampler/correction、audio origin。避免多个独立 optional 形成部分存在状态；公共 validator 要求成员与该子产品等价。
3. RTP locked 结果带成员与实际 calibration；视频必有，音频可选但与合同严格一致。单流“不需 interstream comparison”是集合基数为一的语义，不是校验 fallback。SR stale/degraded、BYE、SSRC change 的组 generation 规则保持。
4. startup 选择器仍按现有 presentation index 与 coverage 查询；视频候选先满足 keyframe+preroll，再在音频成员存在时执行现有 A/V 交集/trim。不要复制第二套 window selector 或另发 first-frame 旁路。
5. epoch 激活将公共 epoch 与成员型 origin 作为一个不可分割的验证产品，从 release 到 transaction、reanchor、activation、snapshot 原样传递；音频消费能力请求无音频域必须报错。ContinuousOutput 域仍要求真实音频 output origin；不能为源域适配降低输出合同。

## 线程、容量、失败与观测

不新增线程或动态队列。继续节点 worker 和现有 metadata/control edges，优先 generation invalidation、受现有背压约束。stream store 与 pending 只对成员分配；单位/字节容量继续由 prepared replay 与 frame envelope 推导。release 保持同一 RAII buffer owner 和原子事务；没有音频意味着无音频分配，而非容量设 0 后仍注册假参与者。缺合同在构图前失败，运行中非法成员/代际退行继续失败。新增观测字段应能显示 planned members、各成员锁定/EOF、activation generation、真实 retained counts；无音频不报告伪 A/V drift。

## 最小完整垂直接线与实施次序

**首个可交付实现切片**应是“成员型 epoch/release 激活合同完整替换”，不是仅新增 helper：公共激活产品 → startup release/transaction/reanchor → ActivatedStartupReleaseSequencer → ActivationCapability/EpochTransitionService → snapshot/bound extractor/音频消费者。所有现有 A/V 创建者显式构造 VideoAudio 产品，现有 realtime 真实链路可直接执行新代码。此切片可单独审查构建/复验，但不能宣称支持纯视频。

随后将 planner membership 从 prepared source 一次贯穿 clock group、startup coverage、builder/ports、generation registration、resource facts，并实际接上 composition 非音源。纯视频能力完成边界必须是这整条垂直链，不能停在 optional 字段或没有调用者的新 planner。若需要分提交，提交说明明确中间状态仍 FAIL。

完整同规格验证必须包含：现有 A/V realtime 回归；selected A/V + nonselected V 合屏；V 端 SR 失效/恢复、关键帧等待、BYE/可信 EOF；所有源丢失但输出持续；确认没有隐藏音频 socket/解码器、假 origin、永远等待音频的 purge participant。当前单源 r50 的 no-progress 与未完成资源准入仍为独立阻塞，不得将成员合同构建成功记作合屏通过。

## 本轮复核与下一实施门禁

2026-09-29，基线746eed37，纯源码/官方资料调查；未修改生产核心，未新建媒体测试。此前r50的完整退出FAIL保持，不能以本次设计调查替代运行证据。

- 已核实 `MediaAvSyncPlanner.cpp::planSourceNonStartupPolicy` 把 `audioServo.minimumUpdateIntervalNs` 设为10 ms；`MediaRealtimeAvSourceClockPlanner::planAssembly`直接复用该字段，runtime validator要求两者相等，`MediaAvStartupClockNode::onProcess`按它产生周期tick。它不是已探测的RTCP调度事实。上文原建议“复用现有RTCP observation schedule”缺少实现依据，已撤回；不能直接照搬为纯视频cadence。
- 首个成员型epoch/release切片不依赖新cadence算法，可继续实施。产品必须在创建时校验成员和origin，冻结后不可变；现有A/V创建者显式构造完整产品。计划成员须由planner写入`MediaAvSyncPlan`并经validator校验，经runtime binding和`MediaAvSyncRuntimeBootstrap::registerGroupAndIssueActivationCapability`传给transition service；service在initial、next和reanchor拒绝成员改变。仅靠origin有无推断计划不可接受；当前bootstrap的create/createInitialOnly均未传成员，必须同步修改。
- `MediaAvStartupVideoPreparationState`的anchor快照与release commit也必须共同迁移；`MediaAvBoundReleaseExtractorNode`只在实际音频成员存在时绑定音频origin；`MediaAudioDriftControllerNode`请求无音频域须明确失败，不能解引用空origin或生成静音origin。ContinuousOutput仍强制VideoAudio。
- `MediaAvReacquisitionCoordinator.cpp::classifyMediaAvGenerationEvidence`在Inactive与Purging/Acquiring分支均强制snapshot.audioOrigin；首切片必须迁移该判定及generation arbitration/recovery消费者，保留精确retired区间、target/future分类、poison/readiness与发布权限校验。不能只让service接受无音频，随后在时钟或gate分类入口再次拒绝。
- 第一个切片完成后，用既有120秒realtime A/V规格实际走新产品；未通过退出与恢复门禁则记录FAIL。不得因此把纯视频、合屏、多源准入标为完成。
- 纯视频clock/startup切片实施前，必须补齐deadline调度的成熟实现对照及权威输入推导：区分SR新鲜度、startup/keyframe/preroll截止时间和音频校正频率，说明停止输入后怎样触发到期检查及队列背压。不从音频servo借常量，也不新增调用方参数。

### 完成顺序（均未完成）

1. [ ] 成员型epoch/release从工厂到激活、快照和音频消费者完整替换，现有A/V生产路径使用；两名独立审查及Release/realtime复验。
2. [ ] planner从真实prepared输入冻结成员，补齐独立源时钟调度设计，贯穿clock/startup/generation/builder/resource；非音源纯视频接入同一DAG。
3. [ ] 多源准备控制器、逐owner统一总准入和公共合屏入口接通；解决r50 no-progress/逻辑对象残留及AAC重入。
4. [ ] Windows同规格2–4源、丢失/恢复/全丢失持续输出及可信EOF/主动停止门禁，通过后再RKMPP；逐项成功提交，失败不计通过。

本轮不改变既有线程、队列或平台adapter，也不改变已拒绝的外部FFmpeg修复范围。设计审查必须分别判断成员模型、cadence未决项和整体实现，不能用“方向PASS”替代完整设计通过。

审查：两名未参与正式文档修改的独立审查者最终Standards/本轮调查Spec均PASS；RFC措辞、bootstrap成员传递和generation classifier漏项已补齐。该结论仅覆盖调查与计划，不代表设计、实现或验收通过。
