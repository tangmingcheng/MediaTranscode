#include "internal/graph/nodes/audio/AudioEncoderPacketLineageMapper.h"

#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status AudioEncoderPacketLineageMapper::submit(
    std::int64_t framePts,
    int frameSamples,
    std::vector<MediaAudioIntervalFragment> fragments)
{
    if (framePts < 0 || frameSamples <= 0 ||
        framePts > std::numeric_limits<std::int64_t>::max() - frameSamples ||
        (m_nextSubmittedPts && framePts != *m_nextSubmittedPts)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio encoder submitted lineage requires a contiguous frame timeline"));
    }

    std::int64_t lineageSamples = 0;
    auto candidate = m_intervals;
    for (auto& fragment : fragments) {
        const auto samples = fragment.interval.sampleCount();
        if (!samples || *samples >
                std::numeric_limits<std::int64_t>::max() - lineageSamples) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Audio encoder submitted lineage sample count overflows"));
        }
        lineageSamples += *samples;
        if (auto status = candidate.push(std::move(fragment)); !status) {
            return status;
        }
    }
    if (lineageSamples != frameSamples) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio encoder submitted lineage must exactly cover the frame"));
    }

    if (!m_nextPacketPts) m_nextPacketPts = framePts;
    m_nextSubmittedPts = framePts + frameSamples;
    m_intervals = std::move(candidate);
    return ::media::Status::success();
}

::media::Result<std::optional<std::vector<MediaAudioIntervalFragment>>>
AudioEncoderPacketLineageMapper::map(
    std::int64_t packetPts,
    std::int64_t packetDuration)
{
    using Mapping = std::optional<std::vector<MediaAudioIntervalFragment>>;
    if (!m_nextPacketPts || packetPts == AV_NOPTS_VALUE || packetDuration <= 0 ||
        packetPts > std::numeric_limits<std::int64_t>::max() - packetDuration) {
        return ::media::Result<Mapping>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio encoder packet lineage requires explicit valid timing"));
    }

    const auto packetEnd = packetPts + packetDuration;
    if (packetPts < *m_nextPacketPts) {
        if (packetEnd > *m_nextPacketPts ||
            (m_nextPrimingPts && packetPts != *m_nextPrimingPts)) {
            return ::media::Result<Mapping>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Audio encoder priming packet timeline is invalid"));
        }
        m_nextPrimingPts = packetEnd;
        return ::media::Result<Mapping>::success(std::nullopt);
    }
    if (packetPts != *m_nextPacketPts ||
        (m_nextPrimingPts && *m_nextPrimingPts != *m_nextPacketPts) ||
        packetDuration > std::numeric_limits<int>::max()) {
        return ::media::Result<Mapping>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio encoder packet timeline is discontinuous"));
    }

    auto candidate = m_intervals;
    auto fragments = candidate.take(static_cast<int>(packetDuration));
    if (!fragments) {
        return ::media::Result<Mapping>::failure(fragments.error());
    }
    m_intervals = std::move(candidate);
    m_nextPrimingPts.reset();
    m_nextPacketPts = packetEnd;
    return ::media::Result<Mapping>::success(
        Mapping(std::move(fragments).value()));
}

::media::Status AudioEncoderPacketLineageMapper::observeLineageCapacity(
    MediaAudioLineageCapacity& capacity) const
{
    return m_intervals.observeLineageCapacity(capacity);
}

::media::Status AudioEncoderPacketLineageMapper::finish() const
{
    return m_intervals.finish();
}

void AudioEncoderPacketLineageMapper::reset() noexcept
{
    m_intervals.reset();
    m_nextSubmittedPts.reset();
    m_nextPacketPts.reset();
    m_nextPrimingPts.reset();
}

} // namespace media::ffmpeg::graph
