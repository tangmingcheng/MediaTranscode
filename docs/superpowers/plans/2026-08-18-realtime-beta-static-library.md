# Realtime Beta Static Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a temporary static C API, callable from C and C++, for one automatically probed VideoOnly RTP input transcoded through the existing production DAG to MPEG-TS over RTP, without exposing graph resource knobs, FMTP, SDP paths, or hardware-backend selection.

**Architecture:** The public Beta layer deep-copies direct caller facts and maps them plus one isolated fixed Beta profile into the existing typed realtime request. A new DAG-external application controller owns the orchestration currently embedded in the realtime CLI; both the CLI and the Beta session call that controller, so planner, builder, graph, nodes, prepared-input handoff, codec lifecycle, output path, and completion semantics remain single-source. The Beta session contributes only asynchronous ownership, callback serialization, POD snapshot projection, temporary output-description-file RAII, and a C exception boundary.

**Tech Stack:** C11-compatible public header, C++20 implementation, FFmpeg, existing MediaTranscode planner/builder/DAG/runtime, CMake/Ninja, Visual Studio 2026, Linux aarch64/RKMPP.

**Spec:** `docs/superpowers/specs/2026-08-18-realtime-beta-static-library-design.md`

## Global Constraints

- Work only on `codex/rkmpp-zero-copy`; never edit or commit on `master`.
- Production graph code under `src/internal/graph` remains unchanged by this Beta wrapper. `MediaRealtimeVideoSession` or any Beta type must never appear inside the DAG.
- Support exactly one VideoOnly RTP input socket, automatic H.264/HEVC parameter-set discovery, H.264/HEVC frame transcoding, CBR/VBR, requested resolution/frame-rate conversion, and MPEG-TS over RTP output.
- Do not expose audio, FMTP, SDP paths, generic URLs, socket handles, queue capacities, startup byte capacities, timeouts, packet size, low-latency policy, or hardware selection.
- Linux selects typed `MediaHardwareBackendRequest::RKMPP`; Windows selects typed `MediaHardwareBackendRequest::Auto`. There is no runtime fallback added by the wrapper.
- The user-approved Beta constants may exist only in `MediaRealtimeBetaFixedProfile`; no other source may repeat them. This exception is temporary and must not migrate into the formal SDK or graph planner.
- CBR consumes only its bitrate member. VBR consumes only target/min/max and validates `min <= target <= max`. Inactive union members never affect the mapped request.
- No `struct_size`, `api_version`, caller-computed string length, SDP/FMTP derivation, or library-owned string-free API is introduced.
- Every C++ exception is caught at the C ABI boundary. Every socket, runtime, prepared input, thread, string copy, and temporary output-description file has one RAII owner.
- `request_stop` and `get_snapshot` are safe from the serialized event callback; `release` from the callback fails by contract and is never implemented as a self-join.
- Temporary TDD sources, targets, objects, executables, PDB/ILK files, generated SDP files, and probe artifacts are deleted before each task commit. No tracked test infrastructure is added.
- All tracked text is UTF-8 without BOM and CRLF. Preserve the user's untracked FFmpeg headers and `out/`; remove only exact task-owned temporary artifacts.
- Each completed task is reviewed, committed, and pushed to the same branch. Final frozen code requires two new independent agents to report zero Critical and zero Important findings.
- Final builds and real-media acceptance use Release. Windows builds strictly follow `.agents/skills/building-with-vs2026/SKILL.md`; RK builds run after `ffenv on` with `--parallel 8`.
- Real acceptance may not reduce source resolution, bitrate, frame rate, duration, codec conversion, or any required standard to obtain a pass.

---

## File Structure

