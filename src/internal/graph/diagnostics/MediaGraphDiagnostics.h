#pragma once

#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaChannel;

enum class MediaGraphDiagnosticPhase {
    PlannerInput,
    PlannerCapability,
    PlannerScore,
    PlannerSelect,
    GraphBuild,

    RuntimeNode,
    RuntimeEdge,
    RuntimeChannel,
    RuntimeLifecycle
};

const char* mediaGraphDiagnosticPhaseName(MediaGraphDiagnosticPhase phase) noexcept;
const char* mediaGraphDiagnosticNodeKindName(MediaNodeKind kind) noexcept;
const char* mediaGraphDiagnosticStreamKindName(MediaStreamKind kind) noexcept;
const char* mediaGraphDiagnosticEdgeKindName(MediaEdgeKind kind) noexcept;
const char* mediaGraphDiagnosticPayloadKindName(MediaPayloadKind kind) noexcept;

void mediaGraphDiagnosticSetGlobalEnabled(bool enabled) noexcept;
bool mediaGraphDiagnosticGlobalEnabled() noexcept;

std::string mediaGraphDiagnosticDescribeBuffer(const MediaBufferRef& buffer);
std::string mediaGraphDiagnosticDescribeChannel(const MediaChannel& channel);

void mediaGraphDiagnosticLog(bool enabled,
                              MediaGraphDiagnosticPhase phase,
                              const std::string& message);

} // namespace media::ffmpeg::graph
