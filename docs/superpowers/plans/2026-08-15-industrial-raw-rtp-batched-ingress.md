# Industrial Raw RTP Batched Ingress Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace per-datagram raw-RTP receive work with a planner-owned, capability-selected, bounded batched ingress module shared by Windows and RKMPP/Linux, without changing the existing DAG media path.

**Architecture:** Preflight retains the exact RTP/RTCP sockets, records source and platform facts, and supplies a typed `MediaRtpIngressPlan`. A deep `MediaRtpIngressReceiver` owns reusable bounded storage and delegates only OS receive mechanics to Windows or Linux adapters. `RawRtpInputNode` consumes batches through its existing RTP/RTCP parser, reorder, depacketizer, clock, and DAG output logic; no codec, platform, or acceptance-specific chain is introduced.

**Tech Stack:** C++20, Winsock2/IOCP/RIO capability APIs, Linux sockets/`recvmmsg`, optional capability-gated `io_uring` zero-copy RX, FFmpeg DAG runtime, CMake/Ninja, VS2026, RKMPP target FFmpeg environment.

---

## Delivery constraints

- [ ] Stay on `codex/rkmpp-zero-copy`; never modify `master` and never create a C-drive worktree.
- [ ] Preserve unrelated working-tree changes, untracked FFmpeg headers, and user-owned `out/` content.
- [ ] Use temporary TDD only. Each RED/GREEN source, CMake target, executable, object, PDB, ILK, and log is deleted immediately after its task.
- [ ] Keep UTF-8 CRLF in every delivered text file.
- [ ] Do not lower input resolution, bitrate, frame rate, duration, codec conversion, scaling, audio, or playback requirements.
- [ ] Commit and push each completed phase to the same branch.

## Task 1: Capture authoritative platform capability evidence

**Read-only targets:**

- Windows Winsock extension availability and current socket implementation.
- RK target kernel, libc, socket APIs, `io_uring` zero-copy RX prerequisites, NIC/driver, and FFmpeg environment.

- [ ] On Windows, inspect `WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER)` support for RIO, IOCP/overlapped UDP requirements, registered-buffer alignment, cancellation, and receive-buffer reporting.
- [ ] On `192.168.96.211`, run read-only commands only after `ffenv on` to record kernel version/config, libc, `recvmmsg`, `io_uring` features, network driver, receive-queue limits, and available build headers/libraries.
- [ ] Record evidence and unavailable reasons in `docs/superpowers/specs/2026-08-15-industrial-raw-rtp-batched-ingress-design.md`; do not infer support from operating-system name.
- [ ] Select implementation scope from evidence: implement every proven production-capable adapter; represent unsupported advanced adapters as explicit unavailable scanner results, never runtime fallback.
- [ ] Commit and push the evidence update.

## Task 2: Add typed capability, observed-source facts, and ingress plan

**Files:**

- Create: `src/internal/graph/planner/realtime/MediaRtpIngressCapability.h`
- Create: `src/internal/graph/planner/realtime/MediaRtpIngressCapabilityScanner.h`
- Create: `src/internal/graph/planner/realtime/MediaRtpIngressCapabilityScanner.cpp`
- Create: `src/internal/graph/planner/realtime/MediaRtpIngressObservation.h`
- Create: `src/internal/graph/planner/realtime/MediaRtpIngressPlan.h`
- Create: `src/internal/graph/planner/realtime/MediaRtpIngressPlan.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeInputPlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpInputPlanValidator.cpp`
- Modify: `CMakeLists.txt`