- `include/media_transcode_beta/realtime.h`: the complete C11-compatible public API.
- `src/media_transcode_beta/MediaRealtimeBetaOwnedConfig.{h,cpp}`: validation and deep copy of public caller facts.
- `src/media_transcode_beta/MediaRealtimeBetaFixedProfile.{h,cpp}`: the sole owner of temporary Beta constants and profile diagnostics.
- `src/media_transcode_beta/MediaRealtimeBetaRequestMapper.{h,cpp}`: pure mapping from owned public facts and fixed profile to the existing typed request/runtime policy.
- `src/application/realtime/MediaRealtimeVideoRunController.{h,cpp}`: shared DAG-external preflight/build/run/report/complete/reset orchestration.
- `src/media_transcode_beta/MediaRealtimeBetaTemporaryDescription.{h,cpp}`: unique temporary SDP artifact and exact-path RAII cleanup.
- `src/media_transcode_beta/MediaRealtimeBetaSession.{h,cpp}`: asynchronous session state, event thread, callback serialization, stop signal, and snapshot projection.
- `src/media_transcode_beta/MediaRealtimeBetaApi.cpp`: opaque C handle and exception-safe public function bridge.
- `tools/realtime_video_cli/main.cpp`: retains parsing/printing only and delegates execution to the shared controller.
- `CMakeLists.txt`: registers the application controller and `media_transcode_beta` static target without adding tests.
- `docs/realtime-beta-static-library.md`: concise header/link/lifetime/example guide.
- `QUALITY_SCORE.md`: records the accepted Beta boundary and remaining temporary-profile debt.

### Task 1: Public C Contract, Owned Configuration, and Fixed Profile

**Files:**
- Create: `include/media_transcode_beta/realtime.h`
- Create: `src/media_transcode_beta/MediaRealtimeBetaOwnedConfig.h`
- Create: `src/media_transcode_beta/MediaRealtimeBetaOwnedConfig.cpp`
- Create: `src/media_transcode_beta/MediaRealtimeBetaFixedProfile.h`
- Create: `src/media_transcode_beta/MediaRealtimeBetaFixedProfile.cpp`
- Create: `src/media_transcode_beta/MediaRealtimeBetaRequestMapper.h`
- Create: `src/media_transcode_beta/MediaRealtimeBetaRequestMapper.cpp`
- Modify: `CMakeLists.txt` only with a temporary RED/GREEN target during this task; remove it before commit.

**Interfaces:**
- Produces: the exact C ABI declared in the approved spec, `MediaRealtimeBetaOwnedConfig::create`, `MediaRealtimeBetaFixedProfile::current`, and `MediaRealtimeBetaRequestMapper::map`.
- Consumes: direct source/signaling/deployment facts supplied in `mt_beta_realtime_config` and existing `MediaRealtimeRtpTranscodeRequest` types.

- [ ] **Step 1: Write C and C++ temporary RED consumers**

Create `out/tdd/realtime-beta-header-c.c` and `out/tdd/realtime-beta-header-cpp.cpp`. Both include only `media_transcode_beta/realtime.h`, initialize H.264 input and HEVC/CBR output, install a callback, and call all four functions. The C++ consumer additionally uses `static_assert(std::is_standard_layout_v<...>)` for every public POD. Compile only; expected RED is a missing public header.

- [ ] **Step 2: Implement the exact public header**

Declare stable explicit enum values and these four functions:

```c
mt_beta_status mt_beta_realtime_start(
    const mt_beta_realtime_config* config,
    const mt_beta_realtime_callbacks* callbacks,
    mt_beta_realtime_session** session);
mt_beta_status mt_beta_realtime_request_stop(mt_beta_realtime_session* session);
mt_beta_status mt_beta_realtime_get_snapshot(
    mt_beta_realtime_session* session,
    mt_beta_realtime_snapshot* snapshot);
void mt_beta_realtime_release(mt_beta_realtime_session** session);
```

Use only `<stddef.h>` and `<stdint.h>`, guard `extern "C"` with `__cplusplus`, and expose the approved codec, rate-control, state, event, error, stage, completion, backend, filter, config, callback, event, and snapshot PODs. Do not add default-valued fields, `bool`, `size_t` compatibility sentinels, C++ types, platform handles, or allocation callbacks.

- [ ] **Step 3: Implement strict validation and deep copy**

`MediaRealtimeBetaOwnedConfig::create` validates before starting a thread:

- non-null config/callback/session output and `on_event`;
- non-null, non-empty `media_id`, bind address, and destination address;
- ports nonzero, RTP payload type no greater than the protocol maximum 127, and clock rate nonzero;
- input/output codec exactly H.264 or HEVC;
- width, height, frame-rate numerator/denominator, GOP, and active bitrate fields nonzero;
- CBR reads only `cbr.bitrate_bps` and ignores inactive VBR storage;
- VBR reads only target/min/max and rejects any violation of `min <= target <= max`;
- every conversion from `uint64_t` bps to the existing integer kbps representation is checked for divisibility/range according to the mapper contract and fails rather than truncating or saturating;
- addresses are syntactically valid numeric IPv4/IPv6 deployment facts; URL construction brackets IPv6 correctly.

