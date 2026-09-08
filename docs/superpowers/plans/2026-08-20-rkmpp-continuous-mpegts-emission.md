# RKMPP Continuous MPEG-TS Emission Plan

## Scope

Only fix the verified MPEG-TS/RTP burst that causes receiver loss, black frames,
stutter, and corruption. Preserve the existing DAG topology, timestamps, A/V
coordination, codec paths, and platform adapters.

## Implementation

1. Add a planner-owned continuous emission contract derived from resolved output
   bitrate, frame cadence, encoder buffer, mux packet geometry, and existing
   maintenance cadence. Reject incomplete or non-representable facts before DAG
   construction.
2. Replace per-access-unit rate escalation with one rational byte timeline that
   is continuous across access units and maintenance datagrams. Access-unit
   boundaries never reset service credit.
3. Make the scheduled sender forward-only: preserve byte spacing after wakeup
   delay and never catch up by submitting consecutive overdue datagrams. A
   non-immediate or ambiguous submit and any planner-bound violation terminate
   truthfully without fallback.
4. Keep PTS, DTS, PCR, RTP timestamps, and the existing mux order unchanged.
   Windows and RK share the planner and schedule; only their existing wait/socket
   adapters remain platform-specific.

## TDD and Verification

- Use temporary RED/GREEN probes for fixed-rate planning, cross-AU continuity,
  rational remainder, and forward-only late scheduling. Delete all probes and
  generated test artifacts after GREEN.
- Run the strict VS2026 full rebuild through the repository skill.
- Sync the committed branch to 192.168.130.229, run `ffenv on; mtenv on`, and
  build Release with eight jobs.
- Validate the unchanged 120-second source with sender/receiver pcaps and VLC.
  Require no RTP gaps in the common sequence range and no visible black frame,
  stutter, or corruption. Then run the existing same-spec A/V drift regression.
- Freeze the diff and require two new independent reviewers to approve it.
