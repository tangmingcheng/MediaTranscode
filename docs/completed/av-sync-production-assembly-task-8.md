# A/V Sync Production Assembly Task 8 Completion

Task 8 connects the shared A/V scheduler and typed router to two production
scheduled RTP senders and one transactional dual-media SDP publisher.

## Result

- The planner carries a complete separate-RTP output and SDP identity plan.
  Runtime codec metadata supplies only validated codec configuration.
- The compiler injects the exact registered A/V sync-group runtime. Ordinary
  factory creation of scheduled sender nodes fails closed.
- Video and audio sender nodes own their RTP/RTCP transport, packetizer,
  counters, sender reports, rollback-safe open transaction, and lifecycle.
- The Task 7 scheduler remains the sole pacing authority. Canonical AUs pass
  through its typed router outputs before reaching either sender.
- The publisher accepts exactly one opened description per stream and replaces
  one dual-media SDP through write, flush, and atomic replace phases.
- Publisher flush clears partial, completed, and pending generation state so a
  later generation cannot inherit stale descriptions.
- Explicit H264 Annex-B packetization normalizes avcC codec configuration once
  for FFmpeg and strictly rejects non-Annex-B access units without fallback.

Implementation commits:

- `d142558` `feat: send scheduled separate rtp output`
- `8ebfa7d` `fix: harden scheduled rtp lifecycle`
- `9ffa1ac` `fix: exercise scheduled rtp pacing path`

## Verification

Commands were executed from the repository root in PowerShell. Build commands
are omitted.

```powershell
ctest --test-dir out/build/x64-debug -R "^media_transcode_node_tests$" --output-on-failure
ctest --test-dir out/build/x64-debug -R "^media_transcode_scheduled_rtp_output_decode_integration_tests$" --output-on-failure -V
ctest --test-dir out/build/x64-debug -R "^(media_transcode_planner_tests|media_transcode_builder_tests|media_transcode_runtime_tests|media_transcode_node_tests|media_transcode_rtp_sdp_description_tests|media_transcode_scheduled_rtp_output_node_tests|media_transcode_rtp_udp_sender_loopback_tests|media_transcode_rtp_udp_sender_ipv6_loopback_tests|media_transcode_scheduled_rtp_output_decode_integration_tests|media_transcode_integration_tests)$" --output-on-failure
```

Results:

- The final clean verification removed 516 files and completed the 517-step
  configured DAG successfully.
- The final ten-target matrix passed 10/10 with zero failures in 153.68
  seconds. Full realtime integration passed in 97.97 seconds.
- The receiver-first generated-SDP test passed in 5.90 seconds. FFmpeg bound
  all planned RTP/RTCP ports before submission, and both video and audio
  `framemd5` outputs contained decoded frames.
- Focused scheduler/output and decode tests passed 2/2 in 14.76 seconds after
  final formatting.
- Static checks found no direct scheduled sender source, manual scheduled AU,
  harness-owned dispatch sleep, dispatch-offset pacing, or unrelated scheduler
  sink.
- All Task 8 text files are UTF-8 without BOM and use CRLF line endings.

Two earlier clean attempts had silent, non-repeatable `cl.exe` and `link.exe`
exit-code 1 failures at different existing targets with no compiler, linker,
WER, Defender, CodeIntegrity, disk, or resource-exhaustion diagnostic. The same
command hashes later succeeded, targeted link and core tests passed, and the
final clean verification completed in one invocation. This remains an external
Windows tool-execution risk rather than a reproduced source failure.

## Review

The initial independent review found missing publisher generation reset and a
decode skip classification that could hide a missing generated SDP. The first
re-review found a remaining generic `Unsupported` skip and a test-only scheduler
bypass. Both were removed and the full verification was repeated.

The final independent review passed specification and code quality with
Critical, Important, and Minor findings at `0/0/0`.

## Remaining Scope

- Task 9 must connect the same scheduler/router contract to project-owned
  MPEG-TS output.
- Task 10 must switch the whole production graph to the new RTP and MPEG-TS
  output segments and remove the synchronized legacy authorities.
- The receiver readiness probe is Windows-specific. Long-duration loss,
  pressure, CPU, drift, and VLC playback remain final acceptance work.
