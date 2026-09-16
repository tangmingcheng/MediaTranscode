# 固定实时视频合屏实施记录

## 当前状态

2026-09-16协议代次交接已实现owner线程清理、异步七组ack、sender授权取消未提交预约及SDP生命周期。r12暴露输入证据早于SSRC失效，顺序修复后r13成功锁generation2并生成新MPEG-TS计划，随后AAC时间轴不连续且退出锁等待；完整恢复FAIL。源码双审通过范围与真实指标详见[协议交接记录](realtime-video-composition-protocol-handoff.md)。漂移控制器已改为短授权，r14同规格Debug复验保留AAC失败且CLI自然退出1、逻辑资源归零；完整恢复仍FAIL。AAC恢复须继续原计划的独立输出域、连续样本和显式静音贡献，未改外部FFmpeg，未提高就绪度。r13旧挂起CLI已按用户明确授权定点结束，日志已删除，r15 Release全量构建通过，同规格重入仍报AAC错误，但CLI自然退出1、逻辑资源归零，并实际取消53个未提交数据报；完整恢复仍FAIL。本轮产物与进程已清理。

2026-09-16继续推进purge屏障、首次/重试发布授权、startup重复失效控制及传输计划注册。r9/r10错误已越过；r11全部5组ack且恢复源锁2，仍因MPEG-TS构造器未切换计划而退出1，完整恢复FAIL。双复审对10文件源码增量PASS，命令/指标/逐项清理见[purge屏障记录](realtime-video-composition-purge-barrier.md)。下一阶段须整体处理协议代次交接和sender owner-thread清理，并将输出时间轴与输入源域解耦；多源合屏仍未完成，不新增致命源缺失期限。

上一阶段（r8）：2026-09-16已修复RTP失效重复增代及old/next投影，两次匹配源BYE均保持next2/old1，无malformed discontinuity；源120秒发送完整，之后CLI无进展超时，完整验收FAIL。当时purge并发屏障及持续输出尚未实现，历史证据见[源失活代次记录](realtime-video-composition-source-generation.md)。

2026-09-15：用户批准两阶段计划及新增合屏输入列表、网格、指定音源的 CLI/C API。基线 `d590cc3b`，工作分支 `feat/realtime-video-composition`。现有未跟踪依赖、`NUL` 和 `out/` 属于实施前状态，不纳入本轮提交。

Release 基线已通过 VS2026 全量 clean-first 构建（configure/build exit code 0）。已实施单域显式注册、逐输入 ingress 与音频 frame credit 基础修改；尚未实现合屏，尚无合屏真实验收通过结论。

平台调查发现 Windows CUDA 合成滤镜的异常返回值传播静态风险，尚无当前环境启动失败证据。此前将其认定为实施阻塞并要求先修依赖的结论撤回。用户明确拒绝扩大依赖修复范围；保留现有 FFmpeg，按原计划验证与实施。详见 [能力与证据边界](realtime-video-composition-capability.md)。

## 静态评估

独立审查的合屏就绪度为 42/100，并非项目整体质量分：DAG 复用/绑定 10/20，时钟/来源 8/20，断流/恢复 6/20，资源/背压 12/20，平台能力 2/10，观测/验收 4/10。

阻塞包括全图单解码器与单同步 binding、单源 lineage、缺少逐格 black/silence 产品及多路总体资源准入。既有 RawRtpInput 的 WaitForEvidence 和关键帧恢复应复用，不能将普通源失活误归为全图内部错误。

## 决策记录

- 在当前目录创建功能分支，复用已有构建目录；不创建额外 worktree。
- 保留旧 `plan.md` 的历史事项，通过新增当前计划入口引用本轮中文计划。
- 外部 FFmpeg CLI 的滤镜存在不能证明生产链接库、设备吞吐、分配和持帧边界可用；实际能力调查是前置门禁。

## 2026-09-15 阶段一实施中

首步落在现有单源生产路径：planner 保留域语义，builder 创建节点时输出类型化角色与 ID，编译前验证角色、归属和连接，runtime 以域状态持有 activation/preparation。唯一协议输出权威保留；不放宽单源形状校验，不把此步标记为已支持多源。依据 GStreamer 图级共享时钟、输入时间映射与 GstAggregator 按输入管理状态的成熟职责边界。

真实 A/V 基线暴露前置问题：RTP H.264/AAC → MPEG-TS/RTP HEVC/AAC，输入120秒连续源、1280×720/30 fps/约8 Mbps，输出HEVC CBR8 Mbps及AAC CBR192 kbps。CUDA能力探测成功后，builder报告 `prepared raw RTP input requires a planner-owned ingress product`，没有进入runtime。根因是preflight只为视频seal/规划/configure ingress，遗漏音频；资源账仅计视频arena。修复复用每输入同一规划流程，从各自真实socket和探测证据生成产品，两份arena经checked add纳入总预算，在DAG构建前验证一致性。无新增对外参数、外部依赖变更或平台专用链路。参考 [GStreamer rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html) 的逐会话接收职责；不声称整套实现等价。

验证尚未完成：原基线CLI退出1；源日志3600帧/120秒，PowerShell调用退出1，未独立取得FFmpeg原生exit code，不能标记源进程exit0。VLC无输出后经RC关闭。原始日志为 `out/acceptance/composition-domain-baseline-{cli,source,vlc}.log`。修改后须同规格复验，并回归VideoOnly；共享变更需随后验证RKMPP。完整命令及最终结果在本轮完成记录中汇总。
阶段一完整失败记录、实际命令和审查边界见 [单源基础实施与验证](realtime-video-composition-stage-one.md)。A/V r3 已进入 runtime，但音频 clock binder acquiring 6/6 失败；不得标记回归通过。两位独立审查者均对当前 WIP 源码范围 PASS，交付验收 FAIL。

## 2026-09-16 启动背压推进

binder 获取队列满时停止取媒体包，继续消费时钟并等待原截止时间，未增加容量。Release 全量重建成功；原规格 A/V r4 越过 binder 后，在启动协调器报容量不足，CLI 退出 1、无编码输出；源退出 0、3600 帧/120 秒。两位独立复审者对本项源码 PASS，完整交付 FAIL，就绪度维持 42/100。本轮命令和遥测归档后已清理临时产物，无本轮文件或进程残留。

该轮定位为Raw RTP缺少独立输入startup retention产品：500ms preroll错配100ms输出驻留4/6 AU，prepared ledger只增加单次解包批次。后续修复及r5～r7证据见[输入保留与发布契约](realtime-video-composition-input-retention.md)。

## 2026-09-16 输入保留与启动发布推进

已由planner从源cadence、10秒既有acquisition window和封存回放AU上界生成有限接纳产品，贯通字节/对象预算；补齐binder移动与scheduled clone共享凭证。r5越过协调器后暴露凭证缺失，r6进入CUDA解码/缩放后暴露31音频AU发布到10项队列的永久等待，均FAIL。

启动发布现使用独立边产品，覆盖转码与音频CopyPacket入口，输出驻留容量保持原规划。两位独立源码复审PASS，全量构建通过；r7持续编码并由VLC显示至120秒源结束，随后因时钟失活投影新代次与gate要求旧代次不符而退出1，完整验收FAIL。FFmpeg退出0、3600帧；内部漂移118条记录为0，热阶段工作集仍缓慢增长。命令/遥测/画面结论已归档，临时文件及进程已清理。没有修改外部FFmpeg，没有新增对外参数。下一落点是源失活与代次转换，不能把BYE当可信EOF绕过；完整控制隔离、多源合成、逐源黑屏/静音与恢复仍待实施，阶段一未标记完成。