Deep-copy all caller strings into owned `std::string` members before `start` returns. No asynchronous code retains caller configuration pointers.

- [ ] **Step 4: Isolate the approved fixed profile and request mapping**

Make `MediaRealtimeBetaFixedProfile` non-default-constructible and return one immutable product containing exactly the approved profile values. Its `diagnosticSummary()` lists the profile identity and every value. No other Beta file contains those numeric constants.

`MediaRealtimeBetaRequestMapper::map` creates one existing `MediaRealtimeRtpTranscodeRequest`:

```text
input.type                         RtpPort
input.streamLayout                 SeparateStreams
input.videoRtp                     rtp://<bind-address>:<port>, codec, PT, clock
input.videoRtp.fmtp                absent
parameters.execution.streamSet     VideoOnly
parameters.execution.disableHardware false
parameters.execution.hardwareBackend Windows=Auto, non-Windows=RKMPP
parameters.video                   output codec/size/fps/GOP/rate control
output.streamLayout                MuxedTransportStream
output.transport                   RtpAvp
output.host/basePort               caller destination facts
output.sdpPath                     session-owned temporary path supplied later
```

Copy queue/startup/probe/runtime fields from the fixed profile. CBR sets `MediaRateControlMode::Cbr` and only `bitrateKbps`; VBR sets `MediaRateControlMode::Vbr` and target/min/max. Do not set buffer size, quality, preset, tune, profile, level, B-frames, audio parameters, prepared-handoff overrides, or a maximum duration.

- [ ] **Step 5: Run GREEN, boundary probes, and exact cleanup**

Compile the C consumer with the platform C compiler and the C++ consumer with C++20. Add one temporary mapper executable that proves deep-copy independence, CBR inactive fields cannot leak, VBR ordering fails, IPv6 URL formatting is correct, and platform backend mapping is exact. Run it to exit 0, then remove all three sources, the temporary CMake stanza, and exact compiler/linker artifacts.

- [ ] **Step 6: Review, commit, and push**

Verify no Beta constant is duplicated outside `MediaRealtimeBetaFixedProfile`, no graph file changed, `git diff --check` is clean, and all modified text is UTF-8/CRLF. Commit and push:

```powershell
git add include/media_transcode_beta src/media_transcode_beta CMakeLists.txt
git commit -m "feat(beta): define realtime C interface profile"
git push origin codex/rkmpp-zero-copy
```

### Task 2: Extract the Shared DAG-External Realtime Run Controller

