# Industrial Raw RTP Batched Ingress Design

## Problem and Evidence

The shared `RawRtpInputNode` currently performs one wait, allocation, socket
receive, parse pass, and DAG `process()` cycle per UDP datagram. The unchanged
2560x1440 HEVC, AAC, 128-second acceptance chain produced 151,122 video input
process calls and consumed 16.52 seconds of video-input worker CPU. The current
planner also supplies fixed receive-buffer, maximum-datagram, reorder-window,
and reorder-delay values instead of authoritative transport facts.

The correction must follow a production network-ingress architecture without
creating codec-, platform-, or acceptance-specific media chains. Windows and
RKMPP share one DAG node, RTP parser, reorder, depacketizer, clock model, buffer
contract, and diagnostics. Only the operating-system receive adapter varies.

## Capability and Planner Authority

Preflight probes typed receive capabilities before graph construction:

- Windows registered or completion-queue receive with reusable storage;
- Linux multi-message receive and, only when kernel, driver, and memory-region
  capabilities prove it, multishot zero-copy receive;
- exact socket receive capacity and cancellation mechanism;
- maximum datagram bytes actually observed during the existing bounded RTP
  signaling/source-rate preflight.

The capability enum has four explicit products:
`WindowsRegisteredIo`, `WindowsOverlappedCompletionQueue`,
`LinuxIoUringZeroCopy`, and `LinuxReceiveMultipleMessages`. Capability scanning
records why each product is available or unavailable. The planner selects one
complete product before building the DAG; an adapter initialization failure is
terminal and cannot select the next enum value at runtime.

The planner selects one supported capability and emits a
`MediaRtpIngressPlan`. This product contains the selected adapter kind, exact
socket receive capacity, observed maximum datagram bytes, user-space batch byte
capacity, descriptor capacity, storage ownership kind, cancellation contract,
and reorder contract. Batch capacities are checked arithmetic products derived
from the probed socket capacity, prepared-input byte budget, and observed
datagram geometry. Runtime neither chooses an adapter nor resizes, clamps, or
falls back.

The existing fixed RTP transport and reorder constants are removed. Automatic
preflight records observed datagram geometry, maximum sequence displacement,
arrival-time variation, and packet-rate evidence. Manual/no-I/O mode instead
requires the corresponding sender `pkt_size` and deployment reorder-latency
SLA as direct facts; callers never enter a derived packet count, batch size, or
memory capacity. The planner derives descriptor, storage, and reorder capacity
with checked arithmetic. Protocol limits may be represented as named standards
facts, but cannot be used as sample-tuned runtime policy. If neither observation
nor direct deployment facts are sufficient, planning fails before DAG startup.

The selected adapter opens the RTP/RTCP sockets during preflight and the exact
same sockets are retained by the prepared-input binding. A bootstrap receive
may use the named UDP protocol payload limit solely to discover the first exact
datagram geometry. Before the DAG starts, the adapter replaces bootstrap
storage with the final planner-owned registered storage; runtime never rebinds
the ports or changes the storage contract.

## Deep Ingress Module

`MediaRtpIngressReceiver` is a deep module behind one small interface: receive
the next bounded batch described by `MediaRtpIngressPlan`, or return typed
`WouldBlock`, cancellation, or failure. It owns reusable receive storage and
all platform handles through RAII.

The Windows adapter posts receives into registered or overlapped reusable
storage and consumes completion-queue results. The Linux adapter receives
multiple messages into the same planned storage; a zero-copy adapter is valid
only when authoritative capability probing proves its required kernel and
driver support. Both adapters return the same `MediaRtpIngressBatch` entries:
channel, byte view or lease, exact byte count, and monotonic observation time.

One batch is bounded simultaneously by its planned byte capacity and descriptor
capacity. RTP and RTCP readiness are serviced without assuming an ordering
between two UDP sockets. No per-datagram heap allocation is permitted on the
steady-state receive path after storage initialization.

## DAG Node Behaviour

`RawRtpInputNode` consumes one typed ingress batch per `process()` invocation
and runs every entry through the existing shared parser, identity validation,
reorder buffer, depacketizer, RTCP tracker, and clock-transition logic. It emits
the same packet and event buffer types on the same ports. Multiple complete
access units produced by a batch remain in the node's existing bounded pending
output state and are emitted under normal DAG backpressure.

The node returns when it has an observable output, the batch is exhausted,
the receiver reports `WouldBlock`, a reorder or clock deadline is due, or stop
or abort cancels the receive. It never invents a packet-count, duration, codec,
resolution, or platform threshold. The input worker remains dedicated, while
all output remains subject to the existing bounded DAG edges.

## Diagnostics and Failure Semantics

Runtime diagnostics report selected ingress adapter, socket and batch byte
capacity, descriptor capacity, datagrams and bytes per receive operation,
receive system calls or completions, storage reuse, allocation after startup,
truncation, pressure, process calls, worker CPU, and final exit reason.

Unexpectedly larger datagrams, truncated messages, capacity overflow, adapter
contract mismatch, cancellation failure, socket pressure, reorder overflow,
and source-clock failure are terminal. No adapter downgrade, heap-growth rescue,
packet drop, or software media fallback is allowed after graph startup.

## Verification

Temporary TDD first proves exact plan serialization, capability rejection,
bounded batch ownership, cancellation, no steady-state allocation, multi-entry
ordering, and unchanged parser/depacketizer output. Every temporary test source,
target, and artifact is deleted after red/green verification.

Production acceptance uses the unchanged 128-second 2560x1440 HEVC plus AAC
source, separate RTP input with automatic video fmtp, H.264 1280x720 30 fps
output, AAC packet copy, and MPEG-TS over RTP output. Windows and RKMPP must
select their probed platform adapter while sharing the same DAG topology and
node logic. Acceptance requires zero dropped buffers, no playback-stage worker
errors, stable memory and A/V drift, no visual corruption or stutter, and lower
input process-call, allocation, syscall/completion, and worker-CPU evidence.
Final delivery requires the prescribed VS2026 clean-first all-target build,
`ffenv on` RK build with eight compile workers, process/script/port cleanup, and
two independent review agents approving the frozen implementation.

## Industrial Reference Baseline

- Microsoft Winsock guidance identifies IOCP/overlapped I/O as its highest
  performance general server model, while Registered I/O provides registered
  buffers and completion queues for lower-latency high-speed networking.
- Linux `recvmmsg` receives multiple messages in one system call. Linux
  `io_uring` multishot zero-copy RX is selected only when the target kernel,
  network device, and memory-region registration prove support.

These references define adapter capability, not policy defaults. Actual target
probing and planner products remain authoritative.
