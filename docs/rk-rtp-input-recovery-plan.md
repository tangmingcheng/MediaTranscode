# RTP 输入损伤恢复：最小实施与验证

依据：run31 正常启动后注入 20% 输入丢包，输入仍到达、workerErrors=0，但控制器在编码无进展 12 秒后终止会话。行业证据见 [输入丢包对照](rk-rtp-input-loss-industry-comparison.md)。

## 实施边界

- 沿用现有 `--progress-timeout-ms 12000` 作为运行中输入活跃窗口；不新增对外参数，不复制行业实现中的默认时间常量。
- 参照 WebRTC `OnDecodableFrameTimeout`，独立记录实际 RTP 接收时间与编码输出进展。合法、匹配 payload type 的 RTP 到达时间来自既有平台 ingress adapter；消费旧队列不能刷新时间。
- 参照 GStreamer AU 输出的 `wait-for-keyframe`，连续性丢失后清理未完成组包，丢弃后续依赖画面，完整关键帧到达后恢复。复用既有 H.264/HEVC depacketizer 和同一 RawRtpInputNode，策略由 planner 显式下发。
- VideoOnly 的 RTCP 时钟证据过期转为等待新证据并等待关键帧；不得伪造或延长时钟事实。A/V 同步策略另有多流契约，本轮不扩大恢复范围。
- 不吞掉资源边界、驱动和内部执行错误；启动探测仍按现有门禁失败。没有 RTX/FEC/反馈会话证据，不实现请求关键帧或补包，不承诺重建已经丢失的画面。

## 执行与所有权

接收和组包继续使用既有节点线程；控制器只读取原子到达时间，无新增线程。活动记录由 execution context 持有，随运行时重置；AU 与 payload credit 延用既有 RAII，丢弃时释放预约。不增加媒体队列，保持既有批次与背压。run39 后将已有乱序队列的包数边界改由接收字节预算与平台描述符能力形成，替代有限采样位移加一；仍保留等待时限和 payload credit 约束。Linux/Windows 共用控制与节点代码，adapter 仅提供既有单调时钟；本轮按用户要求只运行 RKMPP，Windows 属于未实测影响范围。

状态日志区分等待可解码输入、完整关键帧到达、输出恢复以及时钟证据等待；实际编码计数保持原义。输入活跃只抑制相应进展超时，不把等待状态记作转码验收通过。

## 进度

