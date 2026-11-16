# 编码输出进度契约：r440 缺头等待

状态：方向已通过双独立设计审查；仅授权阶段1能力调查，尚未实现。本机 FFmpeg 不修改，不新增公开参数，不放宽 deadline，不丢弃编码包以绕过排序。

## 问题与目标

[r440 证据](realtime-composition-r440-windows-validation.md)记录音频 AU 3672 等待视频 2559 的缺头区间约144ms，交给 mux 时已超过 dispatch 约24ms；sender deadline_miss 为0。当前 `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.cpp:570–584` 在任一未EOF输入缺头时一律等待，没有“此输入未来不会再出现更早DTS”的证据。

目标是让共享 A/V scheduler 在权威进度允许时安全先发布音频，消除这类不必要的无期限缺头等待；不是承诺编码器/操作系统永远及时。`lastDTS + frameDuration`、定时超时后直接放行、一次样例成功均不能授权未来下界。故障链路仍须原规格复验，生产期限与错误语义不变。

## 行业依据与适用边界

- [FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)允许一次输入产生零个或多个输出；[COPY_OPAQUE](https://ffmpeg.org/doxygen/trunk/avcodec_8h_source.html)明确M-to-N关系。EAGAIN及token已resolve都不通用代表某输入已产出最后一个包。必须取得所选编码实现的完整AU、输出顺序及排空契约。
- [GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)默认等待所有输入，通过有序GAP/EOS及显式运行时间支持聚合推进。本项目可复用“显式生产进度＋同一有序数据路径”的原则，不能把原始帧GAP直接等同编码DTS下界。
- [FFmpeg max_interleave_delta](https://ffmpeg.org/ffmpeg-formats.html#Format-Options)是超过缓冲时间差后强制输出，不证明未来DTS，不能等价移植为本项目fallback。
- [NVENC实现](https://github.com/FFmpeg/FFmpeg/blob/9cbd889670/libavcodec/nvenc.c)对完成surface锁定整段bitstream后生成AVPacket，H.264/HEVC时间戳需沿`frameIntervalP`及timestamp FIFO核验；不能引用非REORDER codec的`dts=pts`分支代替证明。必须把能力绑定到实际部署版本和open/readback契约，不能仅按编码器名称推断。
- [RKMPP固定上游实现](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/rkmppenc.c)可见固定cadence、禁RC丢帧、输出DTS=PTS；但没有partition/EOI检查。完整AU、无迟出旧输入、实际MPP配置及目标依赖差异仍需权威核验。现有SubmissionOrder与单帧packet-layout probe均不是完备证明。主线程本轮只读核验目标`/home/tang/MediaTranscode-composition-09093919/out/dependency-work/ffmpeg-lineage`实际HEAD为`92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696`；检索partition/EOI/split相关项未见匹配，仅定位到`rkmppenc.c:963 encode_get_packet`。现存`ffmpeg-lineage.patch`、`mpp-lineage.patch`须纳入能力差异核验，不能将d90e3a1直接视为实际部署版本；本计划不修改这些隔离依赖。

## 可复用产品与缺口

| 现有代码 | 可复用事实 | 缺口 |
|---|---|---|
| `src/internal/graph/nodes/sync/MediaAvContinuousAggregateVideo.cpp:16–20,140–146` | 输出cursor精确生成当前与下一帧presentation，持续黑屏也在同一输出时间轴 | 只约束原始输出帧 |
| `src/internal/graph/sync/MediaVideoPresentationProgress.h` | 同timeline/generation的后续原始帧PTS下界，明确不是编码DTS保证 | 需要经权威编码契约转换 |
| `src/internal/graph/sync/lineage/MediaVideoEncodeLineage.h` | 已有canonical与presentationProgress值类型 | `VideoEncodeNode.cpp:321,714`仍提交/解析裸`MediaCanonicalLineage`，进度未贯穿 |
| `src/internal/graph/planner/capability/MediaEncoderOpenContractAdapter.cpp:112–135`及`MediaVideoEncoderReadback.cpp` | NVENC Immediate实读零B帧、delay0、zerolatency1、lookahead0；prepared context含实际参数 | 未形成完整AU/解码顺序的类型化能力 |
| `src/internal/graph/sync/lineage/MediaCodecLineageRegistry.{h,cpp}` | 有界token存储、代次校验、purge、finishGeneration | 引用生命周期不等于AU结束；不能读取诊断outputCount当生产契约 |
| `src/internal/graph/sync/MediaCanonicalAccessUnitBuffer.{h,cpp}:122` | 当前AU的canonical dispatch与generation | 没有后续编码AU下界；不能把当前duration作下界 |

`MediaPreparedEncoderEmissionEnvelope.maximumEncoderRetainedFrames`目前来自`max_b_frames+1`，用于容量规划，不能拿来推导重排时差或排空保证。能力探测的单次输出分类只证明包布局，不能提升为未来行为保证。

## 阶段1：实际部署能力核验

本轮只读核验，不修改Windows FFmpeg；r440属于共享调度缺少权威进度的证据，不能归因于依赖缺陷。下表的SOURCE PASS仅证明受限后端语义，不表示当前planner已具备该契约或运行验收通过。

| 对象 | 实际身份与权威证据 | 判定与最小边界 |
|---|---|---|
| Windows NVENC | `D:/mabs/local64/bin-video/ffmpeg.exe`：`N-125148-g20054712a2-g9cbd889670+3`；`D:/mabs/build/ffmpeg-git` HEAD=`20054712a242c54aa19d9ae9fca7a0381a2f1397`，tracked clean。相对上游`9cbd889670`，`libavcodec/{nvenc.c,nvenc.h,nvenc_h264.c,nvenc_hevc.c,encode.c}`无差异。本机与Release目录`avcodec-62.dll` SHA256相同：`F720E06FB04379BF8187094B15FD84CA24B25C8567E2842B5457C5D31CFCE621`。 | **受限SOURCE PASS**：逐帧、单视图H264/HEVC，零B、Immediate，固定prepared配置且无运行期重配置。尚缺将这些事实封闭为planner类型化能力；不授权所有NVENC模式。 |
| 完整AU及身份 | 实际`libavcodec/nvenc.c:2763–2860`锁定完成surface、按`bitstreamSizeInBytes`复制整段bitstream形成一个AVPacket；`:3151–3321`每次成功/NEED_MORE_INPUT提交将一个surface和输入时间戳入FIFO，`:3324–3373`每次取一个ready surface。`:2716–2760`以输入slot经`inputDuration/outputDuration`归还`opaque_ref`；不依赖一般COPY_OPAQUE的M-to-N假设。 | 上述受限模式可在**该完整packet返回并解析同代lineage之后**证明该输入完成；不得仅在send成功或token提交时推进。后续严格有序producer进度才可转换成未来decode下界。单视图必须排除`:1582–1644`的multiview分支；逐帧必须排除interlaced配置。 |
| DTS与迟出边界 | 实际`nvenc.c:2659–2714`：H264/HEVC走timestamp FIFO，零B的`frameIntervalP-1=0`使完成输出直接消费对应顺序的输入PTS；不使用非REORDER codec分支。`:2987`起支持reset/reconfig；`:3375`起flush重置FIFO。`libavcodec/encode.c:543–590`可预存frame/packet，NVENC正常EAGAIN也不表示pending输出已排空。 | **禁止**以send接受、EAGAIN或flush调用建立下界。只在有序完整AU结束后推进；旧buffered packet继续走原FIFO。flush/reset/代次变化撤销下界；真实drain EOF及旧AU排空后才终结。运行期修改prepared编码配置不属于本次最小能力。 |
| RKMPP | 目标`/home/tang/MediaTranscode-composition-09093919/out/dependency-work/ffmpeg-lineage` HEAD=`92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696`，`mpp-lineage` HEAD=`560734e83e7d3d8a0a2a337d07108f04f4030caa`，均tracked clean。实际FFmpeg `libavcodec/rkmppenc.c:963`直接把一个MppPacket复制为AVPacket并使用DTS=PTS，无partition/EOI检查；`:1204`先GET_CFG再配置。实际MPP `mpp/codec/mpp_enc_impl.c:241–281,2645,2684,2838,3577`存在split/low-delay partition路径；`mpp/base/mpp_enc_cfg.c:213–215`有`split:mode/arg/out`。 | **FAIL：完整AU能力证据不足**。wrapper不设置split不能证明实际opened split禁用；SubmissionOrder、DTS=PTS、单次probe均不能补足。最小缺口是实际prepared split/low-delay状态的权威证明，或分片模式的EOI/完整AU契约，然后核验顺序及身份；未证明前不授权该后端水位。 |

现存`out/dependency-work/ffmpeg-lineage.patch`只涉及`rkmppdec.{c,h}`；`mpp-lineage.patch`涉及lineage及decoder路径，未补encoder split/EOI契约。以上是补丁文件范围核验，不将它当作全部MPP历史差异证明。独立只读SSH已退出，未接触rk441进程或产物。

[NVIDIA NVENC API官方说明](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/nvenc-video-encoder-api-prog-guide/index.html)与本机`D:/mabs/local64/include/ffnvcodec/nvEncodeAPI.h:2701–2705,3819`支持锁定输出大小及输出duration关联语义；决定本项目一surface一packet与FIFO的证据仍是上述实际FFmpeg文件。最小下一步只在既有prepared/readback adapter封闭Windows受限能力，并接通既有`MediaVideoEncodeLineage`；RKMPP先补能力证据，不增加公开参数，不修改本机FFmpeg，也不按平台建立另一套scheduler。能力未知应保持未授权，不能在生产中猜测放行；要求该优化的组合应在构建阶段明确拒绝缺失契约。
## 最小分阶段实施落点

1. **先封闭能力证明。** 在现有prepared encoder/readback adapter及planner产品中形成内部契约：完整AU的packet边界、输出顺序、零重排及PTS到canonical decode的映射、输入接受/输出完成关系、flush/EOF语义。对NVENC和RKMPP分别以实际库版本、打开参数及实现证据核验；差异仅留在真实backend adapter。无法证明的组合不得授权水位优化，不能靠运行时猜测或增加用户参数补齐。
2. **接通既有进度载体。** 复用`MediaVideoEncodeLineage`与bounded registry传递producer下界；由codec节点依据planner契约，在对应完整AU已结束且更早输入不会迟出之后形成编码进度。若能力允许一输入多包，必须有权威结束标记后才推进；不能看见第一包就推进。不得以生产运行校验代替能力证明。
3. **沿原FIFO交付。** 在现有canonical AU中携带同代、同timeline的后续decode下界。水位只能在携带AU成功进入有序下游后生效；payload/output背压时保留原对象，不另开旁路消息或让进度越过数据。保留现有单节点owner线程模型，不增线程/无界队列/媒体引用；新增值字段与registry/buffer几何必须计入实际sizeof容量规划。
4. **共享scheduler消费。** 复用`MediaAvOutputSchedulerNode`及现有代次状态；缺视频头时，仅当当前音频dispatch严格小于已授权视频下界才放行。相等维持原排序规则，未知不得推导；有头仍按原逻辑排序。验证后续AU不违背已发布下界，违背保留明确错误，禁止夹紧时间戳、丢包或fallback。水位在flush/purge/abort撤销；真实drain EOF且旧AU已排空后才能终结，不能跨代复用。
5. **冻结、双审、原链路验证。** 先由两个未参与实现者按上述官方证据及跨平台能力独立审查。实施后按会话顺序完成r440原四源120秒H264 VBR MPEG-TS/RTP、选中AAC、全失源与完整重入；同时核共享RKMPP链路。观测缺头区间、授权下界、实际后续DTS、AU身份、dispatch余量、CPU/内存/A/V及正常停止；记录错误后按清单清理。不能以较低规格或仅构建通过关闭门禁。

## 判定与剩余风险

当前判定：直接按上一DTS加帧长放行 **FAIL**；上述显式进度方向 **双独立设计PASS，阶段1仅Windows受限SOURCE PASS、RKMPP证据不足FAIL**，不是实现PASS。r440事后到达的视频2559不能倒推此前已具备该证明。零重排本身仍不自动证明一输入一AU或立即可用；合法时间排序也不能消除编码执行超时、线程调度延迟及发送侧rk427可靠性问题。

## RKMPP有效配置读回：实际源码补查

当前隔离依赖的MPP `mpp/codec/mpp_enc_v2.c:378–388`在encoder锁内执行`copy_enc_cfg(param, enc->cfg_obj)`，GET_CFG读取有效配置。`mpp_enc_impl.c:241–283`中low_delay_part_mode及low_delay_output两条路径均以非零split_mode为必要条件；后者还要求split_out的LOWDELAY位。FFmpeg `rkmppenc.c:1204`只在设置编码参数之前GET_CFG，`:378/631`两处SET_CFG后未读回，`rkmppenc.h`没有公开split/low-delay只读AVOption。因此现有公共接口无法取得实际prepared状态；不得复制私有RKMPPEncContext布局强转访问。

最小候选为目标机wrapper在SET_CFG成功后GET_CFG，读取split:mode/arg/out与base:low_delay并暴露只读事实；frame prep更新同样刷新或验证。此候选尚待独立审查及完整输出路径、身份/顺序/EOF核验，不代表已授权RKMPP水位，也未实际修改依赖。若采用，保留目标隔离前缀和源码/二进制回退，仓库保存改动与证据；本机FFmpeg保持不改。官方命令定义参见https://github.com/rockchip-linux/mpp/blob/develop/inc/rk_mpi_cmd.h，实际行为以上述固定目标源码为准。

依赖身份由既有交付门禁和实测加载库hash核验；本任务不新增运行时DLL加密授权框架。实现配置readback与受审源码语义需分别标明，替换依赖后必须重新核验这些能力，不能把版本字符串本身当作二进制行为证明。

## 阶段1内部能力实现（待双审，未构建）

新增`src/internal/graph/model/MediaVideoEncoderCompletionContract.h`与`planner/capability/MediaEncoderCompletionAdapter.{h,cpp}`，仅接入`MediaVideoEncoderReadback::completion`，未接scheduler或授权任何运行水位。实际实现身份由本轮DLL哈希和既有依赖交付门禁承担；库更换必须重新审查，不新增运行时版本名单或公开参数。合同区分受审源码语义与opened参数readback；任何缺失/不符均返回未授权，不改变原生产错误或编码行为。

NVENC谓词检查native encoder身份、H264/HEVC、COPY_OPAQUE、无INTERLACED_DCT、最终非多视图profile、显式GOP与零有效重排，并复用现有Immediate校验。零重排包含全I的frameIntervalP=0，不硬判等于1。实际`nvenc.c:2025–2027`的CPB side data提供初始化maxBitRate/VBV；正值公开配置须与之相等，从而避免首次send因未收敛配置触发reset。首帧reconfig确在upload/encode/入FIFO前，不会推翻尚不存在的水位；但reconfigure失败仅记录日志，不能假设其必然成功收敛，因此本阶段采用CPB一致这一充分条件，未知保持未授权。

合同有效期限定同prepared context、配置不变且未flush/reset/reopen；这些条件不是一次readback可以永久保证的事实。后续接入前必须由现有owner/代次路径落实生命周期校验及撤销，才能消费完整AU的下界。此次仅增加值类型和静态authority字符串视图，没有新线程、队列、媒体引用或每包字段；readback对象sizeof增长尚未实编测量，不宣称内存全局预算已闭合。RKMPP仍未授权，目标post-SET GET_CFG及完整AU证明由主线程继续核验。
## 阶段1源码复审与后续时间域接线

两个未参与实现的智能体均明确阶段1 SOURCE PASS，仅覆盖能力产品与readback；未批准实际scheduler放行。新模型/adapter与MediaVideoEncoderReadback使用现有CMake递归收集。生命周期及配置快照消费检查仍必须接通，不能把该阶段当作r440修复。

独立核验当前VideoEncodeNode沿用原canonical lineage，不以编码packet DTS重建canonical decode；NoReorder的AU dispatch等于canonical presentation。aggregate的nextCursor下界已处于同一canonical时间轴。因此后续应依据完整AU/零重排/同代身份合同提升下界语义，避免再做ns→编码tick→ns重复量化。scheduler对音频dispatch及视频下界均复用mapCanonicalToMaster后严格比较。av_rescale_q使用NEAR_INF，而checkedFromTicks截断向零，不能将两者当无误差逆映射；packet PTS/DTS用于合同验证，不能凭当前差值修正水位。

## 阶段2–4共享接线（源码冻结待双审，未构建/实测）

- prepared完成合同增加固定配置快照；两个既有realtime preparer沿原`FFmpegCodecContextBuffer`传递optional合同，未授权context显式为空，不新增字符串配置或平台链路。metadata只保留定长值，不长期保留readback的vector/string；runtime只核验既有授权。
- `MediaVideoEncodedProgressState`置于encoder既有purge owner内。复用`MediaVideoEncodeLineage`和有界registry，输入断言带完成序号；完整AU按同代、同timeline及严格输入序号完成后才转为decode进度。首个producer断言之前的普通帧不授权水位；无producer进度的旧链路继续原行为。已激活时检查固定配置、producer下界及无重排canonical映射，违背明确失败。purge/flush/reset撤销本代水位与计数；保留prepared绑定，只有owner确认旧registry/codec输出已清空后显式validateUnchanged才恢复新代能力。stop/abort才放弃绑定，prepare/complete不得自恢复。
- 原canonical AU携带optional decode下界，fanout保留；不二次rescale。共享scheduler仅在携带AU输出提交成功后记录同代timeline下界，背压期间不提前生效。缺video head时复用`mapCanonicalToMaster`，仅允许audio dispatch严格小于下界；相等和未知继续原排序/等待。有video head时验证实际dispatch及新下界不违背已提交断言。终止、flush及代次session替换清除状态。
- 不新增线程/队列/媒体引用；仅固定配置、序号、时间值及现有不可变timeline identity引用。registry tree按真实`EntryValue`几何计费，新增completionOrder自然计入；canonical AU的prepared allocator按真实sizeof分配。现有runtime AU、codec metadata及encoder/scheduler owner使用普通new/delete域，本轮不声称它们已进入全进程硬预算；新增定长字段的实际ABI sizeof待构建核验，不能把payload credit当owner内存计费。
- 诊断沿既有scheduler提交日志增加`committed_video_progress_ns`及`carrying_video_progress_ns`，不是新失败路径。Windows原r440规格、失源重入与RKMPP共享链路仍待真实复验；未知RKMPP完成能力不生成水位。
双审修正：显式前置声明AVCodecContext；区分suspendForReset与revoke。noexcept purge hook只撤销本代状态，owner下一次onProcess在既有锁内完成受控重验证；普通非EOF flush在drain、flushBuffers及finishGeneration完成后重验证，失败直接返回错误。没有新metadata也可在同codec进入新代，旧水位不跨代保留。

## RKMPP复用完成合同（双独立源码审查通过，目标构建/实流待完成）

新增`MediaRkmppEncoderCompletionAdapter`，只承担真实backend读值；所有平台继续使用同一`MediaVideoEncodedProgressState`、registry、canonical AU和scheduler。合同显式携带既有`MediaVideoLineagePropagation`：NVENC为CodecCopyOpaque，RKMPP为SubmissionOrder；node bind必须与planner既有传播模式相符。固定快照为22项公共参数加backend variant，NVENC的原有Immediate/CPB授权谓词不变。

RKMPP读取同一post-SET GET_CFG事务的readonly `split_state_known`、`effective_split_mode/arg/out`、`effective_low_delay`及`effective_rc_drop_mode`、`effective_fps_in/out_flex/num/denom`。最低授权要求known、split-none、禁RC丢帧、输入输出固定且与prepared cadence等价；未知保持明确未授权。split-none使split_arg/out/low_delay不启用已审partition路径，但仍冻结实际值。目标依赖的空/partition错误失败与正常FIFO/EOF证明由既有交付门禁绑定；这些运行参数不能单独证明任意同名库的语义。RC/fps谓词用于避免会失败的可用性组合，不能以丢帧恢复替代错误语义。

实际`rkmppenc.h`使用微秒PTS传输；adapter以checked乘法验证encoder时间tick至少1µs，保证可表示整数PTS的最近舍入往返；不把时间精度条件当canonical时间换算。registry额外保存实际提交的encoderInputPts，正常完整AU必须同时满足完成序号及packet PTS/DTS均等于该输入，再沿原canonical lineage发布下界。这对两个后端使用同一验证路径，异常明确失败，不重写时间戳、不跳过token。

有界观测新增`encoder_completion_prepared`（每次prepared读回一次，含授权/原因及实际RKMPP事实；不在每帧验证中输出）、`encoder_completion_first_au`（每codec绑定首个正常完整AU，含identity/order/packet PTS/DTS/canonical）、`av_scheduler_video_progress_release`（每代首次实际输出提交后的watermark放行）。没有每帧诊断或新增测试框架；此前不存在专用watermark放行计数，本次选择首事件证据而非宣称已有计数。后续通过原规格生产CLI实际open及完整重入验证正例，并复核NVENC；当前不声称已运行。

定长增长涉及完成合同的backend variant、registry内encoderInputPts、两个首事件标志及pending commit标志；registry依旧按EntryValue真实sizeof规划，没有复制媒体或records。最终ABI字节数待构建核查，既有普通new/delete owner边界不扩大为全进程硬预算承诺。
2026-11-16：两位未参与实现的审查者均明确 SOURCE PASS，覆盖 typed lineage、同事务只读事实、正常 FIFO/异常拒绝、实际 PTS/DTS 与提交输入匹配、生命周期及成功提交后首次水位诊断；不等于实际 opened 授权或实流通过。rk445 同步包 SHA256 4c8cab3edeaf01e36dcc39dd958cbeb01d96e90a51dc951d38893a425ca4a29e，35 个源文件逐项匹配目标 e127a744 基线后应用；目标 out/rollback/rk445 保存源码、旧 CLI 和 manifest。旧 SCP PID33784 身份核对后清理，重新传输 exit0。目标现有 Release 目录以 8 核增量构建，PID2006489，结果待核验。本机 FFmpeg 保持不变；用户明确其限制来自已有 Windows 正常验证，不能将目标适配问题归因于本机 FFmpeg。
