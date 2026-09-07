# RKMPP 四项持续运行验收汇总

基线 `a5597326464140a319787d123ccc2ffef9c4e40b`，分支 `codex/rk-a559-external-rtp`；原目录工作，无独立工作树。生产代码检查点 `3f10fb4d`，目标机 `/home/tang/MediaTranscode`。四项使用同一二进制，SHA256 为 `96c5c71d3183cfe11846dba36ee48f4116983c775e7caa1b3eb5078188989b57`。

用户最终明确：已观察无卡顿，以 VLC 正常持续解码验证持续播放，以收发无丢包验证无卡顿；另外要求源持续时核心不停、连续超过三分钟、发送无突发。源制作、生产 DAG 与输出硬解校验均使用 RKMPP；VLC 默认选择 NVIDIA D3D11VA，没有软件解码回退。

## 四项结果

输入均为已有高规格素材制作的 2560x1440 30 fps、约 11.6 Mbps 连续文件。输出均为 1920x1080 25 fps、GOP 50、VideoOnly、MPEG-TS/RTP；出口容量 50 Mbps，maximum wire residence 100 ms。

| 测试 | 输入→输出 | RC（Mbps） | 源持续秒数 | 发送/接收 RTP 包数 | 最大驻留 ms | 结果 |
|---|---|---|---:|---|---:|---|
| run24 | H.264→HEVC | CBR 6 | 248.018 | 156186 / 123613（覆盖最后 196.285 秒） | 48.916 | PASS |
| run26 | HEVC→H.264 | CBR 6 | 248.287 | 156176 / 156176 | 56.722 | PASS |
| run27 | H.264→HEVC | VBR 5/12/13 | 248.347 | 302889 / 302889 | 56.819 | PASS |
| run29 | HEVC→H.264 | VBR 5/12/13 | 248.352 | 303041 / 303041 | 59.547 | PASS |

四项接收窗口均超过三分钟。各接收窗口对应的发送/接收 RTP 逐包哈希一致，序列缺失、重复、乱序均为 0；发送 TS sync、continuity、TEI、AFC 错误均为 0。四项发送服务曲线超额均为 1356 B，恰好一个最大 IP 数据报，deadline miss 均为 0。源期间核心无提前退出，输出均由目标机 RKMPP 硬解 6204 帧、退出码 0。

VLC 日志均明确记录 D3D11VA，并在运行期间有持续进程/GPU 活动；没有 deadlock、持续严重晚帧或解码错误。保留少量 `picture might be displayed late` debug 记录，不将其隐藏或声称逐帧零延迟。VideoOnly 不适用 A/V 漂移。

实际命令、PID、时间和数据分别见：

- [H.264→HEVC CBR](2026-09-07-rk-a559-rkmpp-local-rtp-validation.md)，独立提交 `1763e6d8`。
- [HEVC→H.264 CBR](2026-09-07-rk-a559-rkmpp-hevc-h264-cbr6m-validation.md)，独立提交 `6c681ade`。
- [H.264→HEVC VBR](2026-09-07-rk-a559-rkmpp-h264-hevc-vbr12m-validation.md)，独立提交 `db1a1787`。
- [HEVC→H.264 VBR](2026-09-07-rk-a559-rkmpp-hevc-h264-vbr12m-validation.md)，独立提交 `dccf95da`。

## 失败与诊断边界

- run22 使用软件解码，不符合要求，作废。
- run23 抓包接口选择错误；run24 首次抓包启动引号错误。修正诊断命令后，run24 有效接收窗口 196.285 秒，逐包哈希与发送后缀一致。
- run25 因临时脚本 `set -u` 与环境脚本未定义变量冲突，在核心启动前失败；只修正临时脚本。
- run28 的 HTTP 统计停在早期值；该缓存不能证明实际解码停止，本轮提前停止并作证据不完整处理，未据此改核心。
- run29 的额外 PresentMon 采集丢失 519366 个 ETW 事件，显示事件结果无效，不作为媒体失败或通过依据。用户随后明确采用持续解码与收发无丢包门禁，停止扩展显示事件测试。
- 原外部流的入口丢包证据与核心修复逐轮原因见 [外部流记录](../rk-a559-external-rtp-validation.md)。未将外部源或诊断工具问题归因于核心。

## 冻结审查与风险

冻结提交 `dccf95da` 相对基线的全部 23 个生产文件，已由未参与实现的 `final_rk_review_a`、`final_rk_review_b` 独立核对并均明确 PASS。两者核对了目标 ffmpeg-rockchip 的异步 EAGAIN/LOW_DELAY 接口、RFC 1363 最大速率漏桶、WebRTC 软排队目标，以及 planner 契约、prefix 所有权、提交顺序和唤醒。专项评分均建议 90/100。

残余风险：裸 RTP 源结束后仍按 source-clock expiry 以 1 退出；100 ms 只约束 wire 准入后的驻留，不包含未物化等待或端到端延迟；目标 TX timestamp 未跟踪；硬件失响应时 LOW_DELAY 阻塞取包可能阻塞 worker；四分钟测试不覆盖多小时热稳定性；按用户要求未运行最终共享修改后的 Windows 转码。

四项 CLI、源、tcpdump、VLC、PresentMon 进程均已退出。目标机本轮临时运行/分析脚本已删除，脚本内容与证据保留在 `out/acceptance`，不入库。后续用户追加的网卡 20% 丢包测试独立记录，不回写本表的零丢包基线。
