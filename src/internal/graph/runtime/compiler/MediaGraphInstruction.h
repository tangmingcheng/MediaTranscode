#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaNodeKind.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphInstructionKind {
    Unknown,
    Source,
    Transform,
    Sink,
    Control,
    Diagnostic
};

struct MediaGraphInstruction {
    MediaGraphInstructionKind instructionKind = MediaGraphInstructionKind::Unknown;
    MediaNodeId nodeId = MediaNodeId::invalid();
    MediaNodeKind nodeKind = MediaNodeKind::Unknown;
    std::string name;
};

struct MediaGraphInstructionPlan {
    std::vector<MediaGraphInstruction> instructions;

    bool empty() const noexcept
    {
        return instructions.empty();
    }

    std::size_t size() const noexcept
    {
        return instructions.size();
    }
};

} // namespace media::ffmpeg::graph