- [x] 取得 run31 真实失败证据并核对 WebRTC、GStreamer、FFmpeg/RKMPP 官方源码。
- [x] 写入局部恢复实现；等待目标机构建与真实链路验证。
- [x] run32 因源发送网络不可达未完成；run33 同一会话恢复，但历史补帧造成接收 socket 丢包，完整门禁 FAIL。证据见 [逐次报告](completed/2026-09-07-rk-a559-input-recovery-run32-33.md)。
- [x] 按 GStreamer videorate 最大补帧间隔分支写入局部修复；VideoOnly planner 从源帧周期形成契约，跨缺口不追补历史帧，等待 run34 实测。
- [x] run34 暴露 MPEG-TS 的 AU watermark 阻止 PCR/PSI 空闲维护，保留 0.6 秒缺口后触发正确的 PCR 间隔守卫；已移除维护阻挡，等待 run35。见 [失败报告](completed/2026-09-07-rk-a559-input-recovery-run34.md)。
- [x] run35 的等待期 socket drops=0、PCR 持续、发送服务曲线达标，但恢复首 AU 试图倒退已推进的维护时间而退出；已分开维护推进与媒体时间，等待 run36。
- [x] run36 不再报维护时间倒退，但提交的包已经没有发送窗口；尚待逐帧日志区分积压旧帧与时间戳传递错误，未放宽窗口。诊断构建启用现有 Flow 日志，诊断完成后恢复默认日志设置。
- [x] run37 在输入恢复后重复发送窗口失败，输入 socket drops=0；发现 State 赋值覆盖临时 Flow 配置。已在既有 planner 中规划 RGA 零帧等待：读取目标 FFmpeg 的 `async_depth` 类型和最小值，仅接受支持零帧等待的能力，再复用既有硬件滤镜 graph probe。等待 run38 核对逐帧输出与恢复行为。
- [x] run38 确认恢复帧与编码包的 PTS/DTS 一致，编码恢复；发送前仍因把 160 ms API 调用跨度误判为 PCR 样本间隔而失败。已移除该重复判断，保留按 80 ms 逐样本生成与校验，恢复默认诊断配置，等待 run39 完整验收。
- [x] run39 同一 CLI 持续到 248 秒源结束，workerErrors=0，恢复后输入无缺包达 197 秒；额外乱序被当成丢包且发送服务曲线超额 24.3 KB，完整门禁仍 FAIL。见 [run39 报告](completed/2026-09-07-rk-a559-input-recovery-run39.md)，继续定位容量契约与实际发送突发。
- [x] 参照 GStreamer jitterbuffer 的有界等待与 FFmpeg RTP 的有界乱序队列，planner 改用既有接收描述符预算作为乱序队列容量，保留原时间界限；节点日志记录实际契约。全量构建成功，run40 正在同规格 CBR 输入丢包链路复测；未修改发送实现，增加临时内核网络事件诊断。
- [x] run40 同一 CLI 恢复并持续到源结束，恢复后 197 秒输入无缺包、无额外 discontinuity；两端 RTP 全部一致，发送抓包与内核事件服务曲线均为一包。接收 TS 硬解全部 5,428 帧；连续 VLC GPU 解码观测尚未覆盖恢复后三分钟，完整门禁待补证，见 [run40 报告](completed/2026-09-07-rk-a559-input-recovery-run40.md)。
- [x] 冻结 3529c232 的双独立审查均 FAIL，确认损伤 AU 准入、FU/AU 追加容量及 PCR 维护累计容量三项问题；按现有契约写入局部修复并开始 build8，见 [审查与修复记录](completed/2026-09-07-rk-a559-recovery-review-3529c232.md)。
- [x] run43 完成正常启动、30 秒 20% 输入丢包、同一 CLI 恢复，VLC 连续硬解 190.04 秒、收发零丢包及发送一包节奏；已独立提交、标记并在目标机生成新版库。
- [ ] 完成受影响的 CBR/VBR、H.264↔HEVC 验收，逐项记录命令、结果、根因，成功项独立提交并推送。
- [ ] 冻结实现，两名未参与实现的智能体按行业方案独立复审，更新评分、PR 与独立 PR 审核。
- [x] 按用户最新指令交付 run41 对应 53c5c0d0 丢包恢复库，使用指定打包脚本；附 C 示例并验证 C11 编译、链接。包保留目标机，未混入后加的背压修复；完整成功测试仍独立提交并推送。见 [交付记录](completed/2026-09-07-rk-a559-library-run41-delivery.md)。
- [x] build8 全量构建成功；run41 恢复后 VLC 连续硬解 190.64 秒，捕获发送包全部匹配接收端；内核入队后至驱动提交前积累 11 包、172 微秒集中发送，完整门禁仍 FAIL。丢包与恢复实际命令、时间及根因见 [run41 报告](completed/2026-09-07-rk-a559-input-recovery-run41.md)。
- [x] build9 重新全量构建成功；run42 背压版本同会话恢复、恢复后输入零缺包 197.335 秒、收发 137874 个 RTP 完全一致；仍有内核发送突发，GPU 连续采集无样本，完整门禁 FAIL。未替换已交付库，见 [run42 报告](completed/2026-09-07-rk-a559-input-recovery-run42.md)。
- [x] run43：同一 build9、原参数 H.264 2K30 → HEVC 1080p25 CBR 6 Mbps，恢复后 VLC 连续硬解 190.04 秒、收发 137877 包完全一致、发送抓包及核心内核事件节奏均一包上限，本项 PASS。立即独立提交并产库；旧内核突发根因及双复审仍待闭环。见 [run43 报告](completed/2026-09-07-rk-a559-input-recovery-run43.md)。
