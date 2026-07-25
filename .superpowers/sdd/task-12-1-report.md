# Task 12.1 Report

## Handoff and RED evidence

- Continued the interrupted Task 12.1 work on `codex/quality-priority-improvements` from base `ca27afc3716e0012a7e0b26c0a2b6ce19f8de15e`.
- The inherited RED record was the expected clean-build C1083 for the then-missing `MediaRealtimeAvSyncRuntimePlan.h`; the handoff already contained the corresponding GREEN implementation.
- A fresh clean build exposed no remaining move-only compile error. Deterministic verification later found an old builder fixture still omitting the new mandatory encoder frame/delay facts; the Debug assertion wait was reproduced and fixed by explicitly supplying AAC `1024/0`, without a production default or fallback.

## Delivered

- Planner-owned `MediaRealtimeAvSyncRuntimePlan` for synchronized separate RTP and project MPEG-TS outputs.
- Dedicated facts resolver, runtime product constructor, runtime product validator, and generation-transition planner; `MediaRealtimeRtpTranscodePlanner.cpp` is 573 lines instead of the interim 1112-line version.
- Complete typed `MediaRtpUdpSenderConfig` is produced by output policy planning and moved into the runtime product. Runtime planning does not truncate or reparse an output URL.
- RTP packetization descriptors contain planner decisions only. Searches found no `AVCodecParameters`, codec `extradata`, or `ScheduledRtpMuxStreamConfig` in the RTP descriptor types.
- Negative coverage includes RTP/TS adapter and topology pairs, queue/edge/threading products, participant identities/counts, transition timeouts, protocol fields, correction bounds, legacy pacing/barrier authority, and missing codec/queue/resampler/servo facts.

## Verification

- `cmd.exe /d /s /c '"D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out/build/x64-debug --clean-first --target all'` — exit 0; 388 files cleaned and all 389 build actions completed.
- `out/build/x64-debug/media_transcode_planner_tests.exe` — exit 0.
- `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60` — 8/8 passed, 0 failed, 11.01 seconds.

## Self-review

- Planner remains the sole decision authority; builders consume the accepted synchronization product.
- Mandatory facts fail closed. Checked arithmetic rejects zero correction ppm, overflow, missing bounds, and unreachable command lead.
- Move-only RTP sender plans are moved through production and tests; no copy workaround was added.
- Task files are UTF-8 with CRLF and the staged whitelist is checked independently from unrelated workspace changes.

## Remaining concerns

- This planner-product task does not provide the later Task 12 production runtime wiring or hardware playback acceptance; those remain gated by the subsequent design sequence.
- Legacy pacing/barrier fields still physically exist for non-synchronized paths. Synchronized validation requires them to remain inert until the later atomic authority-removal task.

## Third-review remediation

- Added complete separate-RTP mutation proof for both stream descriptors, both typed transports, cross-stream endpoint layout, sender identity/timing, and the audio packetization/correction batch equality. Non-representable codec/mode/address-family/I/O combinations are rejected by their typed factories.
- Audited every `MediaTsMuxPlanParameters` field: all legally constructible alternatives are rejected as runtime mismatches; fixed-domain and invalid values are covered by the mux-plan factory matrix. All outer TS runtime fields, including protocol audio sample rate, are mutated independently.
- Selected AAC decoder capability comes from an opened RAII `AVCodecContext`. Selected resampler capability comes from an initialized RAII `SwrContext`; it primes one rational phase and requires two identical subsequent phase-state sequences before publishing the maximum `swr_get_out_samples` bound. A 48 kHz to 44.1 kHz regression proves the steady-state bound exceeds the fresh rate-scaled block. No FFmpeg runtime context is stored in a plan.
- RED: resetting the selected resolved audio output caused the runtime validator to dereference an empty optional and leave the planner test process hung. GREEN: the RTP validator now rejects the incomplete selected product before descriptor comparison.

## Fresh verification after final source changes

