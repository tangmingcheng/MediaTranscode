#pragma once

#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaMappedTimestamp.h"
#include "media_transcode/Result.h"

#include <memory>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaCanonicalLineage final {
    MediaRunningTime presentation;
    std::optional<MediaRunningTime> decode;
    MediaRunningTime duration;
    MediaDecodeOrderMode decodeOrder;
    std::string sourceIdentity;
    MediaSourceAccessUnitSequence sourceSequence;
    MediaTimeMappingConfidence mappingConfidence;
    std::uint64_t generation;
};

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
createMediaCanonicalLineage(const MediaMappedTimestamp& mapped,
                            MediaDecodeOrderMode decodeOrder,
                            MediaSourceAccessUnitSequence sourceSequence);

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
createMediaCanonicalLineage(
    MediaRunningTime presentation,
    std::optional<MediaRunningTime> decode,
    MediaRunningTime duration,
    MediaDecodeOrderMode decodeOrder,
    std::string sourceIdentity,
    MediaSourceAccessUnitSequence sourceSequence,
    MediaTimeMappingConfidence mappingConfidence,
    std::uint64_t generation);

::media::Status validateMediaCanonicalLineage(
    const MediaCanonicalLineage& lineage) noexcept;

} // namespace media::ffmpeg::graph
