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

- Windows 使用非阻塞 UDP socket、`WSASendMsg`、`WSAEventSelect`、`SIO_TIMESTAMPING`、`SO_TIMESTAMP_ID` 与 `SIO_GET_TX_TIMESTAMP`。requested/effective `SO_SNDBUF` 均保留为 capability telemetry；timestamp 配置失败时按 plan 的 `Report`/`Fail` 策略如实 unavailable/拒绝，绝不运行期改写发送语义。
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

## Fix round 2

本轮继续收紧 Task 4 边界，仍未接入 Task 5 production DAG：

- 新增独立 `MediaDatagramTransmitKernelSchedulePlan`，以部署/planner authority 明确 `SO_TXTIME` correlation entries、run datagrams、error-queue residence 与最大 schedule-ahead。`LinuxSocketTxTime` 不再依赖可选 transmit timestamp evidence；无 evidence 时仍保留 launch low-bits correlation 并始终 drain error queue。Session 原 deadline 与 Linux `CLOCK_MONOTONIC` launch window 分别 fail-closed。
- Windows 不再把所有 Winsock timestamp 无条件标为 QPC。adapter 动态调用系统 `GetBestInterfaceEx`、`ConvertInterfaceIndexToLuid` 与 `GetInterfaceActiveTimestampCapabilities` 确认实际 egress interface 和唯一 active source；软件源才使用 `QueryPerformanceFrequency`，硬件源必须取得 `HardwareClockFrequencyHz`。API、source 或 frequency 无法权威确定时如实 `Unavailable`。
- Session 严格验证 port failure enum/prefix：no-submit 只能为 0，partial 必须位于 batch 内且非 0，ambiguous 只携带确定提交的前缀；无效 metadata 不再 clamp，转为 prefix 0 的 `AmbiguousSubmittedPrefix` terminal。所有 evidence/correlation 状态在 OS 前已预构造，OS 后 `markSubmittedPrefix` 为不可失败提交操作，Task 5 可使用 terminal error 中的确定 prefix。
- Caller-selected Windows timestamp 的完整 planned evidence ID range 在 Session open 前及 collector create 时双重验证为 uint32 可表示。Report ledger 满时，Windows per-job reservation 不再生成 `SO_TIMESTAMP_ID`；Linux kernel sequential ID 因 socket API 语义继续推进，但不建立第二份 correlation ledger，并受 typed run budget 约束。
- Linux UAPI 以 `LINUX_VERSION_CODE` 和对应 socket/error-queue feature macros 做 compile-time capability guard。旧 sysroot 缺少 timestamping 或 `SO_TXTIME` 时可以完成编译并返回 typed unavailable/unsupported，不直接引用缺失符号。
- timestamp duplicate entry 保留到权威 residence 到期，因此 duplicate 与 unmatched 可区分。所有 telemetry counter 使用饱和递增并通过 `counterSaturated` 如实标记，coverage 不会在 counter 饱和后误报 complete。
- `MediaDatagramTransmitSession` 与 platform port 明确为创建线程单一 owner，所有 submit/wait/drain/close 禁止并发和线程迁移；运行期检查违反合同时 fail-closed，避免 close/wakeup handle 竞争。

临时 round2 TDD 使用 `/W4 /WX` 验证无 evidence 的 typed TXTIME session、invalid prefix 不 clamp、Report untracked 不请求 caller-selected timestamp ID、duplicate 分类、uint32 planned range 拒绝和 TXTIME invalid terminal，退出码 0。Windows 真实 loopback 使用实际 `WSASendMsg`，payload 原样收到；timestamp capability 仅允许 authoritative API 证明后的 typed source/frequency，否则验证为 `Unavailable`。临时测试不接入 CMake，提交前删除。

RK 权威环境按当前 `AGENTS.md` 使用 `192.168.130.229`、`/home/tang`、`ffenv on`、`source /opt/mt-tools/mtenv.sh` 与 `mtenv on`。本轮未覆盖 `/home/tang/MediaTranscode`：先只读复制生产树到 `/home/tang/task4-round2-build.WM0Uu0`，再叠加本地冻结工作树的 Task 4 源码、现有 CMake 输入与 FFmpeg headers。overlay 为 1,149,850 bytes，本地和远端核对 SHA-256 均为 `117af27730b89e4c941f4b2a672c807fe2eadc2165976d4f8209c669143db3b4`。

隔离构建实际执行环境与命令为：

