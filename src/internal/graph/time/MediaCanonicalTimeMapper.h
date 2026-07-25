#pragma once

#include "internal/graph/sync/MediaAvSyncError.h"
#include "internal/graph/time/MediaMappedTimestamp.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaCanonicalTimeMapperConfig final {
    MediaRunningTime sourceEpoch;
    MediaRunningTime runningTimeEpoch;
    MediaAvSyncTopology topology;
    std::string sourceIdentity;
    std::uint64_t generation;
};

class MediaCanonicalTimeMapper final {
public:
    static ::media::Result<MediaCanonicalTimeMapper, MediaAvSyncError> create(
        MediaCanonicalTimeMapperConfig config);

    ::media::Result<MediaMappedTimestamp, MediaAvSyncError> map(
        const MediaCanonicalSourceTimestamp& source) const;
    ::media::Result<void, MediaAvSyncError> reset(MediaCanonicalTimeMapperConfig config);

    std::uint64_t generation() const noexcept { return m_config.generation; }

private:
    explicit MediaCanonicalTimeMapper(MediaCanonicalTimeMapperConfig config) noexcept;

    ::media::Result<MediaRunningTime, MediaAvSyncError> mapOne(
        MediaRunningTime sourceTime,
        const char* field) const;

    MediaCanonicalTimeMapperConfig m_config;
};

} // namespace media::ffmpeg::graph