- [ ] Create a temporary compile/runtime TDD probe that fails because typed capability and `MediaRtpIngressPlan::create` do not exist.
- [ ] Define stable enums for adapter, reusable-storage ownership, completion evidence, and cancellation contract.
- [ ] Define direct external facts separately from observed source facts. External facts may name sender datagram geometry or deployment reorder-latency SLA only when directly known; callers never supply batch counts, queue sizes, or derived memory values.
- [ ] Make `MediaRtpIngressPlan::create` derive checked byte/descriptor/reorder bounds only from capability, socket facts, prepared-input budget, and source observations.
- [ ] Remove `RtpReceiveBufferBytes`, `RtpMaximumDatagramBytes`, `RtpReorderWindowPackets`, and `RtpMaximumReorderDelayMs` from `MediaRealtimeInputPlanner.cpp`.
- [ ] Make incomplete facts, arithmetic overflow, unsupported adapter, or internally inconsistent ownership/cancellation fail before graph construction.
- [ ] Run the temporary GREEN probe for exact products and negative cases, then delete every temporary source, target, and artifact.
- [ ] Commit and push the typed planner phase.

## Task 3: Collect exact ingress observations during prepared preflight

**Files:**

- Modify: `src/internal/graph/planner/realtime/MediaRawRtpInputPreparer.h`
- Modify: `src/internal/graph/planner/realtime/MediaRawRtpInputPreparer.cpp`
- Modify: `src/internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.cpp`
- Modify: `src/internal/graph/runtime/buffer/MediaRawRtpPreparedByteBudget.h`
- Modify: `src/internal/graph/runtime/buffer/MediaRawRtpPreparedByteBudget.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaPreparedRealtimeInput.h`
- Modify: `src/internal/graph/planner/realtime/MediaPreparedRealtimeInput.cpp`

- [ ] Create a temporary RED probe for missing observed maximum datagram bytes, packet count/rate, maximum sequence displacement, arrival variation, socket capacity, and truncation evidence.
- [ ] Extend prepared capture to record facts while retaining the same RTP/RTCP sockets and existing signaling packets.
- [ ] Distinguish bootstrap discovery storage from final runtime storage; bootstrap may use the named UDP protocol payload limit only until the exact observed geometry is sealed.
- [ ] Seal observations before planner finalization; reject truncation, budget overflow, regressing observation time, or insufficient evidence.
- [ ] Preserve the existing prepared replay clock and byte budget semantics without copying media logic.
- [ ] Run GREEN including same-socket ownership and exact byte-accounting checks, then delete the temporary probe and artifacts.
- [ ] Commit and push preflight observation support.

## Task 4: Build the bounded deep ingress module

**Files:**

- Create: `src/internal/graph/protocol/rtp/ingress/MediaRtpIngressBatch.h`
- Create: `src/internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h`
- Create: `src/internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.cpp`
- Create: `src/internal/graph/protocol/rtp/ingress/MediaRtpIngressReceiver.h`
- Create: `src/internal/graph/protocol/rtp/ingress/MediaRtpIngressReceiver.cpp`
- Create: `src/internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapter.h`
- Modify: `src/internal/graph/runtime/network/MediaUdpSocket.h`
- Modify: `src/internal/graph/runtime/network/MediaUdpSocket.cpp`
- Modify: `CMakeLists.txt`

- [ ] Create a temporary RED probe for bounded batch construction, stable storage leases, multi-entry ordering, cancellation, overflow, and zero steady-state allocation counters.
- [ ] Allocate the complete descriptor and byte arenas once from `MediaRtpIngressPlan`; expose only non-owning bounded entry views whose lifetime is tied to the returned batch lease.
- [ ] Make RTP/RTCP channel, exact byte count, and monotonic observation time mandatory for every entry.
- [ ] Keep one receiver interface for both platforms and make platform adapters depend only on planned sockets/storage/cancellation facts.
- [ ] Reject truncation, descriptor exhaustion, byte exhaustion, invalid completion, ownership violation, or cancellation failure; do not resize or drop.
- [ ] Run GREEN and allocation instrumentation, then delete all temporary TDD material.
- [ ] Commit and push the shared ingress module.

## Task 5: Implement Windows completion-driven batch receive

**Files:**

