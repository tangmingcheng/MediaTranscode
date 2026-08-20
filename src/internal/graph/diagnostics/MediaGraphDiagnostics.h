#pragma once

#include "internal/graph/model/MediaEdgeKind.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

class MediaChannel;

enum class MediaGraphDiagnosticLevel {
    Off = 0,
    Summary,
    State,
    Flow,
    Trace
};

struct MediaGraphDiagnosticConfig {
    MediaGraphDiagnosticLevel level = MediaGraphDiagnosticLevel::State;
    std::size_t firstPacketLimit = 5;
    std::size_t packetSampleInterval = 100;
};

struct MediaGraphDiagnosticSampleDecision {
    bool shouldLog = false;
    std::uint64_t sequence = 0;
    bool sampled = false;
};

class MediaGraphDiagnosticSampler final {
public:
    explicit MediaGraphDiagnosticSampler(
        MediaGraphDiagnosticLevel requiredLevel) noexcept;

    MediaGraphDiagnosticSampleDecision sample(bool force = false) noexcept;
    void reset() noexcept;

private:
    MediaGraphDiagnosticLevel m_requiredLevel;
    std::uint64_t m_sequence = 0;
};

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
const char* mediaGraphDiagnosticLevelName(MediaGraphDiagnosticLevel level) noexcept;
MediaGraphDiagnosticLevel mediaGraphDiagnosticLevelFromString(const std::string& text,
                                                              MediaGraphDiagnosticLevel missingLevel) noexcept;

const char* mediaGraphDiagnosticNodeKindName(MediaNodeKind kind) noexcept;
const char* mediaGraphDiagnosticStreamKindName(MediaStreamKind kind) noexcept;
const char* mediaGraphDiagnosticEdgeKindName(MediaEdgeKind kind) noexcept;
const char* mediaGraphDiagnosticPayloadKindName(MediaPayloadKind kind) noexcept;

void mediaGraphDiagnosticSetGlobalEnabled(bool enabled) noexcept;
bool mediaGraphDiagnosticGlobalEnabled() noexcept;

void mediaGraphDiagnosticSetGlobalConfig(MediaGraphDiagnosticConfig config);
MediaGraphDiagnosticConfig mediaGraphDiagnosticGlobalConfig();
bool mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel requiredLevel) noexcept;

MediaGraphDiagnosticSampleDecision mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel requiredLevel,
                                                              const std::string& key,
                                                              bool force = false);
void mediaGraphDiagnosticResetSampling();

std::string mediaGraphDiagnosticDescribeBuffer(const MediaBufferRef& buffer);
std::string mediaGraphDiagnosticDescribeChannel(const MediaChannel& channel);

void mediaGraphDiagnosticLog(bool enabled,
                              MediaGraphDiagnosticPhase phase,
                              const std::string& message);
void mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel requiredLevel,
                             MediaGraphDiagnosticPhase phase,
                             const std::string& message);

} // namespace media::ffmpeg::graph
