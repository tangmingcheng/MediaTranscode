# A/V Sync Production Assembly Task 6 Completion

Task 6 wires planner-owned audio drift correction into the synchronized audio
transcode path and publishes one canonical encoded-audio endpoint for the shared
A/V scheduler.

## Result

- `MediaAudioDriftControllerNode` executes the planned servo policy after
  startup trim and before resampling. It publishes the correction decision
  before atomically releasing the corresponding decoded audio.
- Correction commands and audio remain transactionally retained across
  backpressure. EOF, flush, abort, reset, and retained-control freshness preserve
  their typed lifecycle semantics.
- `MediaEncodedAudioCanonicalizerNode` derives presentation and duration from
  exact contiguous sample lineage plus the playback origin. Codec packet PTS is
  not used to reconstruct canonical scheduler time.
- The canonicalizer owns the monotonic encoded-audio sequence and publishes the
  typed `EncodedAudioCanonicalizer.canonical` packet endpoint.
- Generic packet-copy and audio branch builders return complete typed endpoints
  without owning mux connections. Local and realtime graph builders explicitly
  consume those endpoints; the video packet-copy wrapper retains its mux owner
  responsibility.
- Synchronized audio packet copy fails closed as `Unsupported` at planner,
  planned-product validation, runtime planning, and segment construction
  boundaries. Audio transcoding remains naturally selected by comparing
  resolved input and target parameters; no `transcodeAudio` switch was added.
- All synchronized correction, lineage, queue, generation, lookahead, and sync
  group values originate from the planner. Builders and runtime nodes do not
  supply defaults or fallback modes.
- The runtime factory and compiler register both new node kinds and reject an
  incomplete synchronized runtime product before execution.

Implementation commit: `7c4cf86` (`feat: wire synchronized audio correction`).

## Verification

Commands were executed from the repository root in PowerShell. Build commands
are omitted.

```powershell
.\out\build\x64-debug\media_transcode_builder_tests.exe
.\out\build\x64-debug\media_transcode_planner_tests.exe
.\out\build\x64-debug\media_transcode_integration_tests.exe --task5-av-sync-input
ctest --test-dir out/build/x64-debug -R '^media_transcode_av_sync_audio_control_tests$' --repeat until-fail:20 --output-on-failure
ctest --test-dir out/build/x64-debug -R '^(media_transcode_core_tests|media_transcode_builder_tests|media_transcode_av_sync_audio_control_tests|media_transcode_audio_codec_lineage_tests|media_transcode_audio_lineage_node_tests)$' --output-on-failure -j 4
```

Results:

- Builder tests passed with exit code 0.
- Planner tests passed with exit code 0.
- Focused Task 5 A/V-sync input integration passed with exit code 0 and reported
  `Task 5 A/V sync input tests passed`.
- Audio-control lifecycle repetition passed 20/20.
- The focused core, builder, audio-control, codec-lineage, and lineage-node suite
  passed 5/5 in 3.04 seconds.
- The final VS2026 clean rebuild completed successfully with `/showIncludes`
  absent from the generated rules. This build result is recorded here without a
  build command, as required by the repository verification policy.

The complete integration target still has three existing Opus expectations that
assume packetization without the fixed audio access-unit-duration synchronization
contract. The focused Task 5 integration passes, and independent diagnosis found
no Task 6 typed-endpoint regression. These three expectations remain separate
follow-up work and are not reported as passing.

## Review

The first whole-change review rejected synchronized `CopyPacket` because it could
bypass correction and canonical lineage, and noted that the generic endpoint type
lived in a realtime-specific header. The final implementation fails synchronized
copy closed, moves `MediaEndpoint` to the common builder boundary, and directly
tests canonical audio, generic packet-copy, and video mux endpoint ownership.

The independent final review passed with no Critical, Important, or Minor
findings and no remaining suggestions. Review also confirmed that request-level
audio planning already requires frame transcoding, so the unreachable duplicate
top-level copy check was removed. Natural-copy coverage remains at the planned-
product validator and runtime planner boundaries, with the segment boundary
covered separately.

## Remaining Scope

- Task 7 must connect the canonical video and audio endpoints to exactly one
  shared scheduler and typed output router.
- The three legacy Opus integration expectations require a separate contract
  update before the complete integration target can be green.
- Hardware RTP and MPEG-TS playback, drift diagnostics, two-minute stability,
  and process-scoped CPU measurement remain production-assembly acceptance work;
  they are not claimed by Task 6.
- Automatic cross-generation reacquisition remains Task 13 and continues to fail
  closed in the current production assembly.