- Create: `src/internal/graph/protocol/rtp/ingress/windows/MediaWindowsRtpIngressCapabilityScanner.cpp`
- Create: `src/internal/graph/protocol/rtp/ingress/windows/MediaWindowsRtpIngressAdapter.h`
- Create: `src/internal/graph/protocol/rtp/ingress/windows/MediaWindowsRtpIngressAdapter.cpp`
- Modify: `src/internal/graph/runtime/network/MediaSocketRuntime.h`
- Modify: `src/internal/graph/runtime/network/MediaSocketRuntime.cpp`
- Modify: `src/internal/graph/runtime/network/MediaUdpSocket.h`
- Modify: `src/internal/graph/runtime/network/MediaUdpSocket.cpp`
- Modify: `CMakeLists.txt`

- [ ] Create a temporary Windows RED probe using real loopback UDP sockets and more than one datagram per receive cycle.
- [ ] Initialize the exact scanner-selected RIO registered-buffer/completion-queue adapter when fully available; otherwise initialize the scanner-selected overlapped/IOCP adapter.
- [ ] Retain both RTP and RTCP sockets, reusable buffers, completion handles, and cancellation handles with RAII.
- [ ] Validate completion byte counts, channel identity, cancellation, and no post-start allocation; initialization failure is terminal and cannot choose another adapter.
- [ ] Run GREEN with real sockets, cancellation during a blocked receive, and exact resource cleanup; delete probe source/target/binaries.
- [ ] Commit and push the Windows adapter.

## Task 6: Implement Linux/RK batch receive from proven capabilities

**Files:**

- Create: `src/internal/graph/protocol/rtp/ingress/linux/MediaLinuxRtpIngressCapabilityScanner.cpp`
- Create: `src/internal/graph/protocol/rtp/ingress/linux/MediaLinuxRtpIngressAdapter.h`
- Create: `src/internal/graph/protocol/rtp/ingress/linux/MediaLinuxRtpIngressAdapter.cpp`
- Modify: `src/internal/graph/runtime/network/MediaSocketRuntime.cpp`
- Modify: `src/internal/graph/runtime/network/MediaUdpSocket.h`
- Modify: `src/internal/graph/runtime/network/MediaUdpSocket.cpp`
- Modify: `CMakeLists.txt`

- [ ] Create a temporary Linux RED probe on the RK target using retained real UDP sockets and a multi-datagram burst.
- [ ] Implement `recvmmsg` over reusable planned `mmsghdr`/`iovec` storage as the baseline proven Linux batch adapter.
- [ ] If Task 1 proves every zero-copy prerequisite, implement the multishot registered-memory `io_uring` adapter and its ownership/completion path; otherwise preserve the exact scanner rejection reason and do not compile or select a fictitious path.
- [ ] Integrate cancellation without periodic polling defaults; stop/abort must wake a blocked receive and release all platform resources.
- [ ] Run GREEN on the target under `ffenv on`, delete temporary target/source/artifacts, and confirm no test process remains.
- [ ] Commit and push the Linux/RK adapter.

## Task 7: Bind the plan through the production DAG and consume batches

**Files:**

- Modify: `src/internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h`
- Modify: `src/internal/graph/planner/realtime/MediaRawRtpInputPreparer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.cpp`
- Modify: `src/internal/graph/nodes/input/RawRtpInputNode.h`
- Modify: `src/internal/graph/nodes/input/RawRtpInputNode.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `src/internal/graph/runtime/validation/MediaRealtimeVideoGraphShapeValidator.cpp`

- [ ] Create a temporary RED graph probe showing the old one-datagram receiver interface and missing exact plan serialization.
- [ ] Serialize every `MediaRtpIngressPlan` fact into the raw RTP input node plan and decode it exactly; reject missing, unknown, clamped, or inconsistent fields.
- [ ] Transfer the same preflight sockets and finalized storage into `MediaRtpIngressReceiver`; never rebind or rebuild capacity in the runtime node.
- [ ] Replace the node's one-datagram receive with one bounded batch receive per process call.
- [ ] Feed every batch entry through the existing shared RTP/RTCP parse, identity, reorder, depacketizer, clock, lineage, and output routines. Do not add a second media path.
- [ ] Preserve DAG backpressure: stop batch consumption when pending output exists, and resume the retained batch without copying or reordering.
- [ ] Run GREEN comparing existing parser/depacketizer outputs and terminal errors, then delete all temporary TDD files and artifacts.
- [ ] Commit and push the DAG integration.

## Task 8: Add industrial ingress telemetry and acceptance evidence

**Files:**

- Create: `src/internal/graph/runtime/diagnostics/MediaRtpIngressMetrics.h`
- Create: `src/internal/graph/runtime/diagnostics/MediaRtpIngressDiagnostics.h`
- Create: `src/internal/graph/runtime/diagnostics/MediaRtpIngressDiagnostics.cpp`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.cpp`
- Modify: `src/internal/graph/runtime/diagnostics/MediaRuntimeAcceptanceCollector.cpp`
- Modify: `src/internal/graph/nodes/input/RawRtpInputNode.cpp`
- Modify: `QUALITY_SCORE.md`
- Modify: `CMakeLists.txt`

