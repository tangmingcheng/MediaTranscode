# Task 4 实施报告

## 状态与边界

已完成协议无关的非阻塞 Datagram transport、session 与异步 transmit timestamp evidence 旁路。本 Task 未接 production DAG、未修改 planner/CLI/Beta，也未提前替换旧 production direct sender；生产切换仍属于 Task 5。

提交：`d9aaf55f`（`feat(network): submit datagrams without completion gating`），未 amend、未 push。

交付文件：

- `src/internal/graph/runtime/network/MediaDatagramTransmitPort.h`
- `src/internal/graph/runtime/network/MediaDatagramTransmitSession.{h,cpp}`
- `src/internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.{h,cpp}`
- `src/internal/graph/runtime/network/windows/MediaWindowsDatagramTransmitPort.{h,cpp}`
- `src/internal/graph/runtime/network/linux/MediaLinuxDatagramTransmitPort.{h,cpp}`

旧 `MediaUdpDatagramSenderPort/Socket` 与 `MediaRtpUdpSenderTransport` 没有桥接到新 session：旧 open contract 缺少 session、service scope、generation、MTU authority、evidence policy 与 execution authority，若在本 Task 内适配只能伪造 planner 事实。Task 5 必须在 production DAG 拥有完整 planner 产品后直接切换并删除这项迁移债务。

## 状态机与失败语义

- `MediaDatagramTransmitSession` 只允许显式 `UserspaceNonblocking` 或 `LinuxSocketTxTime` execution plan，并按 shaping plan endpoint 建立一份 port。
- `trySubmit` 只有 `Submitted` 或 `WouldBlock` 成功结果；其他结果为 typed terminal failure。`WouldBlock` 不推进任何 session/evidence 状态，同一 bytes、endpoint、evidence ID、deadline 与可选 kernel launch time 可原样重试。
- batch 在进入 OS 前验证同 endpoint、同 `enqueueNotAfter`、完整 wire bytes 不超过 planner burst，且各平台预验证全部 entry 与 timestamp correlation 容量。零 entry 已提交时才允许返回 `WouldBlock`；短写、部分 batch、首 entry 后错误或不明确交付全部终止 session。
- `waitWritable(maximumWait)` 只接收调用方根据原 deadline 计算出的非负剩余时间；session/adapter 没有 deadline rebase 或 catch-up 接口。
- close/abort/submit/evidence 的第一个 terminal error 被保留；后续 close error 不覆盖首错。
- session 拒绝任何 adapter 报告的 zero-copy enablement；本轮没有 `MSG_ZEROCOPY`、completion queue 或 completion credit。

## 平台 adapter 与异步 evidence

- Windows 使用非阻塞 UDP socket、`WSASendMsg`、`WSAPoll`、`SIO_TIMESTAMPING`、`SO_TIMESTAMP_ID` 与 `SIO_GET_TX_TIMESTAMP`。requested/effective `SO_SNDBUF` 均保留为 capability telemetry；timestamp 配置失败时按 plan 的 `Report`/`Fail` 策略如实 unavailable/拒绝，绝不运行期改写发送语义。
- Linux/RK 使用非阻塞 UDP socket、`sendmsg`、`poll`、`SO_TIMESTAMPING`、`MSG_ERRQUEUE` 与 `SOF_TIMESTAMPING_OPT_ID`。`LinuxSocketTxTime` 只有 `SO_TXTIME` capability probe 成功才 open，且每次提交必须携带显式 kernel launch time；否则 fail-closed，不降级到 userspace。
- `MediaDatagramTransmitEvidenceCollector::drainAvailable()` 只更新 submitted/observed/late/lost/duplicate/cross-generation/unmatched 与 timestamp coverage telemetry。相关历史受 `maximumCorrelationEntries` 硬边界限制；TX timestamp coverage 即使完整，`deliveryEvidenceProven` 仍固定为 false，不宣称 receiver delivery。
- evidence `Report` 只报告覆盖缺口；`Fail` 可终止 graph，但 evidence 从不形成 shaper credit、协议 commit 或逐包 completion wait。

## 临时 TDD RED -> GREEN

临时源码为 `out/tdd/task4_transmit_tdd.cpp` 与 `out/tdd/task4_windows_loopback_tdd.cpp`，交付前均已删除，未新增 CMake test target、测试脚本或测试基础设施。

RED 使用 `/W4 /WX` 编译，因 `MediaDatagramTransmitSession.h` 尚不存在以 C1083 失败；失败来自 Task 4 接口缺失，不是测试语法错误。

fake-port GREEN 使用 Debug ABI 与 `/W4 /WX` 编译运行，输出：

```text
Task 4 transmit session checks passed
```

覆盖 `WouldBlock -> WouldBlock -> Submitted` 原请求幂等、Submitted-only 计数、同 deadline/burst batch、mixed deadline 拒绝、partial/ambiguous terminal、首错保留、late/lost/duplicate/cross-generation evidence、Report/Fail policy，以及 timestamp 不等于 delivery proof。

Windows 真实 loopback GREEN 使用 `WSASendMsg` 发送到独立 Winsock receiver，receiver 收到完全相同 payload，输出：

```text
Task 4 Windows loopback checks passed; timestamp=2
```

`timestamp=2` 对应 `Available`；同时核对 requested/effective `SO_SNDBUF` 非零、zero-copy=false、drain 与 close 成功。