**Files:**
- Create: `src/application/realtime/MediaRealtimeVideoRunController.h`
- Create: `src/application/realtime/MediaRealtimeVideoRunController.cpp`
- Modify: `tools/realtime_video_cli/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `MediaRealtimeVideoRunPolicy`, `MediaRealtimeVideoRunControl`, `MediaRealtimeVideoRunObserver`, `MediaRealtimeVideoRunOutcome`, and `MediaRealtimeVideoRunController::run`.
- Consumes: the existing planner, graph builder, runtime, progress tracker, reporter, and completion helper without modifying any of them.

- [ ] **Step 1: Capture CLI parity as temporary RED evidence**

Create a temporary compile probe that includes `application/realtime/MediaRealtimeVideoRunController.h` and supplies a typed request, runtime policy, stop control, and observer. Expected RED is the missing controller. Separately record the current CLI's preflight/build/start/progress/completion sequence and exit mapping so the extraction has an exact parity checklist.

- [ ] **Step 2: Define application-layer products with no policy defaults**

Use a validated factory for `MediaRealtimeVideoRunPolicy`; its caller must provide progress timeout, first-output timeout, and poll interval. Maximum duration remains optional for CLI parity but the Beta mapper leaves it absent. `MediaRealtimeVideoRunControl` owns an atomic stop request plus a condition-variable wakeup used only to interrupt controller waits; it never chooses a completion reason or calls graph nodes directly.

The observer receives copied, immutable application facts:

```cpp
struct MediaRealtimeVideoRunObserver {
    std::function<void(const MediaRealtimeVideoPreparedReport&)> prepared;
    std::function<void(const MediaGraphRuntimeReport&)> progress;
};
```

`prepared` carries selected planner facts and the completed output description as typed strings/enums. `progress` receives the existing report value. No observer callback runs while a runtime, graph, queue, session, or controller mutex is held.

- [ ] **Step 3: Move orchestration, not media logic**

Implement `run` as the single owner of this existing sequence:

1. validate the run policy and observe a pre-existing stop request;
2. preflight the typed request;
3. retain selected planner facts and threading policy;
4. build the existing executable graph;
5. compile and register the existing runtime nodes;
6. start the existing threaded runtime;
7. poll synchronization/progress, sample acceptance, and emit copied reports; wait through `MediaRealtimeVideoRunControl` so a stop request wakes the poll immediately;
8. on caller stop, progress failure, source completion, or worker failure, call `MediaRealtimeRuntimeCompletion::complete` exactly once;
9. capture the final report before reset;
10. reset through RAII on every exit and return the first truthful failure with its stage.

Do not move CLI parsing, console formatting, Beta callback types, fixed profile values, temporary-file policy, or public error enums into the controller. Do not add a second planner, runtime wrapper inside `src/internal/graph`, alternate node registration, or fallback.

- [ ] **Step 4: Convert the CLI into a controller client**

Keep all existing CLI arguments and summaries. Replace only the orchestration body and `waitForRealtimeProgress` with a controller call. CLI observer functions print the same plan/progress/final information and map the same preserved failure to the existing exit code. The CLI remains synchronous and source-driven when `--max-duration` is absent.

- [ ] **Step 5: Run GREEN and real CLI parity smoke**

Compile and run the temporary controller probe, then delete it and all artifacts. Build the realtime CLI and use an existing local real RTP source to demonstrate that parser output, selected plan, first output, progress telemetry, source-loss/requested-stop distinction, and final exit are unchanged. This is a focused smoke, not the final acceptance gate.

- [ ] **Step 6: Review, commit, and push**

Confirm `src/internal/graph` has no diff, controller owns no CLI/Beta semantics, and every runtime exit resets resources. Verify CRLF/UTF-8 and no temporary residue. Commit and push:

```powershell
git add CMakeLists.txt src/application/realtime tools/realtime_video_cli/main.cpp
git commit -m "refactor(realtime): share external run controller"
git push origin codex/rkmpp-zero-copy
```

### Task 3: Asynchronous Beta Session, Snapshot, and Temporary Description RAII

**Files:**
- Create: `src/media_transcode_beta/MediaRealtimeBetaTemporaryDescription.h`
- Create: `src/media_transcode_beta/MediaRealtimeBetaTemporaryDescription.cpp`
- Create: `src/media_transcode_beta/MediaRealtimeBetaSnapshotProjector.h`
- Create: `src/media_transcode_beta/MediaRealtimeBetaSnapshotProjector.cpp`
- Create: `src/media_transcode_beta/MediaRealtimeBetaSession.h`
- Create: `src/media_transcode_beta/MediaRealtimeBetaSession.cpp`
- Modify: `src/media_transcode_beta/MediaRealtimeBetaRequestMapper.h`
- Modify: `src/media_transcode_beta/MediaRealtimeBetaRequestMapper.cpp`

**Interfaces:**
- Produces: one session implementation consumed only by the C bridge.
- Consumes: owned config, fixed profile, shared run controller, existing report values, and OS temporary-directory APIs.

- [ ] **Step 1: Write a temporary RED state-machine probe**

Create `out/tdd/realtime-beta-session-tdd.cpp` with a fake application-controller seam, not a fake graph. It asserts `Starting -> Running -> Stopping -> Completed`, startup failure to `Failed`, callback serialization, callback-safe stop/snapshot, deep-copied strings, final callback before join returns, and exact one-time temporary-file cleanup. Expected RED is missing session types.

- [ ] **Step 2: Implement unique temporary output-description ownership**

`MediaRealtimeBetaTemporaryDescription::create` uses the platform temporary directory plus a cryptographically collision-resistant or OS-atomically-created unique name, owns only that exact path, and removes only that file in its destructor. It never deletes a directory or caller path. Read the completed text into an owned string for the `OUTPUT_READY` callback; a missing/unreadable description is an output-stage failure, not an empty success.

- [ ] **Step 3: Implement one serialized event thread**

The session constructor accepts only already validated/owned inputs. `start` launches one `std::jthread` whose function:

1. emits `STARTING`;
2. creates the temporary description and completes request mapping;
3. calls the shared controller synchronously on that same event thread;
4. projects prepared facts, output description, periodic reports, errors, and final completion into public POD events/snapshot;
5. emits exactly one terminal `COMPLETED` or `FAILED` state and then returns.

All event strings live in session-owned storage through callback return. Copy the callback pointer/user data at start. Never call a callback while holding the snapshot mutex. Do not create per-frame/per-packet events or a second dispatch queue.

- [ ] **Step 4: Implement stop/release concurrency and state invariants**

- `requestStop()` atomically marks stop once and wakes controller polling; it is nonblocking and idempotent.
- A stop during preflight prevents runtime start after preflight returns and is reported as requested cancellation, while preserving any earlier real failure.
- `snapshot()` locks only the session snapshot mutex long enough to copy one POD.
- Record the event-thread identity. `release` invoked on it is forbidden by the public contract and must never join itself.
- External `release` requests stop, joins the event thread, destroys controller/session resources, removes the temp file, and permits no later callback.
- Destruction is noexcept; preserve the first operational error in the terminal event rather than replacing it with cleanup noise.

- [ ] **Step 5: Project only truthful existing evidence**

Map selected backend/filter/codec and `zero_copy_planned` from the selected plan; map queue, progress, packet, CPU, logical processor, and RSS fields only from `MediaGraphRuntimeReport`. Do not parse logs or turn planned zero-copy into observed zero-copy. Use explicit conversion/range checks and stable `UNKNOWN` values when a selected-plan concept has no Beta enum representation.

- [ ] **Step 6: Run GREEN and exact cleanup**

Run the fake-controller state-machine probe under callback reentrancy and concurrent external snapshot calls. Prove no callbacks after release and no self-join. Delete the fake seam, test source/target, generated description, and all binaries/PDB/ILK/objects.

- [ ] **Step 7: Review, commit, and push**

Audit lock ordering, callback lifetime, string ownership, stop idempotence, join/reset ordering, and exact path cleanup. Confirm no graph diff and no unbounded callback/report queue. Verify CRLF/UTF-8. Commit and push:

```powershell
git add src/media_transcode_beta
git commit -m "feat(beta): add asynchronous realtime session"
git push origin codex/rkmpp-zero-copy
```

### Task 4: C ABI Bridge and Static-Library Build Product

**Files:**
- Create: `src/media_transcode_beta/MediaRealtimeBetaApi.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `MediaTranscode::realtime_application` and `MediaTranscode::beta` CMake targets, with `media_transcode_beta` as a static library artifact.
- Consumes: the public C header, Beta session implementation, shared controller, and `MediaTranscode::core` transitively.

