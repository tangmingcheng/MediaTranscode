# 合屏提交服务边界反馈计划

## 问题与依据

r319/r320 原规格实流未再触发期限错误，但当前前缀 planner 仅使用零债务的最大速率服务下界，不能证明完整周期可调度。此项继续补齐现有时序闭环，不增加公开输入、外部依赖或平台专属媒体链路。

WebRTC 的 [PacingController](https://webrtc.googlesource.com/src/+/c52e627c831ace2e6e6b0738715a51d851cd4865/modules/pacing/pacing_controller.cc)在真实发送后计入服务债务，随时间消减，并用债务推导下一可发送时刻。本项目已有 GBRA reservation.serviceDuration 与 submitCompletedAt 推导 theoreticalArrivalTime；应传递已有权威事实，不以均值、日志或经验余量重算。

已核验：sender 正常与 partial 路径先 recordSubmittedPrefix 再 markSubmitted；UDP/RTP transaction 先 global commit 再 nested protocol commit，后者可唤醒 producer。global commit 自身也可能通知 materializer。scope SubmitGuard.complete 在此之前更新共享 theoreticalArrival。sender 当前严格单 datagram 提交。

## 设计约束

- 单次提交观察携带 generation、确认的 global sequence、physical submit completion、实际 reservation service duration，以及 checked-add 得到的 sender frontier。session、scope、clock domain 由已绑定 owner 核验，禁止只比较数值时间。
- 经既有 commit slice/type-erased transaction 传入 global ledger，同锁发布提交与服务观察，先于所有相关通知；producer 仅在上一前缀完成后消费，并核验代与序号对应。
- scope frontier 如接入，由 SubmitGuard.complete 在原锁内形成并返回；不在调用方重新计算，不新增跨线程裸读取。其他成员可随后推进该值，所以它是观察下界，不是服务预约。
- 不将 markSubmitted 移到真实 prefix commit 之前。观察构造或 pacing/scope 检查失败时，仍须记录确定已发生的提交，随后保留原错误进入 terminal；不可因反馈计算失败抹掉物理发送事实。
- Submitted/确定 partial 只记录确认范围，入口显式限制单 datagram；WouldBlock 不发布提交观察；ambiguous 不生成确认序号的正常 frontier。必要时以不可用观察表达失败，禁止伪造默认时间。
- 新代不继承旧 session 观察；单 session purge 不清除共享 scope 的其他成员债务。
- 不新增线程、媒体引用或队列。复核 type-erased model、global owner 与实际 sizeof 计费，避免新增标量遗漏存储准入。
- planner 将可核验 frontier 与真实 now/release 合并为下一候选的必要服务起点，仍保留原 AU/维护期限与 sender 失败语义。该反馈不能代替完整周期义务分析，也不保证 OS 调度。

## 实施顺序与验收

1. 定义内部提交事实及不可用观察的失败表示；沿正常、partial、ambiguous、WouldBlock 梳理全部调用，明确错误优先级。
2. 接通 sender→commit slice→UDP/RTP transaction→global ledger 的同锁发布；保留已发送事实、通知和生命周期边界。覆盖共享 scope 观察的身份及失效语义。
3. 接入前缀 planner，并检查 PCR/PSI 周期义务覆盖；两个 next 实例不是全周期保证。
4. 冻结代码，两个未参与实现的独立智能体逐项复审；Release 全量重建后执行原规格 realtime、VLC 原生快照、源结束/重入与正常停止验证，记录命令/PID/指标后逐项清理。
5. Windows 完整门禁通过前不推进 RKMPP。声音/静音、内容级 A/V、NIC 出线时序、完整矩阵及旧异常退出风险仍保留，不因本切片成功而关闭。

## 当前进度

两名独立审查者均对上述方向给出有条件设计 PASS，条件已纳入本计划；不是实现审查通过。本轮仅完成调用链与失败语义调查，尚未修改服务反馈生产代码或启动新测试。r320 产物及进程已清理。

## 第一切片实现

已接通sender实际reservation→commit slice/type-erased→UDP/RTP transaction→global同锁保存的generation/sequence/submittedAt/serviceDuration。frontier由checkedAdd按需计算，失败不阻断物理事实记录；正常/确定partial保留原commit后markSubmitted顺序，WouldBlock无事实，当前单包ambiguous无确认前缀。purge清事实与预约。诊断输出同一快照的原始事实及available/absent/unavailable状态。两个独立审查者对最终源码均静态PASS。

此切片尚无scope反馈或planner消费；身份限于绑定owner时钟域。新增固定optional位于原ordinary-new global owner，完整owner预算原缺口仍在，不宣称内存准入闭合。Release正在重建，r321尚待执行。

第一切片已完成Release745项全量重建及r321原规格Windows实流核对：最后service sequence与实际提交均86110，completed+duration=available精确一致，错误0，payload回零。记录见r321。仅证明sender→提交账本事实接线；下一步仍为scope观察与planner消费及完整周期分析。

## 共享服务域观察切片

SubmitGuard.complete 已在原锁内返回 scope theoreticalArrival，沿实际提交事务保存到 global ledger；零提交不生成观察。确认提交后即使 scope 记账失败，仍尝试保留实际提交前缀，再返回原错误。failSubmit 的前缀记账与 pacing 二次失败仅记诊断，最终保留原 transport cause；两个独立审查者均对该切片静态 PASS。Release 745 项 clean-first all-target 全量重建成功，configure/build 均 exit 0；必需的 CMakeCache.txt、build.ninja 和两个 CLI 产物齐全。未运行 local CLI；尚无本切片实流结果。

mux 尚未保存 transport owner 或消费服务观察；后续需在同一代与确认序号下核验 sender/scope frontier，并与真实 now 分开输入 planner，不能用未来 frontier 替代维护是否到期的时钟。完整周期义务、内存准入和全量验收仍未完成。

共享域切片已完成r322原规格实流核对，详见 realtime-composition-r322-validation.md：84541数据报发布/提交一致，同次scope/sender available均149012608760ns，错误0、payload回零；VLC原生快照三格→全黑，调用方停止exit0。12项临时产物及5个进程均清理。该切片未证明planner消费、完整周期或整体合屏完成。

## planner 消费切片

已在 mux 保存绑定 transport owner，同一 snapshot 校验代、poisoned、提交序号及服务身份。媒体前缀、pending维护与独立维护路径均传递观察；planner 必须取得有效 scope 观察并 checked-add sender frontier，取 release/真实now/sender/scope 最大值作为必要最早服务起点。维护到期判断仍用真实now，原期限不扩展。首次没有提交允许无观察，不宣称共享域空闲。

两名独立审查者均静态PASS；时钟依赖共同output authority，purge安全依赖adapter现有generation commit permit，helper不可脱离该生命周期调用。Release745项clean-first all-target重建成功，configure/build均exit0，四项必需产物核验齐全；未运行local CLI，尚待realtime实流复验。该观察未覆盖尚未提交队列和未来共享域占用；完整周期义务仍未闭合。

planner切片r323实流完成：84905数据报发布/提交一致，错误0，payload回零，三格→全黑，调用方停止exit0；记录见r323，产物与进程已清理。

后续完整周期审查确认：只枚举AU deadline前事件不充分。须包含mux待发布、FileMux/protocol edge、materializer未预约、global未提交及sender等待工作，按身份去重；现有global字节/包数缺release/deadline/service，不能恢复全时序。须定义已接受工作快照或累计服务合同，并依据busy-period/响应时间分析证明有限horizon；不能直接扩展两个next数组即宣称闭合。shared scope其他成员影响仍需合同证明。

已接受工作覆盖与周期闭合的调查及下一阶段契约见 realtime-composition-accepted-work-plan.md。已有series/global身份可复用，但目前无统一覆盖映射；设计尚未冻结，未将调查记为实现完成。

## 2026-11-16 rk427 复核与证据缺口

rk427发送器预约失败时，not_before比not_after晚87414ns；预约前失败，没有进入wait/submit，不能据activation与permit之间间隔归因OS。当前4aef532e诊断已输出previous_submitted_sequence、last_observed与theoretical_arrival；last_observed也会在预约成功时更新，不能当作上一包提交完成时间。下一原规格复验rk435优先关联终态sender lastSubmittedService的真实完成时间、duration与scope frontier。

独立源码复核：acceptedWorkObservation局部wire snapshot包含提交事实；AcceptedWorkPlanner核序号后max(observedNow,sender,scope)，但返回产品只有availableAt，publication descriptor没有当时消费的序号和frontier来源。因此不能拿失败时global latest反推规划时观察。若现有诊断仍不足，需要沿现有publication holder保存有界只读关联，覆盖普通AU与maintenance，不另造调度状态、不放宽deadline或速率上限；该扩展尚未实现。WebRTC发送债务与队列速率调整可供对照，但其突发/债务限制不等价本项目不可变deadline，不能直接移植为绕过失败的方案。rk429/430有限成功不关闭rk427。