## 构建与冻结证据

- Task 4 core、Windows adapter 与 Windows 上的 Linux unsupported stub 均通过 focused `/W4 /WX`。
- 最终 VS2026 clean-first Debug：clean 554 files，Ninja 分母 555，脚本报告 configure/build exit 0，并验证 local/realtime 两个 CLI artifact。
- WSL Ubuntu 存在，但没有 `g++`、`clang++` 或 `c++`；因此本 Task 不声称 Linux/RK Release 已构建。Linux/RK 权威构建与真实 socket diagnostics 必须在目标环境继续执行。
- 新文件均为 UTF-8 无 BOM、CRLF；`git diff --check` 通过。
- `task4*` 临时 source/exe 与 focused root obj 全部删除。
- 未暂存用户已有 `AGENTS.md`、`CMakeLists.txt`、FFmpeg headers、`out/` 或 2026-08-19 旧计划。

## 已知风险与 Task 5 交接

- Linux/RK adapter 仅完成基于权威 Linux headers 的条件编译实现，本机没有 Linux C++ 工具链，必须在 RK `ffenv on` Release 构建与真实 error-queue/SO_TXTIME capability probe 中验证。
- 当前 Windows loopback 证明了 socket 原子提交与 timestamp capability，未制造稳定 socket pressure，真实 `WouldBlock`/`WSAENOBUFS` 压力与 deadline 行为需由 Task 5 production sender 门禁补齐。
- Task 5 sender 必须以 scheduled entry 的原 `enqueueNotAfter` 计算 writable wait，并仅在 `Submitted` 后 commit 同一 lease；不得等待 collector、不得把 timestamp coverage 当 receiver delivery。
- Task 5 production cutover 后需重新确认旧 direct UDP/RTP transport 与 `MediaForwardOnlyDatagramPacer` 无消费者，再删除，禁止保留两套发送状态机。

## Fix round 1（覆盖前述实现细节）

本轮依据独立审查结论重构了 Task 4 原子边界，未进入 Task 5 production DAG：

- correlation/admission 只保留在 `MediaDatagramTransmitEvidenceCollector`，adapter 不再维护第二份 job ledger。全部 evidence ID、uint32 platform ID、launch-time low bits、range、重复与容量在第一次 OS submit 前整批原子预留；失败不触碰 socket。
- `MediaDatagramTransmitSession` 持有唯一 pending job。`WouldBlock` 后只能先在原 deadline 内等待同 endpoint writability，再提交 Session 保存的同一 requests；每次 submit 前复核非负单调时钟与原 deadline。batch datagram/byte、wire burst、endpoint 与 payload 上限均执行 planner 硬边界。
- submit failure 结构化区分 `TerminalNoSubmit`、`PartialSubmittedPrefix` 与 `AmbiguousSubmittedPrefix`。非法 endpoint、负 wait、未知 execution/submit/wait/capability enum、clock 回退均保留首错并 poison session；worker stop 只返回 `Stopped`，由调用方以明确 worker-local cause 调用 `abort`，不制造无因 `Cancelled`。
- evidence `Report` 在 ledger 压力或 timestamp 缺失时允许基础发送继续，`Fail` 对 late/lost/duplicate/unmatched 生效。Windows timestamp 保留 QPC raw counter、source 与 frequency，不伪装为 nanoseconds；所有 timestamp 仍只作异步 telemetry，不参与 credit 或 completion wait。
- Linux/RK adapter 完整重建并修正全部 `errno` 拼写；`SO_TXTIME` error queue 独立于 timestamp evidence 始终 drain。`SO_EE_ORIGIN_TXTIME` 的 missed/invalid launch 通过 launch-time low bits 关联已提交 job 后终态失败；`MSG_CTRUNC`、`cmsg_len`、`ee_errno/origin/info/code/data`、control alignment 与 size narrowing 均 fail-closed。未启用 `MSG_ZEROCOPY`。
- Windows adapter 改用 `WSAEventSelect` 加独立 stop event 等待 writability，`SIO_GET_TX_TIMESTAMP` 只按 collector 提供的 outstanding IDs 异步查询；Windows 真实 loopback 已验证 `WSASendMsg` payload 原样到达。

临时 RED 使用旧接口编译失败，证明审查要求的新 typed failure/event/job 边界缺失；GREEN 临时 fake-port `/W4 /WX` 覆盖 pending retry、wait-before-retry、batch bound、partial prefix、worker stop、Report lost 与 SO_TXTIME missed terminal，进程退出码 0。Windows 真实 loopback同样以 `/W4 /WX` 编译运行，退出码 0。所有临时源码、目标与 exe 在提交前删除，未接入 CMake。

本轮 fresh VS2026 x64 Debug clean-first all-target 已运行一次并成功：clean-first 后完成 555 个 Ninja step，脚本报告 configure/build exit 0，local/realtime CLI artifacts 均存在。最终冻结后将再次执行同一 clean-first 脚本。

WSL 具有 Linux 5.15 headers，但本机没有 `g++`、`clang++` 或 `c++`，Docker Desktop engine 也未运行，因此不能声称 Linux/RK body 已编译。该风险不以 Windows `#else` stub 冒充验证；RK `ffenv on` Release 与真实 `MSG_ERRQUEUE/SO_TXTIME` 仍是目标机门禁。