- [ ] **Step 1: Write temporary C linkage RED/GREEN consumers**

Use a C11 source compiled to an object by the platform C compiler, then link it with the C++ linker against the Beta CMake target and its declared transitive dependencies. Use a C++20 consumer as the second probe. Do not change `project(... LANGUAGES CXX)` merely for the temporary test and do not add a permanent consumer target.

- [ ] **Step 2: Implement the opaque handle and exception boundary**

Define `struct mt_beta_realtime_session` only in `MediaRealtimeBetaApi.cpp`; it owns one `MediaRealtimeBetaSession`. Each public function:

- validates its pointer arguments before dereference;
- catches `std::bad_alloc`, `std::exception`, and unknown exceptions;
- maps immediate failures to stable `mt_beta_status` values;
- never lets an exception cross C;
- leaves `*session == NULL` on failed start;
- nulls `*session` after external release completes.

Because `release` returns `void`, a callback-thread release attempt is a documented contract violation and performs no destruction: it leaves the handle untouched and never recursively invokes the callback or self-joins. Callers must release from another thread after the callback returns.

- [ ] **Step 3: Register focused static targets**

Add an application-layer static target containing only `MediaRealtimeVideoRunController`, linked to `MediaTranscode::core`. Add `media_transcode_beta STATIC` containing only Beta implementation files, with public include directory `include`, private `src`, C++20, `/utf-8 /EHsc` on MSVC, and transitive linkage to the application target. The realtime CLI links the application target and no longer compiles controller sources privately.

