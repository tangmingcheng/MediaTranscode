# 动态视频编码组接口迁移

状态：接口协调中，待基础动态链路诊断后实施。无新增外部参数，不表示功能已完成。

## 依据与缺口

复用 GStreamer [tee 独立分支队列](https://gstreamer.freedesktop.org/documentation/coreelements/tee.html)和[动态阻断、排空后移除](https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html)模式。NVENC [初始化创建编码 session](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/nvenc-video-encoder-api-prog-guide/index.html)，共享匹配必须在 probe/open 之前。

当前 Preparer 先规划并 probe，再打开保留编码器，最后比较合同。在 session 已满时，本可共享的请求也会失败。builder 的 encodingNodeIds/outputNodeIds/encoded 尚未形成完整分段产品。

## 接口协调

精确字段由 resource 与 application 对齐后实施，不添加占位代码。

| 所属层 | 产品与职责 |
| --- | --- |
| planner | 在 probe 前规范化规格，以固定源和运行组已验证合同核对源 epoch、设备、尺寸、帧率、RC/GOP、滤镜、open 要求；另校验目标协议接受现有 packet layout/emission。匹配复用同一个运行编码器的真实 readback，不伪造候选 readback。 |
| builder | 初始与新增统一输出共享源、编码组、协议段分区，各自提供 nodes、threading、资源及跨段 endpoints；协议保留增量归编码 producer，新组保留增量归源 frame producer。 |
| preparer | 暂定返回 variant：MediaRealtimeVideoExistingGroupOutputPreparation 包含 groupId、协议 plan/graph；MediaRealtimeVideoNewGroupOutputPreparation 保留现有 plan/branch/encoder/encodingContract。Existing 不打开编码器。 |
| application | Registry 持 group ID、segment ID、合同、编码分支、fanout、元数据及输出引用；Output 仅持协议段和 group ID。 |
| runtime | 从实际生产段 context 导出跨段绑定。启动前复用 FFmpegCodecParametersMaterializer::fromContext 与 FFmpegCodecParametersBuffer/timeDescriptor 捕获元数据。既有 MPEGTS/RTP adapter 可消费，不读取活跃 AVCodecContext，不新增重复 snapshot 类。 |

## 生命周期

1. 初始图同样分成三段，resolver 完成真实 encoder readback 后登记首组。初始编码器不得永久归共享源。
2. 准备任务持源事实、hwframes 引用及组快照版本；发布前重新校验 epoch、硬件身份、graph 版本及组可加入状态。
3. 先取得协议和保留增长租约，启动协议段并回放元数据，再订阅 encoded fanout；提交后增加组引用。
4. 候选异常仅拒绝候选；已启动段失败后留容器至实际回收，不停止健康组。超时任务继续持有资源并拒绝新事务，直到实际返回。
5. 删除先撤销协议订阅并排空；协议实际退役后扣组引用。最后引用退出后再撤销组的 frame 订阅并排空编码组，实际退役后释放编码器及源保留租约。
6. 零输出保留共享输入。output ID、group ID 各自单调；runtime segment 共用单调分配器。初始两段不同 ID，溢出失败且不复用。

## RAP 与就绪

EncodedVideoOutputFanoutNode 当前强制 canonical lineage，但 VideoOnly 无 AV lineage registry 时不产生该元数据。应复用 packet 的 pts/dts/duration 与 MediaTimeDescriptor，核对 encoder readback timebase；canonical 仅已有时保留。组绑定 MediaVideoSourceGenerationPlan，不伪造 AV canonical lineage。

AV_PKT_FLAG_KEY 不足以单独证明可独立加入。planner 须提供 codec、NAL layout、IDR/CRA 边界和 leading-picture 策略，节点复用 MediaRtpNalUnitParser 与 AnnexB 校验完整 AU。参数集复用 MediaTsVideoAccessUnitFramer 的 BeforeRandomAccess，不把任意 I 帧当作 IDR。

新输出等待自然 RAP，不改变健康组 GOP。READY 要求该输出完整首 RAP 最后媒体 datagram 实际提交证据，等待受会话事务预算和 planner startup 期限共同约束。

## 分工与验证状态

application agent 负责 registry、控制事务、初始接入、引用回收、ID 和状态；resource agent 负责打开前匹配及 planner/builder 产品；runtime agent 负责 AU/RAP、元数据和跨段生命周期。

仅完成接口协调，未新增测试或完成编码组真实链路验证。独立编码分支通过不能替代编码共享通过。
