#include "internal/graph/sync/lineage/MediaAudioSampleProjection.h"

extern "C" {
#include <libavutil/mathematics.h>
}

#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::int64_t> checkedNonNegativeAccumulate(
    std::int64_t left, std::int64_t right, const char* message)
{
    if (left < 0 || right <= 0 ||
        left > std::numeric_limits<std::int64_t>::max() - right) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(message));
    }
    return ::media::Result<std::int64_t>::success(left + right);
}

::media::Result<std::int64_t> checkedForwardAdd(
    std::int64_t left, std::int64_t right, const char* message)
{
    if (right <= 0 ||
        left > std::numeric_limits<std::int64_t>::max() - right) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(message));
    }
    return ::media::Result<std::int64_t>::success(left + right);
}

} // namespace

::media::Result<MediaAudioSampleProjection>
MediaAudioSampleProjection::create(
    std::int64_t outputStart, int sourceSampleRate, int outputSampleRate)
{
    if (sourceSampleRate <= 0 || outputSampleRate <= 0) {
        return ::media::Result<MediaAudioSampleProjection>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio sample projection requires positive rates"));
    }
    return ::media::Result<MediaAudioSampleProjection>::success(
        MediaAudioSampleProjection(
            outputStart, sourceSampleRate, outputSampleRate));
}

::media::Result<MediaCanonicalAudioSampleInterval>
MediaAudioSampleProjection::append(std::int64_t sourceSamples)
{
    auto cumulative = checkedNonNegativeAccumulate(
        m_sourceSamples, sourceSamples,
        "Audio sample projection source accumulation overflows");
    if (!cumulative) {
        return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
            cumulative.error());
    }
    const std::int64_t projected = av_rescale_q_rnd(
        cumulative.value(), AVRational{1, m_sourceSampleRate},
        AVRational{1, m_outputSampleRate}, AV_ROUND_NEAR_INF);
    if (projected == std::numeric_limits<std::int64_t>::min()) {
        return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio sample projection rate conversion overflows"));
    }
    auto end = checkedForwardAdd(
        m_outputStart, projected,
        "Audio sample projection output boundary overflows");
    if (!end || end.value() <= m_outputEnd) {
        return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
            end ? ::media::ErrorInfo::invalidArgument(
                      "Audio sample projection cannot advance output boundary")
                : end.error());
    }
    const MediaCanonicalAudioSampleInterval interval{
        m_outputEnd, end.value(), m_outputSampleRate};
    m_sourceSamples = cumulative.value();
    m_outputEnd = end.value();
    return ::media::Result<MediaCanonicalAudioSampleInterval>::success(interval);
}

::media::Result<MediaCanonicalAudioSampleInterval>
MediaAudioSampleProjection::extend(std::int64_t outputSamples)
{
    auto end = checkedForwardAdd(
        m_outputEnd, outputSamples,
        "Audio sample projection correction extension overflows");
    if (!end) {
        return ::media::Result<MediaCanonicalAudioSampleInterval>::failure(
            end.error());
    }
    const MediaCanonicalAudioSampleInterval interval{
        m_outputEnd, end.value(), m_outputSampleRate};
    m_outputEnd = end.value();
    return ::media::Result<MediaCanonicalAudioSampleInterval>::success(interval);
}

std::int64_t MediaAudioSampleProjection::sourceSamples() const noexcept
{
    return m_sourceSamples;
}

std::int64_t MediaAudioSampleProjection::outputEnd() const noexcept
{
    return m_outputEnd;
}

int MediaAudioSampleProjection::sourceSampleRate() const noexcept
{
    return m_sourceSampleRate;
}

int MediaAudioSampleProjection::outputSampleRate() const noexcept
{
    return m_outputSampleRate;
}

MediaAudioSampleProjection::MediaAudioSampleProjection(
    std::int64_t outputStart, int sourceSampleRate, int outputSampleRate) noexcept
    : m_outputStart(outputStart)
    , m_outputEnd(outputStart)
    , m_sourceSampleRate(sourceSampleRate)
    , m_outputSampleRate(outputSampleRate)
{
}

} // namespace media::ffmpeg::graph
