#pragma once
#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/MediaVideoStartupReadiness.h"
#include <array>
#include <string_view>

namespace media::ffmpeg::graph {
enum class MediaStartupEventMediaForm { VideoEnvelope, AudioEnvelope, Release, Transaction };
enum class MediaStartupRouteRole { VideoInput, AudioInput, Coordinator, Binder, Sequencer, Extractor };
enum class MediaStartupRoutePolicy { Metadata, AtomicMetadata };
struct MediaStartupRouteConnection final {
    MediaStartupRouteRole from;
    std::string_view output;
    MediaStartupRouteRole to;
    std::string_view input;
    MediaStartupEventMediaForm form;
    MediaStartupRoutePolicy policy;
    std::string_view label;
};
struct MediaStartupRouteShape final {
    MediaTranscodeStreamSet members;
    MediaVideoStartupReadiness readiness;
    // One authoritative descriptor set, consumed by assembly and validation.
    using Connections = std::array<MediaStartupRouteConnection, 6>;
    static const Connections& connections() noexcept;
    bool includes(const MediaStartupRouteConnection& connection) const noexcept;
    MediaNodeKind nodeKind(MediaStartupRouteRole role) const noexcept;
    const MediaStartupRouteConnection* find(MediaNodeKind from, std::string_view output,
        MediaNodeKind to, std::string_view input) const noexcept;
};
struct MediaStartupRoutePlan final {
    MediaStartupRouteShape shape;
    MediaEdgePolicy metadata;
    MediaEdgePolicy atomicMetadata;
    const MediaEdgePolicy& policy(const MediaStartupRouteConnection& connection) const noexcept;
};
} // namespace media::ffmpeg::graph
