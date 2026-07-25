# Task 10.2C - Explicit UDP sender transport

## Objective

Provide the UDP transport boundary consumed by the Task 10.2B RTP and RTCP sinks. The transport owns two independent sender ports and exposes exact delivery certainty without retrying, queueing, sleeping, probing ports, parsing URLs, resolving DNS, or making planner decisions.

## Owned scope

- `runtime/network/MediaUdpDatagramSenderPort`: sender-only port interface and typed send outcome.
- `runtime/network/MediaUdpDatagramSenderSocket`: Windows Winsock RAII implementation using an explicit shared `MediaSocketRuntime` lifetime.
- `protocol/rtp/MediaRtpUdpSenderConfig`: complete validated RTP/RTCP endpoint and socket policy.
- `protocol/rtp/MediaRtpUdpSenderTransport`: two-port lifecycle, routing, rollback, and certainty propagation.
- Deterministic fake-port tests, a real Windows IPv4 loopback integration test, and Task 10.2B sink composition tests.

## Non-owned scope

SDP and codec metadata, CNAME publication, planner/request/node registration, A/V composition, shared epoch, pacing, BYE, legacy removal, receive-side `MediaUdpSocket`/`MediaRtpUdpTransport`, and production graph wiring remain outside Task 10.2C.

## Contracts

### Sender port

- `MediaUdpDatagramSendOutcome` is non-default-constructible and has exactly three kinds: `Accepted`, `NotAccepted`, and `AmbiguousPartial`.
- `Accepted` means the complete datagram was accepted by the local kernel in one `sendto` call; it does not claim remote delivery.
- `NotAccepted` means zero bytes were accepted and retry is side-effect-safe, but the cause is not necessarily transient. It carries an `ErrorInfo`; transport returns its `Status` unchanged.
- `AmbiguousPartial` means the OS reported a positive short send and carries an error; transport throws because an external partial side effect cannot be represented as retryable failure.
- Port `open`, `send`, `localEndpoint`, `remoteEndpoint`, and idempotent `close` are sender-only operations. The interface does not expose receive or event APIs. A factory creates exactly two unopened ports; only the transport projects the public plan into the two internal port-open requests.
- The Winsock implementation uses `socket`, explicit `SO_SNDBUF`, explicit `NonBlockingRejectOnPressure` selection, `bind`, `getsockname`, and `sendto`. It does not use `WSAEventSelect`. Blocking mode is not part of the stable surface.
- `SOCKET_ERROR` and a zero result for non-empty input are `NotAccepted`; an exact-size result is `Accepted`; a positive short result is `AmbiguousPartial`.

### Configuration

- `MediaRtpUdpSenderConfig` and its endpoint/local-policy value types are non-default-constructible and created only by validating factories.
- Required inputs are explicit: one address family; numeric local and remote addresses in that family; an even remote RTP port and its adjacent `+1` RTCP port; either an even fixed local RTP port and its adjacent `+1` RTCP port or the sole `OsAssignedIndependent` policy; positive send-buffer bytes; one positive pair-wide maximum datagram size; and the sole stable `NonBlockingRejectOnPressure` behavior.
- Numeric addresses are validated for the selected family. URL syntax, host names, wildcard family inference, port inference, default ports, and fallback are rejected.
- Fixed local ports preserve the requested pair exactly. OS-assigned ports bind each sender independently to port zero and publish the actual bound endpoints.

### Transport

- `MediaRtpUdpSenderTransport` owns the RTP and RTCP ports plus the immutable validated config. Its lifecycle is `Created`, `Open`, `Closed`, or `Poisoned`.
- `open` opens RTP first and RTCP second. A returned failure or thrown exception from either port is caught, both ports are closed without throwing, and the transport permanently stores a structured poison cause. Duplicate open is ordinary API misuse and does not replace the terminal cause.
- `sendRtp` and `sendRtcp` route only to their dedicated ports. Empty/oversized datagrams, non-open state, and reentry are rejected before the port call.
- `NotAccepted` returns a side-effect-safe `Status` and leaves the transport open. `AmbiguousPartial` or an unknown exception from `send` first closes both ports and stores a sticky poison cause, then throws a typed ambiguous-delivery exception to the Task 10.2B sink boundary. There is no internal retry.
- One mutex protects lifecycle and operation admission. Concurrent or reentrant `open`/send attempts fail before a port call. `Status close() noexcept` waits across threads for the active send; same-thread close and reentrant `try_lock` failure return `InvalidArgument` without changing state or sockets. Destruction calls `close()` and ignores its status.
- `close` is idempotent, closes both ports, and preserves `Poisoned` rather than changing it to `Closed`. Destruction closes both ports through RAII.
- Bound local endpoints are atomically published only after both ports are open and `getsockname` has verified them. Fixed endpoints must match exactly; OS-assigned endpoints must have nonzero, distinct ports. Getters lock and return values by copy. Configured remote endpoints are immutable and observable by value. A factory success containing a null port, duplicate bound endpoints, or an endpoint inconsistent with the plan is rejected at the transport boundary.

## TDD and verification

1. RED: deterministic tests define config rejection, exactly-two factory creation, ordered open/rollback (returned and thrown failures), atomic endpoint publication, lifecycle/reentry/concurrent-send admission, cross-thread close waiting, routing, pre-syscall validation, delivery-certainty mapping, sticky poison, and Task 10.2B transaction/counter behavior.
2. GREEN: implement the smallest port/config/transport surface satisfying the tests.
3. Task 10.2B composition tests assert: RTCP `NotAccepted` leaves the deadline unchanged, retry uses the same scheduled deadline plus its new `reportInstant` and current counters, and commits once; RTCP partial and throw each create sticky poison and no second port syscall; FU-A mid-datagram `NotAccepted` counts only accepted datagrams, leaves transport `Open`, and poisons the sender; sink captures cannot outlive the transport; sender guard reentry makes no second port call.
4. RED/GREEN: Windows IPv4 loopback verifies distinct RTP/RTCP sockets and payload routing. An unavailable socket runtime exits with skip code 77 and an explicit unsupported diagnostic; it is never reported as pass.
5. Run focused node and loopback targets. Root owns the final clean-first, deterministic/integration suite, review, commit, and push.
