#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaGraphError.h"
#include "internal/graph/core/MediaNodeId.h"

#include <cstddef>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphValidationSeverity {
    Info,
    Warning,
    Error
};

struct MediaGraphValidationIssue {
    MediaGraphValidationSeverity severity = MediaGraphValidationSeverity::Info;
    MediaGraphErrorCode code = MediaGraphErrorCode::None;
    std::string message;

    MediaNodeId nodeId = MediaNodeId::invalid();
    MediaPortId portId = MediaPortId::invalid();
    MediaEdgeId edgeId = MediaEdgeId::invalid();
};

struct MediaGraphValidationReport {
    std::vector<MediaGraphValidationIssue> issues;

    bool ok() const;
    bool hasErrors() const;

    std::size_t errorCount() const;
    std::size_t warningCount() const;
};

class MediaGraphValidation final {
public:
    static MediaGraphValidationReport validate(const MediaGraph& graph);
};

} // namespace media::ffmpeg::graph
