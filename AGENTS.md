# AGENTS.md

This file is the repository-level harness for OpenAI Codex and other coding agents. Keep it short, executable, and project-specific. Prefer local evidence from the codebase over assumptions.

## Project scope

This repository implements a C++17 FFmpeg media transcoding framework. The current focus is the `src/internal/graph` DAG pipeline architecture and the next major feature is realtime video transcoding.

Primary graph areas:

- `src/internal/graph/core`: graph model, node/port/edge descriptors, topology, validation.
- `src/internal/graph/model`: media kinds, policies, descriptors, transcode parameters.
- `src/internal/graph/planner`: strategy selection. Planner must produce plan objects and must not mutate `MediaGraph`.
- `src/internal/graph/builder`: DAG construction. Builders decide topology and connect nodes.
- `src/internal/graph/builder/segments`: reusable branch/segment builders and option appliers.
- `src/internal/graph/runtime`: graph compilation, channels, queues, scheduler, threaded executor.
- `src/internal/graph/nodes`: concrete runtime node implementations.

## Architectural rules

Follow these rules when modifying graph code:

1. Reusable logic shared by audio/video/realtime/packet-copy paths belongs in a common helper or shared segment component.
2. Logic used only by one builder, and shorter than a small cohesive helper, may stay in that `.cpp` file's anonymous namespace.
3. Code that writes node options should be centralized in an `OptionApplier` class when the option has semantic meaning beyond a single local builder detail.
4. Code that decides topology belongs in builders.
5. Code that decides strategy, codec choice, branch mode, hardware/software preference, or fallback belongs in planners.
6. Planner output is data. Builders consume plans. Runtime nodes execute graph nodes.
7. Do not let a planner mutate `MediaGraph` or create nodes/ports/edges.
8. Do not let runtime nodes decide global topology or branch mode.
9. Avoid over-splitting pure field-copy helpers unless the helper is reused, enforces invariants, or is expected to grow.
10. Avoid keeping obsolete APIs around for compatibility unless an active caller still requires them.

## DAG invariants

All graph changes must preserve these invariants:

- Nodes are connected only through named ports.
- Every required input port must be connected before runtime compile.
- Edge stream kind, payload kind, format descriptor, time descriptor, hardware descriptor, and queue policy must remain meaningful.
- Graphs must remain acyclic.
- Packet-copy branches must not decode or encode.
- Transcode branches must receive codec metadata before packet/frame processing.
- Mux/output segments must know expected video/audio streams before runtime execution.
- Realtime paths must use bounded queues and explicit backpressure/drop policies.

## Current realtime-transcode readiness

The DAG framework is ready for incremental realtime-video-transcode work, but not for a full one-shot end-to-end implementation.

Known current gaps:

- `RealtimeInputNode` is currently a placeholder and must be implemented before realtime decode can work.
- `RtpMuxNode` is currently a placeholder and must be implemented before real RTP packetization/muxing can work.
- `RtpOutputNode` currently forwards buffers and does not yet perform network RTP output.
- `MediaRealtimeGraphKind` currently has packet-relay and ingest-to-mux modes only; a realtime video transcode graph kind/builder is still needed.
- Existing video transcode branch builders and runtime nodes should be reused instead of duplicated.

Recommended next implementation sequence:

1. Add `MediaRealtimeGraphKind::VideoTranscode`.
2. Add `MediaRealtimeVideoTranscodeGraphBuilder` as a concrete builder dispatched by `MediaRealtimeGraphBuilder`.
3. Reuse `MediaVideoTranscodeBranchBuilder` for decode/filter/encode topology.
4. Implement the minimum `RealtimeInputNode` behavior needed to emit format metadata and packets.
5. First verify encoded packet production through debug/packet inspection before implementing full RTP network output.
6. Implement `RtpMuxNode` and `RtpOutputNode` only after the realtime decode/encode packet path is proven.

## Builder and applier conventions

Use these naming patterns:

- `*GraphBuilder`: builds a complete graph or graph preset.
- `*SegmentBuilder`: builds a reusable graph segment.
- `*BranchBuilder`: builds one concrete branch topology.
- `*OptionsMapper`: maps one option shape into another only when this removes duplication or preserves a boundary.
- `*OptionApplier`: writes semantic node options.
- `*PlannerRequestBuilder`: converts user/build options into planner requests.

Preferred flow:

```text
User options -> PlannerRequestBuilder -> Planner -> Plan -> GraphBuilder -> Segment/BranchBuilder -> OptionApplier -> MediaGraph -> Runtime
```

## Verification commands

Run the strongest available local verification before finishing a task.

Preferred CMake build examples:

```bash
cmake --build out/build/x64-debug
cmake --build out/build/x64-release
```

If those build directories do not exist, configure first using the repository's existing CMake workflow. Do not invent a new build system.

Useful static searches before completion:

```bash
rg "createLocalFileRemux|applyVideoPlanToGraph|MediaPipelineGraphBuilder"
rg "setNodeOptionChecked" src/internal/graph
rg "MediaRealtimeGraphKind" src/internal/graph
rg "TODO|FIXME" src/internal/graph
```

When editing graph builders, also inspect:

```bash
rg "BranchSegmentBuilder|BranchBuilder|OptionApplier|Planner" src/internal/graph
```

## Completion checklist

Before reporting completion:

- Build or explicitly state why build could not be run.
- Confirm no obsolete API remains unless justified by an active caller.
- Confirm planner/builder/runtime boundaries were not crossed.
- Confirm node option writes are in appliers where appropriate.
- Confirm graph validation assumptions still hold.
- Confirm new files are picked up by the existing CMake glob or otherwise added to the correct target.
- Summarize changed files and the reason for each change.

## Do not do this

- Do not rewrite unrelated graph layers while implementing a focused feature.
- Do not duplicate local-file transcode logic into realtime code when a segment builder can be reused.
- Do not hide validation failures by making ports optional unless the stream is truly optional.
- Do not add a new node kind in the middle of `MediaNodeKind`; append only.
- Do not change public behavior just to make tests pass.
- Do not claim a build passed unless the command actually ran successfully.
- Do not silently remove hardware-planning metadata or timebase propagation.

## Response expectations for agents

When returning work to the user, include:

1. What changed.
2. Why it matches the DAG architecture.
3. What verification was run.
4. What remains risky or unimplemented.

Be explicit about uncertainty. A partial but honest result is better than a confident unsupported claim.