```text
ffenv on
source /opt/mt-tools/mtenv.sh
mtenv on
cmake -S /home/tang/task4-round2-build.WM0Uu0 -B /home/tang/task4-round2-build.WM0Uu0/out/build/task4-round2-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /home/tang/task4-round2-build.WM0Uu0/out/build/task4-round2-release --clean-first
```

记录到 remote shell PID `3718405`、configure PID `3718418`、build wrapper PID `3718565` 与 Ninja PID `3718575`。GCC 12.4 的 Linux/aarch64 Release clean-first 完成全部 `554/554` step；其中 `MediaLinuxDatagramTransmitPort.cpp`、`MediaDatagramTransmitSession.cpp` 与 `MediaDatagramTransmitEvidenceCollector.cpp` 均真实编译，不是 Windows stub。build log 没有 `FAILED:`、`ninja: build stopped` 或 `error:`；两个最终链接产物如下：

```text
media_transcode_local_video_cli     5904168 bytes  sha256=380d08bbea9a82c7510679e392c72a192caad6fa452b0754d9e702b797f3d278
media_transcode_realtime_video_cli  6883008 bytes  sha256=7b5a942e74f12985ed9d8c145d186417dea16f804e11ee2d7250aa82c2ff29bb
```

SSH 前台输出在 build 结束前脱离，未捕获脚本内的单独 `BUILD_RC=` 行；因此本报告以 `554/554` 最终链接、两个产物及零 failure marker 作为完成证据，不伪造缺失的 rc 文本。构建后验证 resolved path 精确等于 `/home/tang/task4-round2-build.WM0Uu0` 才递归删除；输出 `REMOTE_TEMP_REMOVED`，后续仅观察到检查命令自身，无 build/Ninja 残留。本地 overlay 与临时 RK build script 同步删除。

## Fix round 3

- Linux submit 仅把 `EAGAIN/EWOULDBLOCK` 映射为 `WouldBlock`，Windows 仅接受 `WSAEWOULDBLOCK`；`ENOBUFS/WSAENOBUFS` 与其他 pressure/native error 均进入 typed terminal failure，已提交前缀仍按实际值保留。
- `SO_TXTIME` low-bit correlation 不再只保留 `maximumErrorQueueResidence`。create 在任何 OS open 前验证 `maximumScheduleAheadNanoseconds + maximumErrorQueueResidence` 可由 `MediaRunningTime` 表示；每个 entry 在 OS submit 前保存 typed `launchCorrelationRetainUntil`，回收及 low-bit 复用均等待该完整 horizon，从而覆盖合法最晚 launch 后才抵达的 error queue 事件。
- 业务 `close()` 继续执行 single-owner 检查并 fail-closed；Linux/Windows adapter 的 `noexcept` 析构改走私有 `forceCloseForDestruction()`，无论销毁线程是否为 owner 都无条件释放 socket、eventfd、WSAEVENT 与 HANDLE。该析构前提仍是不与业务操作并发，跨线程迁移本身不会再造成资源泄漏。

临时 Windows `/W4 /WX` TDD 验证 `WSAEWOULDBLOCK` 与 `WSAENOBUFS` 分类、完整 TXTIME horizon 边界及溢出拒绝、真实 `WSASendMsg` loopback payload，以及 32 次非 owner 线程销毁后的进程 handle 计数无增长，退出码 0。临时 Linux TDD 在 RK 以 GCC 12.4、`-Wall -Wextra -Werror` 编译，验证 `EAGAIN/EWOULDBLOCK=true`、`ENOBUFS/ECONNREFUSED=false`，退出码 0。

冻结后的 Windows VS2026 Debug clean-first 完成 555 step，configure/build exit 0。RK 使用隔离目录 `/home/tang/task4-round3-build.6YdXRU`，overlay SHA-256 为 `5165254bc71e3fc19fde2af5e44181ce35d0cd887c22889b49e61dd138121119`。首次 configure 如实因非权威 `/usr/bin/ffmpeg` 被选择而失败；删除尚未生成产物的隔离 build dir 后，以 `ffenv on`、`source /opt/mt-tools/mtenv.sh`、`mtenv on` 并显式确认 `/usr/local/bin/ffmpeg` 重新 fresh configure。第二次 configure rc 0、Release clean-first build rc 0、`554/554`，PID 为 shell `3882947`、configure `3883013`、build wrapper `3883465`、Ninja `3883469`；build log 无 failure marker。构建与 TDD 后隔离目录、脚本、源码、目标和进程全部删除并复核无残留。
