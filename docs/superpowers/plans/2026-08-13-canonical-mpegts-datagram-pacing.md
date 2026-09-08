# Canonical MPEG-TS Datagram Emission Plan

## Goal

Replace fixed-rate/burst MPEG-TS pacing with a planner-constrained,
access-unit-aware canonical emission schedule shared by Windows, Linux, UDP,
and MP2T/RTP.

## Constraints

- Work only on `codex/rkmpp-zero-copy`.
- The planner owns topology, timing, and protocol contracts; downstream code
  fails on missing or contradictory facts and never falls back.
- Do not add sample-derived rate, headroom, burst, or lateness constants.
- Preserve one canonical scheduler, RAII transactions, bounded cursor
  ownership, lineage, clock commits, and nonblocking stop/abort.
- Do not lower any source or acceptance parameter.
- Temporary TDD is deleted completely after RED/GREEN.
- Text is UTF-8 without BOM and CRLF.

## Implementation

- [x] Make `MediaTsDatagramEmissionPlan` a deterministic projection of the
  planner-owned mux plan: canonical access-unit window plus datagram geometry.
- [x] Remove duplicated serialized emission tuning fields; decode the same
  typed projection and require exact equality in graph/runtime validation.
- [x] Materialize the exact TS cursor before beginning access-unit emission.
- [x] Derive each stream's observed rate from committed wire bytes and
  canonical emit cadence; combine active stream and last other-stream facts.
- [x] Raise the selected rate as required to finish inside the remaining
  `emitOnMaster` to `dispatchOnMaster` window.
- [x] Prepare fixed per-datagram canonical deadlines with checked integer
  arithmetic and commit state only after successful sink writes.
- [x] Keep PAT/PMT/PCR on their canonical deadlines without a second pacing
  authority; keep UDP/RTP sinks transport-only.
- [x] Add real current/maximum scheduling debt, selected rate, access-unit,
  pressure, wait, lateness, pending-byte, and exit-reason telemetry.
- [x] Prove large-IDR and following-frame window behavior with temporary RED
  and GREEN TDD, then remove its source, CMake target, objects, and binaries.
- [x] Run the repository VS2026 Debug clean-first all-target build within 120s
  (500 build steps, both CLIs linked, 78.3s, exit code 0).
- [x] Independently review planner authority, arithmetic, RAII, bounded state,
  shared-platform behavior, telemetry, and repository hygiene; all findings
  were fixed and the final independent review was approved.
- [ ] Commit and push the branch, deploy its exact archive to RK, then build
  under `ffenv on` with eight jobs.
- [ ] Run equal-spec Windows and RK real-media acceptance: unchanged 2K
  120-second source, HEVC to H.264, 2560x1440 to 1280x720, AAC, separate RTP
  input with automatic fmtp, and MP2T/RTP output. Record exact commands, PIDs,
  receiver sequence loss/reorder, VLC result, CPU/RSS, queues, drops, A/V drift,
  pacing diagnostics, exit reason, and residue checks.

## Pass Conditions

- No fixed/sample-specific MPEG-TS pacing policy or downstream fallback.
- Every access unit completes within its planner-owned canonical window, or
  fails explicitly when the physical transport cannot meet that contract.
- Scheduling debt and transport pressure are measurable rather than hidden.
- Windows and RK use the same scheduling/mux logic; only platform socket and
  hardware APIs differ.
- No temporary TDD, script, process, port, or generated validation artifact is
  committed or left running.
