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
