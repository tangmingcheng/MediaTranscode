#pragma once

#include "internal/graph/core/MediaNodeId.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaGraphErrorCode {
    None,
    InvalidNode,
    InvalidPort,
    InvalidEdge,
    MissingNode,
    MissingPort,
    DuplicateId,
    DuplicatePortName,
    PortDirectionMismatch,
    PortTypeMismatch,
    RequiredInputMissing,
    RequiredOutputUnused,
    InputMultiplicityViolation,
    CycleDetected,
    InvalidPolicy,
    InvalidDescriptor
};

enum class MediaGraphErrorSeverity {
    Info,
    Warning,
    Error
};

struct MediaGraphError {
    MediaGraphErrorCode code = MediaGraphErrorCode::None;
    MediaGraphErrorSeverity severity = MediaGraphErrorSeverity::Info;

    std::string message;

    MediaNodeId nodeId = MediaNodeId::invalid();
    MediaPortId portId = MediaPortId::invalid();
    MediaEdgeId edgeId = MediaEdgeId::invalid();

    bool isError() const noexcept
    {
        return severity == MediaGraphErrorSeverity::Error;
    }
};

} // namespace media::ffmpeg::graph
