# RTP 输入等待预算提案

状态：2026-10-08用户明确允许，并指定逐源可配置；CLI与内部接线已通过修复后双审、Release构建和逐源值实测；合屏整体仍FAIL，无默认值。C字段载体仍受原新入口提案边界约束。

## 当前缺口

`MediaRtpIngressPlan::create` 将有限观察窗口的最大 interarrival 用作 reorder delay。该样本不是未来等待上界，提前启动协议处理后再用最终观察值替换会改变已产生的丢失判定。代码扫描未找到可等价复用的稳定入口预算：`maximumWireResidence` 是出口驻留，read/open timeout 是读取/打开期限，RTCP timeout 是证据有效期，startup wait 是初始窗口获取期限。

[GStreamer jitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)使用明确 latency 限定等待，包间距估计是另一事实。不能以样本、经验常量或上述无关超时代替调用方的部署延迟要求。

## 已授权公共事实

- 每源“最大允许输入等待时间”，单位毫秒，表示部署允许接收侧为缺失/乱序媒体等待的期限，不是编码参数或需调用方计算的队列容量。
- CLI：在每个`--inputs`内新增`--input-latency-ms`；同源AV共享该预算。
- C API：在尚未批准的新合屏独立类型`mt_beta_composition_input`中增加`uint32_t input_latency_ms`；不追加到现有无版本结构，不改变旧ABI。
- 必须为正且时间转换可表示；无默认值。生产值由部署要求提供；用户随后明确要求可配置，未指定本机数值。验证用值必须在命令中显式标明，不作为产品默认值，不用于掩盖已复现的停读缺口。
- planner形成独立typed输入等待产品，持续会话与最终runtime使用同一政策；descriptor/socket/wire事实继续限定包数和内存，不能仅凭时间预算推导无依据的内存容量。

## 边界与后续

本确认只涉及这一部署事实，不自动批准其他C API提案字段，也不授权外部FFmpeg修改。真正接入仍需完成连续receiver、有限启动窗口、协议状态/AU归属交接以及Windows原规格验证后RKMPP验证。本提案不主张增加预算能修复当前停读或时钟配对问题。

### 2026-10-08 等待预算授权与实施选择

用户已明确允许input-latency-ms/input_latency_ms，并要求逐源可配置；不设统一值或默认值。本机后续采用源0=100ms、源1=200ms作为显式验证配置，验证各源独立生效，不将其写入产品常量。先接CLI和内部typed契约；旧无版本C结构布局保持，新合屏C入口其余字段仍待原提案确认，不能因本字段授权扩大到整套API。

已声明等待预算的合屏源缺失/非法必须在开启源前失败；旧有限probe入口是独立明确的planner准备模式，不能作为新契约失败后的回退。两模式复用同一媒体协议实现。声明等待产品在bootstrap与final保持同源同值；观察最大interarrival不再改写该产品，只保留容量与事实检查。
