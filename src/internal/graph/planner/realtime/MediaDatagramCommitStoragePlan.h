#pragma once
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"
#include <optional>
namespace media::ffmpeg::graph {
enum class MediaDatagramCommitStorageKind { Rtp, MpegTsUdp };
struct MediaDatagramCommitStoragePlan final {
    MediaDatagramCommitStorageKind kind;
    MediaSharedAllocationGeometry geometry;
    MediaSharedAllocationGeometry batchGeometry;
    std::optional<MediaSharedAllocationGeometry> protocolPrefixGeometry;
};
} // namespace media::ffmpeg::graph
