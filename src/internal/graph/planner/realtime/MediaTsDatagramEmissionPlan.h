#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaTsMuxPlan;

class MediaTsDatagramEmissionPlan final {
public:
    static ::media::Result<MediaTsDatagramEmissionPlan> create(
        const MediaTsMuxPlan& muxPlan,
        MediaRunningTime videoAccessUnitCadence,
        std::optional<MediaRunningTime> audioAccessUnitCadence,
        std::uint64_t maximumQueuedBytes);

    MediaRunningTime accessUnitWindow() const noexcept;
    std::size_t packetSizeBytes() const noexcept;
    std::size_t maximumPayloadBytes() const noexcept;
    std::size_t perDatagramOverheadBytes() const noexcept;
    std::size_t maximumWireDatagramBytes() const noexcept;
    bool usesScheduledDatagramOutput() const noexcept;
    MediaRunningTime videoInitialServiceWindow() const noexcept;
    const std::optional<MediaRunningTime>&
    audioInitialServiceWindow() const noexcept;
    std::uint64_t maximumQueuedBytes() const noexcept;

    friend bool operator==(const MediaTsDatagramEmissionPlan&,
                           const MediaTsDatagramEmissionPlan&) = default;

private:
    MediaTsDatagramEmissionPlan(
        MediaRunningTime accessUnitWindow,
        MediaRunningTime videoInitialServiceWindow,
        std::optional<MediaRunningTime> audioInitialServiceWindow,
        std::size_t packetSizeBytes,
        std::size_t maximumPayloadBytes,
        std::size_t perDatagramOverheadBytes,
        bool scheduledDatagramOutput,
        std::uint64_t maximumQueuedBytes) noexcept;

    MediaRunningTime m_accessUnitWindow;
    MediaRunningTime m_videoInitialServiceWindow;
    std::optional<MediaRunningTime> m_audioInitialServiceWindow;
    std::size_t m_packetSizeBytes;
    std::size_t m_maximumPayloadBytes;
    std::size_t m_perDatagramOverheadBytes;
    bool m_scheduledDatagramOutput;
    std::uint64_t m_maximumQueuedBytes;
};

} // namespace media::ffmpeg::graph
