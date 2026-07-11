# Task 2 Report: Planner-Owned A/V Synchronization Contract

## Result

Implemented one complete `MediaAvSyncPlan` contract for the two synchronized topologies:

- separate RTP audio/video input to separate RTP audio/video output;
- MPEG-TS UDP input to MPEG-TS output.

Separate RTP input to MPEG-TS output and URL/RTSP A/V synchronization are rejected before graph construction. The realtime planner embeds the validated plan and remains the orchestration owner.

## TDD Evidence

### RED

Command:

```text
cmd /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --target media_transcode_planner_tests"
```

Expected failure:

```text
test_planner.cpp(9): fatal error C1083: cannot open include file: internal/graph/planner/avsync/MediaAvSyncPlanValidator.h
```

The failure proved that the new planner contract did not exist before production implementation.

### GREEN

Focused planner command:

```text
out\build\x64-debug\media_transcode_planner_tests.exe
```

Result: exit code 0. Tests cover both supported topologies, explicit RTP-to-TS rejection, all required-field omissions, ordered thresholds, RTP identities/clock rates/CNAME/SR policy, TS program/PID/PCR policy, and the 1000/5000 ppm limits.

Integration command:

```text
ctest --test-dir out\build\x64-debug -C Debug -R media_transcode_integration_tests --output-on-failure
```

Result: 1/1 passed in 72.32 seconds. The realtime plan contains a validator-approved RTP synchronization contract, and URL/RTSP A/V is rejected because it is outside the two approved synchronized topologies.

## Clean Rebuild Evidence

Command:

```text
cmd /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --target clean && cmake --build out\build\x64-debug"
```

Result: passed; clean removed 212 outputs and the rebuild completed 213/213 steps, including the integration target.

Deterministic suite command:

```text
ctest --test-dir out\build\x64-debug -C Debug --output-on-failure
```

Result before the final topology expectation correction: deterministic tests 1-6 all passed; the integration test exposed one obsolete URL/RTSP A/V success expectation. After correcting that expectation to the approved support matrix, the focused integration suite passed 1/1. No production behavior was loosened to satisfy the test.

## Files

- `src/internal/graph/planner/avsync/MediaAvSyncPlan.h`
- `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.h`
- `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- `src/internal/graph/planner/avsync/MediaAvSyncPlanner.h`
- `src/internal/graph/planner/avsync/MediaAvSyncPlanner.cpp`
- `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- `src/internal/graph/planner/realtime/MediaRealtimeRequestValidator.cpp`
- `tests/unit/test_planner.cpp`
- `tests/unit/test_realtime_rtp_graph.cpp`

## Self-Review

- Exactly two synchronized topologies are accepted.
- Separate RTP to MPEG-TS is explicitly rejected.
- Required values have no value-producing default member initializers. Missing or inconsistent values fail validation.
- Every duration and threshold uses the strong `MediaRunningTime` nanosecond type; raw tick values are not stored in the contract.
- RTP input identities, payloads, clock rates, common-CNAME/SR requirements and timeouts are explicit.
- RTP output identities, SSRCs, base timestamps, common non-sensitive CNAME, clock rates, SR interval and shared NTP epoch policy are explicit.
- TS program, PMT/video/audio/PCR PIDs, PCR interval/gap/jitter and PTS/DTS time base are explicit.
- Startup, audio servo, video recovery, discontinuity, reacquisition, metrics and acceptance thresholds are explicit and ordered.
- Normal correction is capped at 1000 ppm and recovery correction at 5000 ppm.
- Request validation, existing topology classification, synchronization policy construction and plan validation remain separate responsibilities.
- Realtime orchestration calls the specialized planner and embeds only a validated contract; builders and nodes do not infer new synchronization values.
- Enum values are explicit and stable.

## Sequencing Concern

The pre-existing startup-barrier and per-mux pacing plan fields remain physically unchanged as a temporary build seam because their replacement runtime components are not implemented in Task 2. No adapter, compatibility branch, new fallback, or expanded legacy population was added. Task 12 must remove those legacy fields and the old execution path when the graph-level coordinator and shared scheduler consume `MediaAvSyncPlan`.

## Review Fix Evidence

### RED

`cmake --build out\build\x64-debug --target media_transcode_integration_tests` failed at `test_realtime_rtp_graph.cpp:1453` because `MediaAvSyncPlan` had no `has_value()`. This proved a video-only plan could not represent synchronization as absent.

`cmake --build out\build\x64-debug --target media_transcode_planner_tests && out\build\x64-debug\media_transcode_planner_tests.exe` failed named mutations for program-number overflow, reserved/null TS PIDs, startup skew ordering, audio slew ordering, RTP SR timeout/extrapolation ordering, and TS PCR jitter/interval/gap ordering.

After adding the requested RTP SR skew test, the planner test build failed because `MediaAvSyncRtpInputPolicy::maximumSenderReportSkewNs` did not exist. A second RED run failed the named audio filter/control/estimator and RTP SR interval/timeout comparisons.

### GREEN

The realtime plan now stores `std::optional<MediaAvSyncPlan>`: video-only paths leave it absent, while synchronized A/V paths contain the specialized planner's fully validated result. The validator now enforces valid program/PID ranges and isolated ordering invariants for startup, servo, video, recovery, metrics, RTP SR, and TS PCR policies.

Final verification: the post-review clean rebuild completed 213/213 steps; `ctest -R media_transcode_planner_tests` passed 1/1 in 0.08 seconds; `ctest -R media_transcode_integration_tests` passed 1/1 in 76.63 seconds.
