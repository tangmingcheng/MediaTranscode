# 分离 RTP A/V Prepared Transport 设计

## 问题

视频 fmtp 自动探测在 preflight 期间绑定视频 RTP/RTCP transport，并在硬件能力扫描期间继续保存视频数据；手动 AAC transport 直到 runtime 才绑定。两路输入因此具有不同的接收起点，造成数秒 A/V 偏移、队列与内存增长，并最终使旧 RTCP 证据过期。

## 决策

当分离 RTP A/V 的视频需要带内 fmtp 探测时，preflight 必须同时绑定视频和音频 transport：

- 仅视频执行 H.264/HEVC 参数集探测；音频 fmtp、codec、PT、clock rate 和声道仍完全来自请求。
- 两路 transport 从同一 preflight 阶段开始接收，并分别保留原始到达顺序、到达时间和 RTP/RTCP 数据。
- planner 输出逐流 prepared-input 要求；executable builder 将每个 RAII owner 精确绑定到对应 `RawRtpInputNode`。
- runtime 先让各流消费自己的预读队列，再从原 transport 继续接收；不重新绑定端口，不复制 transport，不丢弃预读数据。
- 手动视频 fmtp 模式保持现有 node-owned transport 路径，不执行任何 preflight 网络 I/O。

## 容量与错误

`probe-size` 是每个 prepared 流允许保留的显式字节容量。视频探测证据不完整、任一路保留容量超限、transport 接收失败、绑定缺失或绑定身份冲突都必须在 preflight 或编译阶段明确失败，DAG 不得以缺失数据启动。

不得通过扩大队列、延长时钟阈值、丢弃早期视频、等待音频追赶、吞掉 discontinuity 或 runtime fallback 来维持运行。真实 discontinuity 和真实时钟失效仍按既有错误策略处理。

## 模块边界

- raw RTP preparer 负责创建同步开始的逐流 transport，并从视频流生成 typed signaling facts。
- prepared-input buffer 只负责单个 transport 的 RAII 所有权、限界捕获和无缝移交。
- planner 决定哪些输入节点必须使用 prepared transport，并校验完整产品。
- executable builder 只执行 planner 的逐流绑定决策；runtime node 不选择模式。

## 验证

使用固定连续 120 秒源、硬件链路和 1280x720 输出，按 VLC、CLI、FFmpeg 顺序运行。验证自动 H.264、自动 HEVC 与手动 H.264 输入矩阵，持续监控 CLI CPU、工作集、队列、A/V 漂移和退出原因。自动模式必须保持 A/V 起点一致，源流有效期间不得误退出；真实错误仍必须退出。