Expose aliases:

```cmake
add_library(MediaTranscode::realtime_application ALIAS media_transcode_realtime_application)
add_library(MediaTranscode::beta ALIAS media_transcode_beta)
```

The static archive is not claimed to be a self-contained replacement for FFmpeg/system link dependencies; CMake target usage propagates its declared link interface. Do not merge archives with platform-specific librarian scripts, copy FFmpeg binaries, create a shared library, or modify graph linkage.

- [ ] **Step 4: Run consumers and ABI misuse probes**

Run C and C++ consumers against the real static target. Exercise null arguments, unsupported enums, invalid VBR bounds, invalid addresses, callback stop/snapshot, external release, and a deliberately throwing internal factory seam. Expected: stable status/events, no exception escape, no callback after release, and no leaked temp description.

- [ ] **Step 5: Remove all temporary artifacts and commit**

Delete consumer/fault-injection sources, temporary target definitions, objects, executables, PDB/ILK, and generated SDP. Verify only production targets remain, build metadata contains no TDD target, and `git diff --check`/encoding checks pass. Commit and push:

```powershell
git add CMakeLists.txt src/media_transcode_beta src/application/realtime tools/realtime_video_cli/main.cpp include/media_transcode_beta
git commit -m "build(beta): publish realtime static library"
git push origin codex/rkmpp-zero-copy
```

### Task 5: Strict Release Builds and Shared-Path Regression

**Files:**
- Modify only if failures reveal a production defect in files already owned by Tasks 1-4.

- [ ] **Step 1: Run the strict Windows Release build skill**

Read and execute `.agents/skills/building-with-vs2026/SKILL.md` exactly. Fully regenerate and clean-build all Release targets within its wall-clock contract. Record successful linkage of `media_transcode_beta.lib`, both existing CLIs, and no `/showIncludes` trace. A timeout or partial target build is not a pass.

- [ ] **Step 2: Run direct absolute-path Windows regression commands**

Without Windows scripts or `Start-Process`, launch the 120-second continuous source, realtime CLI/Beta consumer, and visible VLC using direct absolute-path `&` commands in immediate sequence. Cover the same VideoOnly separate RTP input to MPEG-TS/RTP output route with Auto hardware selection, resolution conversion, H.264->HEVC and HEVC->H.264, one CBR case, and one VBR case. Record exact commands, PIDs, output picture, stalls, machine/single-core CPU, RSS trend, dropped buffers, A/V scope as VideoOnly, and truthful source-driven termination.

- [ ] **Step 3: Synchronize committed bytes to RK and build Release with 8 jobs**

Create a Git archive from the committed branch. On `192.168.96.211`, verify `pwd -P` exactly equals `/home/firefly/Downloads/MediaTranscode` before replacing the old tree; do not touch sibling media or versioned backups. Enter the FFmpeg environment with `ffenv on`, fully configure Release, and build all targets with `--parallel 8`. Preserve the exact output and prove `libmedia_transcode_beta.a` plus both CLIs link against the intended FFmpeg environment.

- [ ] **Step 4: Run RK real-stream Beta acceptance**

Use the existing real VideoOnly RTP feed or the required non-degraded continuous source, the Beta C consumer, and Windows VLC. Cover H.264->HEVC and HEVC->H.264, CBR and VBR, and resolution conversion without reducing source parameters. Confirm automatic FMTP discovery, explicit planner selection of RKMPP, MPEG-TS/RTP output, visible picture, no corruption/stalls, bounded RSS, CPU in machine and single-core units, no dropped buffers, zero-copy diagnostics from the existing graph, and truthful termination. Record exact absolute commands and PIDs; stop the source rather than forcibly terminating the consumer.

- [ ] **Step 5: Back up the accepted RK artifact and publish commands**

