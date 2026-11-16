# 合屏当前验收覆盖与收尾顺序

核验日期：2026-11-16；生产源码ce71265d，归档基线e2bc9fbe。两位未参与实现的独立智能体已给当前合屏范围整体SOURCE PASS；QUALITY_SCORE维持84/100。源码、C有限边界与双平台功能回归已完成，剩余实证不因此视为通过。下表为当前结论，后附历史记录保留当时原错，不重复列作当前未实现项。

| 覆盖项 | Windows | RKMPP | 当前边界或剩余事项 |
|---|---|---|---|
| 2/3/4源、异构、黑屏、重入 | r436/r449四源及选中源完整重入，早期源数记录保留 | rk422–425覆盖源数；rk445四源及选中源完整重入，rk450四源120秒/全黑 | 不重做全部源数排列；rk450本轮无重入 |
| HEVC CBR 8Mbps、TS/UDP | r436功能证据保留 | rk438诊断版本功能通过 | 后续共享逻辑已由r449/rk450同规格四源回归；不追加编码/协议全排列 |
| HEVC CBR 8Mbps、TS/RTP | r243等历史三源有限证据 | rk439四源120秒、四格/全黑、正常停止 | rk439不宣称本轮重入 |
| H264 VBR目标8Mbps、范围8–12Mbps、TS/RTP | r449最新共享源码四源120秒及选中源完整重入，成功水位、正常停止 | rk445实际prepared授权/首AU/成功水位及完整重入；rk450最新共享版本活动输出正常停止 | 两端caller_stop/exit0；不以未复现关闭rk427 |
| 选中音频失源/恢复与内容对应 | 既有失源恢复；r452联合输入/输出取证取得有限内容对应及SR基准归因 | rk426近静音/恢复；RKMPP同规格内容联合取证待完成 | r452不代表首次有效输出、重入、全程分位数或长稳 |
| C公共入口与生命周期 | 共享mapper/session已整体源码双审 | rk430–434配置寿命、负例、ABI、旧头实流；rk446正预算不足、rk448 RUNNING回调早停及外部release通过 | 本范围有限C门禁已闭合；rk448 encoded0，不冒称C消费者持续出流后回调停止或深层部分准备回滚 |
| 既有非合屏链路回归（原阶段一要求） | 当前共享版本VideoOnly/AudioVideo/动态多输出对应记录待补 | 同样不能用合屏内纯视频源替代独立链路回归 | 最小补单源及动态增删/复用/归零，不扩全排列，不改变旧EOF语义 |
| 最新停止语义与资源收尾 | r449 caller_stop/exit0，账本329191平衡归零 | rk450 caller_stop/exit0，账本208140平衡归零 | 账本不是RSS/VRAM总预算；短程内存观察不证明长期稳定 |

r452：110秒整段音频全源独立相关0.998703404，其他间隔至少1秒局部峰最高0.070474925；输入AAC在实际Skip Samples对齐后5291008样本与参考逐位相等，参考末992样本未覆盖。25组视频/音频SR给出输入基准偏差208.771–208.791ms；六个视频标记采样对应输出内容偏差225.805ms，扣除输入基准后的残差17.014–17.034ms。这是Windows本轮有限内容证据，范围不是全程误差置信区间，不把发送端基准差归为引擎故障。r451约230ms候选保留为此前仅输出捕获不足以归因的记录。

原计划明确MPEG-TS/RTP、H264/HEVC与CBR/VBR覆盖，未明确编码×RC×UDP/RTP全笛卡尔积。因此H264 CBR、HEVC VBR以及所有UDP扩展组合不增加为强制收尾门禁。既有原规格源始终120秒、至少1280×720/30fps/约8Mbps，不缩小失败复现条件。历史flash/beep的p95/p99及每小时数值目标保留其适用范围，未找到原合屏明确继承时不自动新增硬门禁。

优先顺序：完成RKMPP内容联合取证（rk453仍有周期候选），并补原计划明确要求的最新单源VideoOnly/AudioVideo与动态多输出回归；利用既有有界来源诊断收敛rk427真实期限失败，并补持续运行可靠性证据，不能用无限重复无故障轮次代替根因；明确真实线端证据边界，目前sender delivery_evidence仍not_proven，接收完整性不是NIC发送服务曲线。最终按已约定范围汇总结论；若新增功能修复，重新双独立源码审查及受影响平台回归。当前整体SOURCE双审和有限C门禁不再重复列作未完成。

r437不是已证实AV故障；rk433旧单源ProgressTimeout不是已证实ABI破坏；合屏全部失源保持输出是目标行为，不能要求普通RTP源结束必然自动EOF。原目标的显式停止或全部可信结束后关闭仍保留；普通RTP失源不等于可信EOF。动态输入、动态布局、混音和跨源拍摄时刻同步不在范围内。rk450压力5、HEVC输入告警及其他历史原错仍保留，不由最终errors0掩盖。

依据：[原计划](realtime-video-composition-plan.md)、[r449](realtime-composition-r449-windows-validation.md)、[rk445](realtime-composition-rk445-validation.md)、[rk450](realtime-composition-rk450-validation.md)、[C门禁](realtime-composition-c-api-current-gates.md)、[r452](realtime-composition-r452-input-clock-validation.md)、[质量评分](../QUALITY_SCORE.md)、[总进度](../plan.md)。

## 历史阶段记录（当时状态，当前结论以上表为准）

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
