#pragma once

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"

#include <cstdint>
#include <vector>

namespace media_transcode::test {

struct MediaTsObservedPesTimestamp final {
    std::uint16_t pid;
    std::uint64_t pts;
    std::uint64_t dts;
};

class MediaTsPesTimestampInspector final
    : public media::ffmpeg::graph::MediaTsPacketSink {
public:
    ::media::Status onPacket(
        const media::ffmpeg::graph::MediaTsPacketView& packet) override;
    ::media::Status onContinuityEvent(
        const media::ffmpeg::graph::MediaTsContinuityEvent& event) override;

    const std::vector<MediaTsObservedPesTimestamp>& timestamps() const noexcept
    {
        return m_timestamps;
    }
    const std::vector<std::uint64_t>& pcrValues() const noexcept
    {
        return m_pcrValues;
    }
    std::size_t continuityEventCount() const noexcept
    {
        return m_continuityEvents;
    }

private:
    std::vector<MediaTsObservedPesTimestamp> m_timestamps;
    std::vector<std::uint64_t> m_pcrValues;
    std::size_t m_continuityEvents = 0;
};

} // namespace media_transcode::test