- `out/build/x64-debug/media_transcode_planner_tests.exe` - exit 0.
- `out/build/x64-debug/media_transcode_builder_tests.exe` - exit 0.
- `out/build/x64-debug/media_transcode_node_tests.exe` - exit 0.
- `out/build/x64-debug/media_transcode_core_tests.exe` - exit 0.
- `out/build/x64-debug/media_transcode_runtime_tests.exe` - exit 0.
- `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60` - 8/8 passed, 0 failed, 12.62 seconds after CRLF normalization.

## Third-review findings 12-13 remediation

- Renamed the decoder fact to `delayOutputSamples` with no compatibility alias. The shared component-bounds planner converts it from the selected decoder output sample rate and rejects decoder/resampler/encoder domain conflicts. An unequal 96 kHz input / 48 kHz decoder-output regression preserves a 480-sample decoder delay instead of scaling it as an input-domain value.
- The authoritative runtime product now retains the complete finalized `MediaRealtimeAvSyncPlanningFacts`. Final validation recomputes component bounds from selected decoder, resampler, queue, encoder, and mailbox products; resolves protocol facts; compares every retained fact; and uses one shared checked reachability planner to compare the complete correction product and exact command-lead, compensation-window, and frequency-filter time representations.
- Tamper coverage removes component facts, substitutes positive component bounds, mutates every retained planning fact, mutates each correction/time field, and substitutes a separately recomputed positive self-consistent facts/correction/time set. All are rejected against the selected components.
- RED: planner compilation failed at the missing shared component/reachability planner contracts. Focused GREEN: planner build and executable both exited `0` with the new unequal-rate and authoritative-product regressions.
- Fresh verification: planner, builder, node, core, and runtime executables exited `0`; after final CRLF normalization, clean-first all exited `0` with 393 files cleaned and 394 actions, and deterministic passed 8/8 in 14.05 seconds.

## Independent review remediation

- RED: under `VsDevCmd.bat`, the planner target built and the planner executable failed at `tests/unit/test_planner.cpp:293` because removing the mandatory synchronized runtime product was still accepted.
- GREEN: synchronized A/V products now reject an absent runtime product; video-only products remain explicitly non-synchronized.
- Decoder, resampler, queue, mailbox, and protocol bounds are published as typed planning facts. The resolver only validates and transfers them. A regression supplies distinct values for every bound and verifies none are substituted.
- Copy-path codec frame timing is supplied by the selected input capability; the production AAC `1024` codec-name fallback was removed. Raw RTP uses the parsed access-unit duration, while prepared MPEG-TS requires codec parameters to publish frame size and initial padding.
- RTP packetization is immutable, non-default-constructible, and created by one codec-to-packetization factory. Runtime planning no longer passes a packetization-mode constant.
- `MediaAvSyncPlanner` emits an explicitly incomplete policy. Correction lead, compensation window, and frequency filter are finalized once from reachability facts before the full validator accepts the runtime plan.
- RTP validation now covers typed local/remote addressing, address family, RTP/RTCP ports, local-port policy, send buffer, datagram size, and I/O behavior. MPEG-TS validation compares the complete accepted mux parameter product.
- `MediaAvSyncOutputAdapterKind` moved to a neutral plan header, removing the transition planner's dependency on the full runtime product.

### Review remediation verification

- `cmd.exe /d /s /c '"D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --config Debug --target media_transcode_planner_tests && out\build\x64-debug\media_transcode_planner_tests.exe'` - exit 0.
- `cmd.exe /d /s /c '"D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --config Debug --clean-first --target all'` - exit 0; 389 files cleaned and 389 build actions completed.
- `out\build\x64-debug\media_transcode_planner_tests.exe` - exit 0.
- `out\build\x64-debug\media_transcode_builder_tests.exe` - exit 0.
- `ctest --test-dir out\build\x64-debug -C Debug --output-on-failure -L deterministic --timeout 60` - 8/8 passed, 0 failed, 12.87 seconds.
