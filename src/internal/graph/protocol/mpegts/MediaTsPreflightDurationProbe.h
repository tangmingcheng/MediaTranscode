#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketDurationEvidence.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace media::ffmpeg::graph {

class MediaTsDurationProbeBudget final {
public:
    static ::media::Result<MediaTsDurationProbeBudget> create(
        std::size_t selectedPacketLimit,
        std::size_t sourcePacketLimit);

    std::size_t selectedPacketLimit() const noexcept
    {
        return m_selectedPacketLimit;
    }
    std::size_t sourcePacketLimit() const noexcept
    {
        return m_sourcePacketLimit;
    }

private:
    MediaTsDurationProbeBudget(
        std::size_t selectedPacketLimit,
        std::size_t sourcePacketLimit) noexcept
        : m_selectedPacketLimit(selectedPacketLimit),
          m_sourcePacketLimit(sourcePacketLimit)
    {
    }

    std::size_t m_selectedPacketLimit;
    std::size_t m_sourcePacketLimit;
};

class MediaTsPreflightDurationProbe final {
public:
    static ::media::Result<MediaTsPreflightDurationProbe> create(
        MediaTsRuntimeBinding binding,
        MediaTsDurationProbeBudget budget);

    MediaTsPreflightDurationProbe(const MediaTsPreflightDurationProbe&) = delete;
    MediaTsPreflightDurationProbe& operator=(
        const MediaTsPreflightDurationProbe&) = delete;
    MediaTsPreflightDurationProbe(MediaTsPreflightDurationProbe&&) noexcept = default;
    MediaTsPreflightDurationProbe& operator=(
        MediaTsPreflightDurationProbe&&) noexcept = default;

    ::media::Result<std::optional<MediaTsSelectedPacketDurationEvidence>> buffer(
        MediaTsReadFrameEnvelope envelope);
    ::media::Result<MediaTsReadFrameEnvelope> popReplay();
    bool replayEmpty() const noexcept { return m_replay.empty(); }

private:
    MediaTsPreflightDurationProbe(
        MediaTsRuntimeBinding binding,
        MediaTsDurationProbeBudget budget) noexcept;

    MediaTsRuntimeBinding m_binding;
    std::size_t m_selectedPacketLimit;
    std::size_t m_sourcePacketLimit;
    std::size_t m_selectedPacketCount = 0;
    std::size_t m_sourcePacketCount = 0;
    std::deque<MediaTsReadFrameEnvelope> m_replay;
    std::optional<MediaTsPacketDurationEvidence> m_videoEvidence;
    std::optional<MediaTsPacketDurationEvidence> m_audioEvidence;
};

} // namespace media::ffmpeg::graph
