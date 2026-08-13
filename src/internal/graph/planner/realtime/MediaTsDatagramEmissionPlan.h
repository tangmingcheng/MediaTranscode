#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

class MediaTsDatagramEmissionPlan final {
public:
    static ::media::Result<MediaTsDatagramEmissionPlan> create(
        std::int64_t wireBytesPerSecond,
        std::size_t burstWireBytes,
        MediaRunningTime maximumLateness,
        std::size_t maximumPayloadBytes,
        std::size_t perDatagramOverheadBytes);

    std::int64_t wireBytesPerSecond() const noexcept;
    std::size_t burstWireBytes() const noexcept;
    MediaRunningTime maximumLateness() const noexcept;
    std::size_t maximumPayloadBytes() const noexcept;
    std::size_t perDatagramOverheadBytes() const noexcept;
    std::size_t maximumWireDatagramBytes() const noexcept;

    friend bool operator==(const MediaTsDatagramEmissionPlan&,
                           const MediaTsDatagramEmissionPlan&) = default;

private:
    MediaTsDatagramEmissionPlan(
        std::int64_t wireBytesPerSecond,
        std::size_t burstWireBytes,
        MediaRunningTime maximumLateness,
        std::size_t maximumPayloadBytes,
        std::size_t perDatagramOverheadBytes) noexcept;

    std::int64_t m_wireBytesPerSecond;
    std::size_t m_burstWireBytes;
    MediaRunningTime m_maximumLateness;
    std::size_t m_maximumPayloadBytes;
    std::size_t m_perDatagramOverheadBytes;
};

} // namespace media::ffmpeg::graph
