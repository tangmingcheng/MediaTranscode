# RTP 输入丢包的行业实现对照

调研日期：2026-09-07。范围：FFmpeg、GStreamer、WebRTC 接收引擎与 AWS Elemental MediaLive 的公开实现/官方文档，对照本项目 run30/run31。WebRTC 是实时接收恢复机制的参照，并非完整转码服务；本报告不做性能排名，不修改生产代码。

结论：成熟实现提供有界接收、损坏媒体丢弃、关键帧恢复以及输入失效期间的会话处理。输入没有可解码画面与引擎不可恢复故障应分别处理。本项目已经保护了输入完整性与资源边界，但 run31 的“无新编码输出 12 秒即终止会话”缺少运行中输入损伤的恢复分支。丢包是触发条件，终止会话属于本项目应用运行控制策略。

## 代表实现

| 实现 | 官方处理机制 | 适用边界 |
|---|---|---|
| FFmpeg | RTP 重排队列释放时记录缺包并解析后续包；RTSP/RTP 等待达到 max_delay 后消费队列。CLI 解码循环记录可恢复解码错误，未启用 exit_on_error 时继续处理。 | 不能据此保证所有错误都可恢复或 CLI 永不退出；硬件错误、EOF、I/O 与显式退出策略另行处理。使用 FFmpeg 编解码库不等于继承 FFmpeg CLI 的会话控制。 |
| GStreamer | rtpjitterbuffer 在 latency 界限内等待，超时标记丢包，可发送丢包与重传事件；H.264/H.265 depayloader 提供 request-keyframe 与 wait-for-keyframe。RTX 组件按 RFC 4588 与发送端缓存配合。 | 关键帧请求/等待不是所有管线的默认启用行为；wait-for-keyframe 要求 AU 输出。H.264 自 1.20、H.265 自 1.26 提供上述两属性；本项目不能直接照搬默认延迟常量。 |
| WebRTC | 配置 NACK/RTX/FEC 后尝试修复丢包。OnDecodableFrameTimeout 区分仍有 RTP 到达与流不活跃；活跃但无可解码帧时可请求关键帧，然后继续 StartNextDecode。 | 此处理分支不因单次可解码帧等待超时销毁接收流；应用仍可自行结束会话。反馈/重传需要双方协议能力、发送缓存和足够时延预算。 |
| AWS Elemental MediaLive | 已正常运行的输入失效后按策略编码最后有效帧、黑帧或占位内容；也可暂停交付或切备用输入，输入恢复后回到正常编码。启动探测失败单独处理。 | MediaLive 的 RTP 输入是 MPEG-TS over RTP，当前测试是 H.264 elementary RTP，只对照会话与连续输出策略。填充画面保持输出，不等于找回真实运动画面。 |

依据分别为 [FFmpeg RTP 队列](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavformat/rtpdec.c)、[FFmpeg RTSP 等待](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavformat/rtsp.c)、[FFmpeg CLI 解码循环](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/fftools/ffmpeg_dec.c)；[GStreamer jitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)、[H.264 depay](https://gstreamer.freedesktop.org/documentation/rtp/rtph264depay.html)、[H.265 depay](https://gstreamer.freedesktop.org/documentation/rtp/rtph265depay.html)、[RTX 接收](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtprtxreceive.html)；[WebRTC VideoReceiveStream2](https://webrtc.googlesource.com/src/+/refs/heads/main/video/video_receive_stream2.cc)；[MediaLive 输入失效](https://docs.aws.amazon.com/medialive/latest/ug/feature-input-loss.html)、[自动备用输入](https://docs.aws.amazon.com/medialive/latest/ug/automatic-input-failover.html)、[输入格式](https://docs.aws.amazon.com/medialive/latest/ug/inputs-supported-formats.html)。开放分支源码与产品文档按调研日期核对，不将其等同于目标机已部署版本。

## 当前链路逐项判定

| 项目 | 当前证据与差异 | 对照判定 |
|---|---|---|
| 丢包识别和坏分片处理 | RawRtpInputNode 记录 sequence_gap，NAL parser 清理不连续分片；run31 中没有将所有损坏 IDR 当完整帧传入。符合 RFC 6184 §5.8 的丢弃建议。 | PASS：完整性保护；不代表画面恢复。 |
| 启动探测的有界等待 | run30 中 bootstrap 将重排等待设为整个 5 秒分析期，而约 3.700 秒下一数据报先触发 5 MB 总探测预算。 | FAIL：当前组合不能通过该损伤启动；预算拒绝正确，等待与预算配合存在限制。 |
| 运行中可恢复损伤与终止故障分离 | MediaRealtimeProgressTracker 在首次编码输出后只认编码包增量；controller 无输入损伤恢复分支，12 秒无增量即返回 ProgressTimeout 并停止 DAG。 | FAIL：run31 无法保持会话等待恢复，与 WebRTC/MediaLive 的上述运行中恢复处理不等价。 |
| 缺包修复与关键帧反馈 | 本轮 SDP 是 RTP/AVP、PT96 H.264，没有 RTX/FEC 或 RTCP feedback 的会话配置；生产裸 RTP 链路未发现 NACK/PLI/FIR 恢复实现。 | 未具备本轮适用证据；不能只在接收端打开一个开关就宣称可用。 |
| 输入失效期间连续播放 | 出口零丢包、wire 约束达标，但 run31 存在 7.225769 秒输出空档与 VLC 明显晚帧；没有验证输入恢复后的重新出图。 | FAIL：现有结果不等价于持续播放，也不等价于 MediaLive 明确配置的填充输出策略。 |

源码定位：`MediaRawRtpBootstrapPlan.cpp`、`MediaRtpReorderBuffer.cpp`、`MediaRawRtpPreparedByteBudget.cpp`、`MediaRtpNalUnitParser.cpp`、`RawRtpInputNode.cpp`、`MediaRealtimeProgressTracker.cpp`、`src/application/realtime/MediaRealtimeVideoRunController.cpp`。详细时间、命令与失败证据见 [run30](completed/2026-09-07-rk-a559-input-loss20-validation.md)、[run31](completed/2026-09-07-rk-a559-runtime-input-loss20-validation.md)。

## RKMPP 与当前任务边界

目标 ffmpeg-rockchip `d90e3a1` 的 rkmpp_get_frame 对 H.264/HEVC 的 discard/errinfo 帧丢弃后返回 EAGAIN；无帧时也返回 EAGAIN，部分真实驱动/API 错误返回 AVERROR_EXTERNAL。它没有保证缺失分片仍可重建正确画面；本轮也没有证明发生驱动 fatal error。[目标后端源码](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/d90e3a1/libavcodec/rkmppdec.c)

本次既有文件以 `-c:v copy` 推送 RTP，不是能够响应关键帧请求的现场编码器会话；也未建立重传/FEC 能力。收端可做有界等待与会话恢复，已丢失的数据仍需发送端重传、冗余保护或之后完整随机访问点才能可靠恢复。不能把硬件解码换成软件解码来验证这一点。

后续最小修复的优先对象应是“运行中输入受损/等待恢复”与“内部执行失活”的判定边界，保持既有 DAG、资源上限和明确的错误证据；不能把单纯延长或移除进展超时当作完整播放恢复。是否采用最后有效帧填充、关键帧反馈、重传或备用源属于不同能力，均需真实会话/平台证据；本报告不新增参数、不实施这些功能。修复验收必须包含丢包停止后能否在同一会话恢复真实连续画面。
