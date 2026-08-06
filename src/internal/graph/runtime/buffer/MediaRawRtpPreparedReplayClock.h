#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaRawRtpPreparedReplayEpoch final {
    std::int64_t sourceOriginNs;
    std::int64_t runtimeOriginNs;
};

class MediaRawRtpPreparedReplayClock final {
public:
    ::media::Status observe(std::int64_t observedAtNs);
    ::media::Result<MediaRawRtpPreparedReplayEpoch> activate();

private:
    std::mutex m_mutex;
    std::optional<std::int64_t> m_sourceOriginNs;
    std::optional<std::int64_t> m_runtimeOriginNs;
};

} // namespace media::ffmpeg::graph
