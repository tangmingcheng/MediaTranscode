# 合屏当前验收覆盖与收尾顺序

核验日期：2026-11-16。由未参与本轮实现的独立智能体核对原计划与逐轮记录，主线程复查原计划“目标与约束”。此表不改变目标，不把部分证据记为整体完成。

| 覆盖项 | Windows | RKMPP | 尚需处理 |
|---|---|---|---|
| 2/3/4源、异构、黑屏、重入 | r436四源及选中源重入，早期源数记录保留 | rk422–425已覆盖 | 不重做全部源数排列 |
| HEVC CBR 8Mbps、TS/UDP | r436最新功能修复，早于e127a744诊断 | rk438同诊断源码通过 | Windows共享诊断回归 |
| HEVC CBR 8Mbps、TS/RTP | r243等历史三源有限证据 | rk439四源120秒、四格/全黑、正常停止，媒体/控制已提交 | rk439不重复宣称重入已测 |
| H264 VBR目标8Mbps、范围8–12Mbps、TS/RTP | r131/r133双路有限证据，非最新共享版本 | 尚无合屏实流记录 | 优先补RKMPP及对应最新Windows回归 |
| 选中音频失源/恢复 | 既有记录；r437内容关联仍有重复画面歧义 | rk426近静音与恢复 | 内容级A/V及持续运行收敛 |
| C公共入口 | 已实现；跨平台共享适配仍需最终审查 | rk430–434配置寿命、负例、ABI、旧头实流和启动回调边界 | 按公开契约补运行回调/压力等有限残项 |

原计划明确MPEG-TS/RTP、H264/HEVC与CBR/VBR覆盖，未明确编码×RC×UDP/RTP全笛卡尔积。因此H264 CBR、HEVC VBR以及所有UDP扩展组合暂不增加为强制收尾门禁。既有原规格源始终120秒、至少1280×720/30fps/约8Mbps，不以缩小失败复现条件替代rk427四源可靠性复验。

优先顺序：完成e127a744 Windows回归；补RKMPP H264 VBR/RTP真实合屏与失源重入；利用有界来源诊断收敛rk427已证实的deadline失败，不能无限重复无故障轮次充当修复；收尾内容A/V、线端和持续运行证据；最终双独立整体审查及QUALITY_SCORE更新。

r437不是已证实AV故障；rk433旧单源ProgressTimeout不是已证实ABI破坏；合屏全部失源保持输出是目标行为，不能要求普通RTP源结束必然自动EOF。原目标的显式停止或全部可信结束后关闭仍保留；普通RTP失源不等于可信EOF。动态输入、动态布局、混音和跨源拍摄时刻同步不在范围内。

依据：[原计划](realtime-video-composition-plan.md)、[r131/r133历史记录](realtime-video-composition-wiring-runtime.md)、[rk438](realtime-composition-rk438-validation.md)、[rk439](realtime-composition-rk439-validation.md)、[C门禁](realtime-composition-c-api-current-gates.md)、[总进度](../plan.md)。

独立审查确认矩阵范围与证据表述PASS；可信结束要求已按审查补回。