After all gates pass, create one timestamped directory under `/home/firefly/Downloads` containing the exact Release Beta archive, public header, required CMake/link manifest, example C source, and matching realtime CLI. Do not call it accepted if any codec/rate-control/quality/termination gate failed. Publish direct target-machine compile/link/run commands and the exact source/VLC commands used.

- [ ] **Step 6: Fix failures by ownership and repeat both platforms**

If shared controller/mapping/session logic fails, inspect and retest both Windows and RK. Only OS API, driver, hardware backend, or platform library adapters may differ. Do not lower parameters, add a platform media path, expose hidden knobs, insert fallback, or change constants to fit one sample.

### Task 6: Documentation, Quality Review, Dual Independent Review, and PR

**Files:**
- Create: `docs/realtime-beta-static-library.md`
- Modify: `QUALITY_SCORE.md`
- Modify: `docs/superpowers/plans/2026-08-18-realtime-beta-static-library.md`

- [ ] **Step 1: Write the concise external guide**

Document supported scope, header/target names, C and C++ linkage, full four-function example, input ownership/deep-copy rule, callback lifetime/reentrancy, forbidden callback-thread release, CBR/VBR union usage, automatic FMTP discovery, Linux RKMPP/Windows Auto behavior, output description callback, stop/release sequence, and Beta instability. Do not document hidden constants as caller options.

- [ ] **Step 2: Update completion and quality evidence**

Check every plan box only after evidence exists. Update `QUALITY_SCORE.md` concisely with DAG reuse, public-boundary safety, lifecycle, resource observability, Windows/RK parity, real-stream evidence, and explicit remaining debt: fixed Beta profile, no stable ABI promise, and detailed zero-copy facts still available only through graph diagnostics.

- [ ] **Step 3: Freeze and run two independent reviews**

Freeze the worktree at one commit. Spawn two new agents that did not implement the change; both must read the code-review skill and independently review the same fixed commit against the spec, this plan, AGENTS.md, C ABI safety, static linkage, DAG isolation, lifecycle/RAII, callback concurrency, exact parameter mapping, Windows/RK shared logic, no fallback/hardcoded-value leakage, encoding, and temporary residue. Neither reviewer may rely on the other's findings.

- [ ] **Step 4: Resolve every blocking finding and re-review**

For any Critical or Important finding, apply TDD RED/GREEN when code changes are needed, delete the temporary test, rebuild both affected Release paths, commit/push, freeze a new commit, and send both original reviewers the new fixed commit. Repeat until both explicitly report zero Critical and zero Important.

- [ ] **Step 5: Final verification, commit, push, and PR**

Run final status/diff/UTF-8/CRLF/temp-artifact/process/port checks and the strongest Release validations again. Commit documentation and evidence, push the branch, create the PR, and include exact real CLI/Beta/FFmpeg/VLC results plus remaining risks. Do not include credentials, build commands, generated SDP, temporary tests, untracked FFmpeg headers, or `out/`.

```powershell
git add docs/realtime-beta-static-library.md QUALITY_SCORE.md docs/superpowers/plans/2026-08-18-realtime-beta-static-library.md
git commit -m "docs(beta): record realtime static library acceptance"
git push origin codex/rkmpp-zero-copy
```

## Completion Gate

The Beta library is complete only when all of the following are simultaneously true:

- a C11 and a C++20 caller compile and link through the declared static CMake target;
- the four-function API obeys deep-copy, callback, stop, snapshot, join, and exception-boundary contracts;
- no public field exposes FMTP, SDP path, hardware backend, queue/startup/probe/pacing capacity, or internal timeout;
- CLI and Beta use the same application controller and the same existing planner/builder/DAG/runtime path;
- `src/internal/graph` contains no Beta-specific dependency or special route;
- Windows strict Release all-target build and same-route regression pass;
- RK `ffenv on` Release all-target build with 8 jobs and real-stream acceptance pass;
- H.264/HEVC both directions, CBR/VBR, and resolution conversion have real visual and runtime evidence;
- no lowered test standard, runtime fallback, hidden error, temporary TDD residue, process/port residue, UTF-8/CRLF defect, or credential exists;
- two independent reviewers approve the same frozen commit with zero Critical and zero Important findings;
- every task commit and the final documentation commit are pushed to `codex/rkmpp-zero-copy`, and the PR is created.