- [ ] Report planned adapter and capacities, actual socket capacity, receive calls/completions, datagrams/bytes per batch, storage reuse, allocation after startup, truncation/pressure, reorder facts, worker process calls/CPU, memory, A/V drift, and exit reason.
- [ ] Keep process machine CPU and per-worker thread CPU semantically separate; never convert one into the other or hide unavailable evidence.
- [ ] Run the documented VS2026 clean-first all-target Debug build under the 120-second limit.
- [ ] Run the unchanged Windows real chain with absolute commands and no fixed launch delay: 128-second 2560x1440 HEVC plus AAC separate RTP input with automatic video fmtp, H.264 1280x720 30 fps output, AAC packet copy, and MPEG-TS over RTP output to visible VLC.
- [ ] Record exact commands, PIDs, ports, codec/size/fps, adapter, calls, completions, allocation, CPU, RSS, A/V drift, output readability, human-eye result, and terminal reason.
- [ ] Archive the committed branch to the exact RK path only after `pwd -P` matches `/home/firefly/Downloads/MediaTranscode`; preserve sibling media files.
- [ ] Under `ffenv on`, rebuild all targets with eight workers and run the same non-lowered 2K-to-720p codec-changing A/V chain. Use an RK-side temporary launch script only if needed; record content and PIDs, then delete it and verify ports/process residue.
- [ ] Do not claim acceptance if either platform has corruption, stutter, drift, drops, unexpected allocation, runtime fallback, worker error, abnormal exit, or unaccounted CPU.
- [ ] Commit and push diagnostics, acceptance report, and concise score update.

## Task 9: Freeze, cross-review, and deliver

- [ ] Delete every task-specific temporary TDD source, CMake target, executable, object, PDB, ILK, script, log, and remote process; do not delete unrelated user artifacts.
- [ ] Verify `git diff --check`, UTF-8 CRLF, no duplicate semantics, no fixed/sample-tuned RTP constants, no platform media-chain fork, RAII closure, exact plan codec, bounded bytes/items, and clean cancellation.
- [ ] Freeze the diff and assign two fresh independent agents that did not implement it. Each reviews planner authority, DAG reuse, ownership, batching/backpressure, memory bounds, error paths, telemetry truth, Windows/RK isolation, and real acceptance evidence.
- [ ] If either reviewer does not explicitly approve, fix the findings, repeat relevant build/real-media verification, freeze again, and have both agents re-review until both explicitly approve.
- [ ] Update `QUALITY_SCORE.md` and completion/risk documentation with the two review conclusions.
- [ ] Commit all intended production files, push `codex/rkmpp-zero-copy`, create the PR, and request final independent PR review.

## Remaining risk gates

- A target may not expose RIO or `io_uring` zero-copy RX; this is acceptable only when the scanner records authoritative unavailability and the planner selects another already-proven industrial batch capability before DAG construction.
- Bare RTP still cannot prove authoritative end-of-stream without RTCP BYE or an external session contract; playback acceptance and termination evidence must be reported separately.
- Kernel acceptance of a UDP datagram is not wire-transmit evidence; ingress completion telemetry must not be described as network delivery proof.
