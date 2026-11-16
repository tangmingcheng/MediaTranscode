# 合屏当前验收覆盖与收尾顺序

核验日期：2026-11-16。由未参与本轮实现的独立智能体核对原计划与逐轮记录，主线程复查原计划“目标与约束”。此表不改变目标，不把部分证据记为整体完成。

| 覆盖项 | Windows | RKMPP | 尚需处理 |
|---|---|---|---|
| 2/3/4源、异构、黑屏、重入 | r436四源及选中源重入，早期源数记录保留 | rk422–425已覆盖 | 不重做全部源数排列 |
| HEVC CBR 8Mbps、TS/UDP | r436最新功能修复，早于e127a744诊断 | rk438同诊断源码通过 | Windows共享诊断回归 |
| HEVC CBR 8Mbps、TS/RTP | r243等历史三源有限证据 | rk439四源120秒、四格/全黑、正常停止，媒体/控制已提交 | rk439不重复宣称重入已测 |
| H264 VBR目标8Mbps、范围8–12Mbps、TS/RTP | r443四源及选中源完整重入功能回归通过 | rk441四源及选中源完整重入，功能检查通过 | rk445验证新RKMPP完成合同；随后回归最新共享改动的Windows路径 |
| 选中音频失源/恢复 | 既有记录；r437内容关联仍有重复画面歧义 | rk426近静音与恢复 | 内容级A/V及持续运行收敛 |
| C公共入口 | 已实现；跨平台共享适配仍需最终审查 | rk430–434配置寿命、负例、ABI、旧头实流和启动回调边界 | 按公开契约补运行回调/压力等有限残项 |

原计划明确MPEG-TS/RTP、H264/HEVC与CBR/VBR覆盖，未明确编码×RC×UDP/RTP全笛卡尔积。因此H264 CBR、HEVC VBR以及所有UDP扩展组合暂不增加为强制收尾门禁。既有原规格源始终120秒、至少1280×720/30fps/约8Mbps，不以缩小失败复现条件替代rk427四源可靠性复验。

优先顺序：完成rk445新RKMPP完成合同实际授权、水位放行与原规格失源重入；回归本轮共享PTS/DTS校验后的Windows路径；利用有界来源诊断收敛rk427已证实的deadline失败，不能无限重复无故障轮次充当修复；收尾内容A/V、线端和持续运行证据；最终双独立整体审查及QUALITY_SCORE更新。

r437不是已证实AV故障；rk433旧单源ProgressTimeout不是已证实ABI破坏；合屏全部失源保持输出是目标行为，不能要求普通RTP源结束必然自动EOF。原目标的显式停止或全部可信结束后关闭仍保留；普通RTP失源不等于可信EOF。动态输入、动态布局、混音和跨源拍摄时刻同步不在范围内。

依据：[原计划](realtime-video-composition-plan.md)、[r131/r133历史记录](realtime-video-composition-wiring-runtime.md)、[rk438](realtime-composition-rk438-validation.md)、[rk439](realtime-composition-rk439-validation.md)、[C门禁](realtime-composition-c-api-current-gates.md)、[总进度](../plan.md)。

独立审查确认矩阵范围与证据表述PASS；可信结束要求已按审查补回。

## r440新增失败（2026-11-16）

Windows新共享版本四源H264 VBR/RTP已执行，首轮约93秒因audio等待缺失video144ms导致原deadline过期而exit1；最新回归为FAIL，不覆盖旧r131/r133的有限通过。优先核共享AV scheduler排序证明合同，不能把该失败误记为rk427 sender失败或FFmpeg问题。RKMPP该编码矩阵可继续取得独立平台证据，整体仍未完成。见[r440](realtime-composition-r440-windows-validation.md)。

## rk441更新

H264 VBR目标8/范围8–12Mbps、720p30 MPEG-TS/RTP四源、全黑和选中源完整120秒重入已取得RKMPP功能证据，CLI caller_stop/exit0、errors0。pressure19、系统UDP错误增量及delivery not_proven保留，不判整体可靠性通过。见[记录](realtime-composition-rk441-validation.md)。

## r443共享进度回归

9cac2eb0 Release四源H264720p30 VBR目标8/范围8–12Mbps MPEG-TS/RTP及选中源完整120秒重入功能回归通过，四张VLC原生图确认，CLI caller_stop/exit0，errors/pressure/deadline_misses0、账本348326归零。r440原参数本轮通过，但无专门水位放行计数，不宣称已穷尽原失败所有时序；rk427、内容A/V、长稳及RKMPP进度授权仍未闭合。18临时文件与七PID已清理。见[r443](realtime-composition-r443-windows-validation.md)。

## 50993608 收尾核验

RKMPP完成合同复用接入经双独立源码审查通过并本地提交；目标增量构建中，未宣称实流通过。剩余门禁保持：实际prepared授权/完整AU/成功水位提交、当前共享修改的Windows回归、rk427期限失败与持续运行可靠性、接收端内容A/V和线端证据、C接口RUNNING回调及正预算不足准备失败、最终整体双审与QUALITY_SCORE。已有2/3/4源和编码/协议功能记录继续有效，不扩充全笛卡尔积。

## rk445 更新

50993608原规格四源H264 VBR/RTP及选中源完整120秒重入功能检查通过；实际prepared授权、首完整AU及成功水位放行已有正例，caller_stop/exit0、账本归零。压力6和HEVC告警保留，详见realtime-composition-rk445-validation.md。优先继续最新共享代码Windows回归和C有限残项，不把本轮通过等同rk427或内容同步关闭。
