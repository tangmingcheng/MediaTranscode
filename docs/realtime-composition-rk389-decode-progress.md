# rk389：解码 admission 背压下持续接收

## 证据与成熟机制

rk388 三路 accepted−output 均为 6，Input−Accepted 均为 1。源码确认 RKMPP 使用 SubmissionOrder；registry 满时 attachPendingLineage 返回 WouldBlock，pendingPacket 分支一直遮蔽无输入 receive polling。该闭环可以阻止硬件已完成帧被接收，仍需原规格实流验证因果。

复用 FFmpeg send/receive 的双向推进机制（仓库 avcodec.h 的 API 状态机说明），以及已部署 rkmppdec.c receive_frame 的 MPP_TIMEOUT_NON_BLOCK 路径。RKMPP 异步返回差异已由 planner 的 decoderReceiveInterval 表达；不增加默认定时器、外部参数或依赖补丁。

## 实施与边界

- 仅在已有轮询契约且 lineage admission WouldBlock 时接收输出，再重试同一包；其他错误原样传播。
- 仍无槽位时按既有 interval 使用 DeadlineOrCancellation，避免积压输入触发忙轮询。
- 复用现有 receivePending、pending output、token 释放和 packet.buf credit 所有权；不另开线程，不扩容，不提前释放 token。
- 记录每节点 admission 背压和接收重试次数，保留停止时摘要；共享节点需核查 Windows，非轮询后端不新增定时行为。
- 修改后双独立审查、Windows Release 构建、RKMPP 8 核增量及原规格 realtime FFmpeg→CLI→VLC；构建或代码审查通过不替代实流通过。

当前状态：实现中；尚无修复后构建或实流通过证据。

构建实证：Windows Release 全量 794 目标退出 0，两个 CLI 与 cache/ninja 产物存在；RKMPP 8 核增量 6/6，RK389_BUILD_EXIT=0。源码同步包 SHA256 为 69f0c405e8d184b82032d45c2f309272a02848890e15cf8c3149f485ffedeb2e；双端已核对。rk389 原规格实流正在运行，尚未判定通过。

独立审查：progress_frame_review_a 与 progress_frame_review_b 均明确 PASS；两者均未参与实现。原规格实流早段与约 90 秒 VLC 原生快照均看到三格视频，非完整验收结论；后段与退出数据待归档。
