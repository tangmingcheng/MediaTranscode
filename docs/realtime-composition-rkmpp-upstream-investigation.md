# RKMPP 合屏：官方方案、issue 与修改边界调查

日期：2026-11-13。只读调查；未修改外部依赖、生产源码，未构建或运行媒体测试。现有固定依赖与真实失败见 [rk409](realtime-composition-rk409-validation.md)。

## 结论

找到官方相关 issue 和现成基础机制，但本轮未找到可直接补齐本项目 H264/HEVC 解码身份传播及有界持有合同的官方修复。检索未命中不等于官方绝无相关 issue。此前“没有项目内路线”应理解为尚无已证实路线，不是数学意义的不可能，也不意味着 RKMPP 不能正常解码或合屏。

rk409 的直接失败是本项目 capability adapter 拒绝，发生在媒体输出前；不能把它当作已复现的官方 decoder bug。本项目 SourcePresentation 要求原始显示时间、AU身份、generation、不可变时钟投影一起通过重排，并在有界预算内知道何时释放身份记录，这比普通PTS驱动转码的契约更强。

## 官方与第三方来源

| 来源/记录 | 调查结论 | 本次适用性 |
| --- | --- | --- |
| [FFmpeg COPY_OPAQUE 官方API](https://ffmpeg.org/doxygen/trunk/group__lavc__core.html) | 解码时请求packet opaque/opaque_ref传给对应frame；不能以设置flag代替后端实现证明 | 正确的通用接口方向，未证实RKMPP实现完整支持 |
| [FFmpeg官方RKMPP源码](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/rkmppdec.c) | rkmpp_write_data写入PTS，retrieve_frame读取PTS；未看到专用packet opaque所有者随对应picture传播 | 不能直接替换本机fork就宣称解决 |
| [Rockchip官方MPP指南](https://github.com/rockchip-linux/mpp/blob/develop/doc/Rockchip_Developer_Guide_MPP_EN.md) | 声明PTS/DTS/EOS继承；区分整帧输入与split模式、输入消费和输出；MJPEG路径不同 | 已有时间戳通道，但没有在此文档找到任意opaque引用及逐AU最终退休合同 |
| [Rockchip mpp #872](https://github.com/rockchip-linux/mpp/issues/872) | 页面Open；特定H264解码后转MPP或x264均卡顿 | 相关解码问题，未证明与rk409身份门禁同根因，未见可直接采用修复 |
| [Rockchip mpp #834](https://github.com/rockchip-linux/mpp/issues/834) | 页面Closed；异步H26x编码userdata/SEI顺序问题，贡献者指出metadata指针不深拷贝，调用方须保持buffer到编码完成；作者关联fork提交9ce5a19解决 | 编码方向，不能用作解码COPY_OPAQUE证明 |
| [Rockchip mpp #963](https://github.com/rockchip-linux/mpp/issues/963) | 页面Closed；旧MPP MJPEG任务接口问题 | KEY_INPUT_PACKET候选机制的边界，不适用当前H264/HEVC链路 |
| [FFmpeg旧reordered_opaque移除补丁](https://ffmpeg.org/pipermail/ffmpeg-devel/2024-January/320147.html) | 旧字段注明以COPY_OPAQUE替代 | 回退旧字段不是现成RKMPP方案 |
| [ffmpeg-rockchip #185](https://github.com/nyanmisaka/ffmpeg-rockchip/issues/185) | 维护者复现后关联官方#872；后续HEVC评论被指出不相关 | 不合并不同报告的根因，不据此宣称已有修复 |

nyanmisaka/ffmpeg-rockchip的[README](https://github.com/nyanmisaka/ffmpeg-rockchip)明确声明UNOFFICIAL fork；本项目依赖来自该fork父版本加已有关闭所有权修订，见[依赖来源](../3rds/ffmpeg-rockchip/README.md)。不得把fork补丁称为FFmpeg或Rockchip官方修复。

检索覆盖：FFmpeg API、master RKMPP源码、ffmpeg-devel公开邮件；Rockchip MPP指南、develop解码/metadata源码和上述issue；fork最新版解码与issue。关键词含rkmpp COPY_OPAQUE、opaque_ref、reordered_opaque、PTS、KEY_INPUT_PACKET。GitHub connector查询参数失败，改用公开网页/源码交叉读取；没有完成所有历史issue/PR的穷举，网页也可能有索引延迟。master/develop链接可变化，不等于安装版本。

## 具体缺什么

1. **帧对应的输入身份。** 本项目MediaVideoDecodeLineage不仅是PTS，还保存identity、generation、packetTimeBase及projection。B帧输出顺序不同于提交顺序，不能FIFO猜配。重复PTS、时间基换算也使普通PTS查表不能直接充当唯一身份。
2. **身份所有者的最终释放与持有上界。** 正确方案可以依靠引用计数最后释放，不必强制增加一个公开“退休事件”API；但必须覆盖零输出、丢弃、延迟/多输出、flush/reset/close及内部DPB引用。只知道输入字节被消费不等于对应picture以后不会输出。上界供现有planner预算使用，不能经验扩容。
3. **匹配后端的能力产品。** 当前MediaDecoderOpaqueRetentionAdapter仅审计特定Windows FFmpeg native h264/hevc（软件/CUDA），没有RKMPP产品。这是rk409直接拒绝的位置；即使底层补好传播，仍需补RKMPP持有上界和版本证据，不能照搬native DPB公式。

[当前fork源码](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/master/libavcodec/rkmppdec.c)仍调用通用ff_decode_frame_props、单独读取MppFrame PTS；discard/errinfo路径可释放frame而不交给应用。函数回调参数名opaque是释放上下文，不能当作AVPacket.opaque_ref传播证据。

[MPP普通解码源码](https://raw.githubusercontent.com/rockchip-linux/mpp/develop/mpp/codec/mpp_dec_normal.c)确实存在KEY_INPUT_PACKET，但将输入packet放入输出frame metadata的路径位于MJPEG advanced处理；H264/HEVC走parser/hal。只有metadata键存在不足以证明端到端传递。[mpp_dec.c](https://raw.githubusercontent.com/rockchip-linux/mpp/develop/mpp/codec/mpp_dec.c)的sort_pts可覆写输出时间戳，因此token借用PTS还必须证明配置与所有处理分支的原样往返。以上是候选路线不充分的证据，不是断言所有PTS关联方案均不可能。

## 需要改哪里：候选范围，尚未授权实施外部修改

| 层 | 若采用引用计数身份传播，需要补齐的职责 | 目前判定 |
| --- | --- | --- |
| MPP用户态 | inc/mpp_meta.h及packet/frame所有权；H264/HEVC parser、picture/DPB、输出、discard/reset路径传递并释放输入关联引用；若已有机制能证明覆盖，可避免改MPP | 公开接口当前未证明完整；不能预先断言必须改驱动 |
| FFmpeg RKMPP bridge | libavcodec/rkmppdec.c/.h：提交时保存正确packet属性/引用；接收时取对应picture身份填AVFrame opaque_ref；清理所有失败、丢弃、flush/close引用 | 最直接的桥接缺口；仅ff_decode_frame_props或flag不够 |
| 本仓库capability/planner | MediaDecoderOpaqueRetentionAdapter及RKMPP身份能力证据；MediaPipelinePlanner形成类型化传播/持有产品 | 必须适配，不能删门禁或给经验常量 |
| 本仓库probe/runtime | MediaRtpSourceFirstFrameProbe、VideoDecodeNode、MediaCodecLineageRegistry消费相同产品；已有COPY_OPAQUE路径满足时优先复用 | 不另造RKMPP合屏算法，不改公共参数来转嫁内部容量 |

另一候选是项目内可证明的token映射adapter，但需同时解决原样往返、重复PTS、丢弃输入的最终释放、容量硬界及generation隔离；当前没有完整证明。不能把“可尝试”写成现成方案。是否需要MPP改动，取决于能否从其现有路径补齐证明；本轮没有确定最小补丁或宣称只改某个文件就能完成。

优先顺序：把明确的H264/HEVC身份及释放语义问题交由上游确认（本轮未发布issue或联系维护者）；有可核验补丁再评估版本/依赖差异与本仓库adapter接线；随后原规格RKMPP实流及共享链路回归。保留用户要求的本机修改/提交、RKMPP全核增量编译与暂缓Windows编译。完整合屏目标未完成。
