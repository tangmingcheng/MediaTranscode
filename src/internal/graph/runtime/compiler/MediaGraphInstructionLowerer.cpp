#include "internal/graph/runtime/compiler/MediaGraphInstructionLowerer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaGraphInstructionPlan> MediaGraphInstructionLowerer::lower(const MediaGraph& graph)
{
    MediaGraphInstructionPlan plan;
    plan.instructions.reserve(graph.nodes().size());

    for (const MediaNode& node : graph.nodes()) {
        MediaGraphInstruction instruction;
        instruction.instructionKind = classify(node.kind);
        instruction.nodeId = node.id;
        instruction.nodeKind = node.kind;
        instruction.name = node.name;
        plan.instructions.push_back(std::move(instruction));
    }

    return ::media::Result<MediaGraphInstructionPlan>::success(std::move(plan));
}

MediaGraphInstructionKind MediaGraphInstructionLowerer::classify(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::FileInput:
    case MediaNodeKind::RealtimeInput:
        return MediaGraphInstructionKind::Source;

    case MediaNodeKind::FileOutput:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
        return MediaGraphInstructionKind::Sink;

    case MediaNodeKind::EofBarrier:
    case MediaNodeKind::Flush:
    case MediaNodeKind::Finalize:
    case MediaNodeKind::ControlSignal:
        return MediaGraphInstructionKind::Control;

    case MediaNodeKind::MetadataProbe:
    case MediaNodeKind::DebugDump:
    case MediaNodeKind::TraceProbe:
        return MediaGraphInstructionKind::Diagnostic;

    default:
        return MediaGraphInstructionKind::Transform;
    }
}

} // namespace media::ffmpeg::graph
