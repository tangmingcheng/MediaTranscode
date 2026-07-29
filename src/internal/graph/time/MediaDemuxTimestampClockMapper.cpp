#include "internal/graph/time/MediaDemuxTimestampClockMapper.h"

#include "internal/graph/time/MediaCanonicalTimeMapper.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

::media::Result<MediaRunningTime> positiveDistance(
    MediaRunningTime left,
    MediaRunningTime right,
    const char* field)
{
    auto distance = left >= right
        ? left.checkedSubtract(right)
        : right.checkedSubtract(left);
    if (!distance) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Demux timestamp ") + field +
                " distance is not representable"));
    }
    return distance;
}

} // namespace

::media::Result<std::shared_ptr<MediaDemuxTimestampClockMapper>>
MediaDemuxTimestampClockMapper::create(
    MediaDemuxTimestampClockMapperConfig config)
{
    if (config.videoTimeBase.num <= 0 || config.videoTimeBase.den <= 0 ||
        config.audioTimeBase.num <= 0 || config.audioTimeBase.den <= 0 ||
        config.firstWindowMaximumSkew <=
            MediaRunningTime::fromNanoseconds(0) ||
        config.timestampRegressionLimit <=
            MediaRunningTime::fromNanoseconds(0) ||
        config.discontinuityThreshold <=
            config.timestampRegressionLimit ||
        config.initialGeneration == 0 ||
        config.videoSourceIdentity.empty() ||
        config.audioSourceIdentity.empty() ||
        config.videoSourceIdentity == config.audioSourceIdentity) {
        return ::media::Result<
            std::shared_ptr<MediaDemuxTimestampClockMapper>>::failure(
            invalid("Demux timestamp mapper requires a complete positive planner policy"));
    }
    return ::media::Result<
        std::shared_ptr<MediaDemuxTimestampClockMapper>>::success(
        std::shared_ptr<MediaDemuxTimestampClockMapper>(
            new MediaDemuxTimestampClockMapper(std::move(config))));
}

MediaDemuxTimestampClockMapper::MediaDemuxTimestampClockMapper(
    MediaDemuxTimestampClockMapperConfig config) noexcept
    : m_config(std::move(config))
    , m_generation(m_config.initialGeneration)
{
}

::media::Result<MediaRunningTime>
MediaDemuxTimestampClockMapper::rescale(
    std::int64_t timestamp,
    AVRational timeBase,
    const char* field)
{
    if (timestamp == AV_NOPTS_VALUE || timeBase.num <= 0 ||
        timeBase.den <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Demux timestamp mapper requires ") + field +
                " and a positive time base"));
    }
    auto scaled = MediaRunningTime::checkedFromTicks(
        timestamp, timeBase.num, timeBase.den);
    if (!scaled) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("Demux timestamp mapper ") + field +
                " rescale is not representable"));
    }
    return scaled;
}

std::size_t MediaDemuxTimestampClockMapper::streamIndex(
    MediaScheduledStream stream) noexcept
{
    return stream == MediaScheduledStream::Video ? 0u : 1u;
}

const std::string& MediaDemuxTimestampClockMapper::sourceIdentity(
    MediaScheduledStream stream) const noexcept
{
    return stream == MediaScheduledStream::Video
        ? m_config.videoSourceIdentity
        : m_config.audioSourceIdentity;
}

const MediaRational& MediaDemuxTimestampClockMapper::plannedTimeBase(
    MediaScheduledStream stream) const noexcept
{
    return stream == MediaScheduledStream::Video
        ? m_config.videoTimeBase
        : m_config.audioTimeBase;
}

::media::Status MediaDemuxTimestampClockMapper::validateMapInput(
    MediaScheduledStream stream,
    std::int64_t pts,
    AVRational timeBase,
    std::int64_t duration,
    std::uint64_t generation) const
{
    const MediaRational& planned = plannedTimeBase(stream);
    if (pts == AV_NOPTS_VALUE || duration <= 0 ||
        timeBase.num != planned.num || timeBase.den != planned.den) {
        return ::media::Status::failure(
            invalid("Demux timestamp mapper rejects absent timestamps, duration, or unplanned time base"));
    }
    if (generation != m_generation) {
        return ::media::Status::failure(
            invalid("Demux timestamp mapper rejects an unplanned generation change"));
    }
    if (m_pendingPurge) {
        return ::media::Status::failure(
            ::media::ErrorInfo::wouldBlock(
                "Demux timestamp mapper is completing its planned generation purge"));
    }
    if (m_readiness == MediaSourceClockReadiness::ReacquireRequired) {
        return ::media::Status::failure(
            ::media::ErrorInfo::wouldBlock(
                "Demux timestamp mapper awaits planned generation purge"));
    }
    return ::media::Status::success();
}

::media::Status MediaDemuxTimestampClockMapper::establishCommonEpoch()
{
    if (!m_streams[0].firstPresentation ||
        !m_streams[1].firstPresentation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Demux timestamp mapper is acquiring the first common A/V window"));
    }
    auto skew = positiveDistance(
        *m_streams[0].firstPresentation,
        *m_streams[1].firstPresentation,
        "first-window skew");
    if (!skew) return ::media::Status::failure(skew.error());
    if (skew.value() > m_config.firstWindowMaximumSkew) {
        return ::media::Status::failure(
            invalid("Demux timestamp mapper rejects excessive first-window A/V skew"));
    }

    const MediaRunningTime sourceEpoch = std::min(
        *m_streams[0].firstPresentation,
        *m_streams[1].firstPresentation);
    auto video = MediaCanonicalTimeMapper::create(
        MediaCanonicalTimeMapperConfig{
            sourceEpoch,
            m_config.canonicalTargetEpoch,
            MediaAvSyncSourceClockMode::DemuxTimestamps,
            sourceIdentity(MediaScheduledStream::Video),
            m_generation});
    auto audio = MediaCanonicalTimeMapper::create(
        MediaCanonicalTimeMapperConfig{
            sourceEpoch,
            m_config.canonicalTargetEpoch,
            MediaAvSyncSourceClockMode::DemuxTimestamps,
            sourceIdentity(MediaScheduledStream::Audio),
            m_generation});
    if (!video || !audio) {
        return ::media::Status::failure(
            !video ? video.error().toErrorInfo()
                   : audio.error().toErrorInfo());
    }
    m_videoMapper.emplace(std::move(video).value());
    m_audioMapper.emplace(std::move(audio).value());
    for (std::size_t index = 0; index < 2; ++index) {
        m_streams[index].latestPresentation =
            m_streams[index].firstPresentation;
        m_streams[index].latestDuration = m_streams[index].firstDuration;
    }
    m_readiness = MediaSourceClockReadiness::Locked;
    markStateChanged();
    return ::media::Status::success();
}

::media::Status MediaDemuxTimestampClockMapper::validateContinuity(
    MediaScheduledStream stream,
    MediaRunningTime presentation,
    MediaRunningTime duration)
{
    StreamState& state = m_streams[streamIndex(stream)];
    if (!state.latestPresentation || !state.latestDuration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "Demux timestamp mapper lost its locked stream window"));
    }
    if (presentation < *state.latestPresentation) {
        auto regression = state.latestPresentation->checkedSubtract(
            presentation);
        if (!regression) {
            return ::media::Status::failure(regression.error());
        }
        if (regression.value() > m_config.timestampRegressionLimit) {
            m_readiness = MediaSourceClockReadiness::ReacquireRequired;
            markStateChanged();
            return ::media::Status::failure(
                ::media::ErrorInfo::wouldBlock(
                    "Demux timestamp regression requires planned reacquisition"));
        }
        return ::media::Status::success();
    }
    if (presentation == *state.latestPresentation) {
        return ::media::Status::success();
    }
    auto expectedNext = state.latestPresentation->checkedAdd(
        *state.latestDuration);
    if (!expectedNext) {
        return ::media::Status::failure(expectedNext.error());
    }
    if (presentation > expectedNext.value()) {
        auto gap = presentation.checkedSubtract(expectedNext.value());
        if (!gap) return ::media::Status::failure(gap.error());
        if (gap.value() > m_config.discontinuityThreshold) {
            m_readiness = MediaSourceClockReadiness::ReacquireRequired;
            markStateChanged();
            return ::media::Status::failure(
                ::media::ErrorInfo::wouldBlock(
                    "Demux timestamp discontinuity requires planned reacquisition"));
        }
    }
    state.latestPresentation = presentation;
    state.latestDuration = duration;
    return ::media::Status::success();
}

::media::Result<MediaMappedTimestamp>
MediaDemuxTimestampClockMapper::mapLocked(
    MediaScheduledStream stream,
    MediaRunningTime presentation,
    MediaRunningTime duration,
    std::uint64_t generation) const
{
    const auto& mapper = stream == MediaScheduledStream::Video
        ? m_videoMapper : m_audioMapper;
    if (!mapper) {
        return ::media::Result<MediaMappedTimestamp>::failure(
            ::media::ErrorInfo::notInitialized(
                "Demux timestamp mapper has no locked canonical projection"));
    }
    auto mapped = mapper->map(MediaCanonicalSourceTimestamp(
        presentation,
        std::nullopt,
        duration,
        generation,
        sourceIdentity(stream),
        MediaTimeMappingConfidence::Locked));
    return mapped
        ? ::media::Result<MediaMappedTimestamp>::success(
              std::move(mapped).value())
        : ::media::Result<MediaMappedTimestamp>::failure(
              mapped.error().toErrorInfo());
}

::media::Result<MediaMappedTimestamp>
MediaDemuxTimestampClockMapper::map(
    MediaScheduledStream stream,
    std::int64_t pts,
    AVRational timeBase,
    std::int64_t duration,
    std::uint64_t generation)
{
    bool notify = false;
    ::media::Result<MediaMappedTimestamp> result =
        ::media::Result<MediaMappedTimestamp>::failure(
            ::media::ErrorInfo::internalError(
                "Demux timestamp mapper did not produce a result"));
    {
        std::lock_guard lock(m_mutex);
        if (auto valid = validateMapInput(
                stream, pts, timeBase, duration, generation); !valid) {
            return ::media::Result<MediaMappedTimestamp>::failure(
                valid.error());
        }
        auto presentation = rescale(pts, timeBase, "PTS");
        auto mappedDuration = rescale(duration, timeBase, "duration");
        if (!presentation || !mappedDuration ||
            mappedDuration.value() <= MediaRunningTime::fromNanoseconds(0)) {
            return ::media::Result<MediaMappedTimestamp>::failure(
                !presentation ? presentation.error()
                : !mappedDuration ? mappedDuration.error()
                : invalid("Demux timestamp mapper requires positive mapped duration"));
        }

        StreamState& state = m_streams[streamIndex(stream)];
        if (m_readiness == MediaSourceClockReadiness::Acquiring) {
            if (!state.firstPresentation) {
                state.firstPresentation = presentation.value();
                state.firstDuration = mappedDuration.value();
                markStateChanged();
                notify = true;
            } else if (*state.firstPresentation != presentation.value() ||
                       *state.firstDuration != mappedDuration.value()) {
                return ::media::Result<MediaMappedTimestamp>::failure(
                    invalid("Demux timestamp mapper retains exactly one first-window packet per stream"));
            }
            auto locked = establishCommonEpoch();
            if (!locked) {
                result = ::media::Result<MediaMappedTimestamp>::failure(
                    locked.error());
            } else {
                notify = true;
                result = mapLocked(
                    stream, presentation.value(), mappedDuration.value(),
                    generation);
            }
        } else {
            auto continuity = validateContinuity(
                stream, presentation.value(), mappedDuration.value());
            if (!continuity) {
                notify =
                    m_readiness ==
                    MediaSourceClockReadiness::ReacquireRequired;
                result = ::media::Result<MediaMappedTimestamp>::failure(
                    continuity.error());
            } else {
                result = mapLocked(
                    stream, presentation.value(), mappedDuration.value(),
                    generation);
            }
        }
    }
    if (notify) notifyStateChange();
    return result;
}

::media::Result<MediaRunningTime>
MediaDemuxTimestampClockMapper::projectLocked(
    MediaScheduledStream stream,
    MediaRunningTime sourceTime,
    std::uint64_t generation) const
{
    const auto& mapper = stream == MediaScheduledStream::Video
        ? m_videoMapper : m_audioMapper;
    if (!mapper) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::notInitialized(
                "Demux decode-time projection requires a locked common epoch"));
    }
    auto mapped = mapper->map(MediaCanonicalSourceTimestamp(
        sourceTime,
        std::nullopt,
        std::nullopt,
        generation,
        sourceIdentity(stream),
        MediaTimeMappingConfidence::Locked));
    return mapped
        ? ::media::Result<MediaRunningTime>::success(
              mapped.value().presentationTime())
        : ::media::Result<MediaRunningTime>::failure(
              mapped.error().toErrorInfo());
}

::media::Result<MediaRunningTime>
MediaDemuxTimestampClockMapper::projectDecodeTime(
    MediaScheduledStream stream,
    std::int64_t dts,
    AVRational timeBase,
    std::uint64_t generation) const
{
    std::lock_guard lock(m_mutex);
    const MediaRational& planned = plannedTimeBase(stream);
    if (m_readiness != MediaSourceClockReadiness::Locked ||
        generation != m_generation || dts == AV_NOPTS_VALUE ||
        timeBase.num != planned.num || timeBase.den != planned.den) {
        return ::media::Result<MediaRunningTime>::failure(
            invalid("Demux decode-time projection requires locked planned evidence"));
    }
    auto source = rescale(dts, timeBase, "DTS");
    if (!source) return source;
    return projectLocked(stream, source.value(), generation);
}

::media::Status
MediaDemuxTimestampClockMapper::bindStateChangeNotifiers(
    std::function<void()> videoNotifier,
    std::function<void()> audioNotifier)
{
    std::lock_guard lock(m_mutex);
    if (m_notifiersBound || !videoNotifier || !audioNotifier) {
        return ::media::Status::failure(
            invalid("Demux timestamp mapper binds both stream wakeups exactly once"));
    }
    m_videoNotifier = std::move(videoNotifier);
    m_audioNotifier = std::move(audioNotifier);
    m_notifiersBound = true;
    return ::media::Status::success();
}

MediaDemuxTimestampClockSnapshot
MediaDemuxTimestampClockMapper::snapshot() const noexcept
{
    std::lock_guard lock(m_mutex);
    return MediaDemuxTimestampClockSnapshot{
        m_readiness,
        m_generation,
        m_revision,
        m_streams[0].firstPresentation.has_value() ||
            m_streams[1].firstPresentation.has_value(),
        m_pendingPurge.has_value()};
}

::media::Status MediaDemuxTimestampClockMapper::purgeParticipant(
    MediaScheduledStream stream,
    const MediaAvGenerationPurge& purge)
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_pendingPurge) {
            if ((m_readiness != MediaSourceClockReadiness::Locked &&
                 m_readiness !=
                     MediaSourceClockReadiness::ReacquireRequired) ||
                purge.oldGeneration != m_generation ||
                purge.nextGeneration <= purge.oldGeneration ||
                purge.transitionSequence == 0 ||
                (m_lastTransitionSequence &&
                 purge.transitionSequence <= *m_lastTransitionSequence)) {
                return ::media::Status::failure(
                    invalid("Demux timestamp mapper purge requires its exact pending transition"));
            }
            m_pendingPurge = purge;
        } else if (m_pendingPurge->oldGeneration != purge.oldGeneration ||
                   m_pendingPurge->nextGeneration != purge.nextGeneration ||
                   m_pendingPurge->transitionSequence !=
                       purge.transitionSequence) {
            return ::media::Status::failure(
                invalid("Demux timestamp mapper rejects mismatched participant purge facts"));
        }
        const unsigned participant =
            stream == MediaScheduledStream::Video ? 1u : 2u;
        if ((m_pendingPurgeParticipants & participant) != 0) {
            return ::media::Status::failure(
                invalid("Demux timestamp mapper rejects duplicate participant purge"));
        }
        m_pendingPurgeParticipants |= participant;
        if (m_pendingPurgeParticipants == 3u) {
            m_lastTransitionSequence = purge.transitionSequence;
            resetForGeneration(purge.nextGeneration);
            m_pendingPurge.reset();
            m_pendingPurgeParticipants = 0;
        }
        markStateChanged();
    }
    notifyStateChange();
    return ::media::Status::success();
}

::media::Status MediaDemuxTimestampClockMapper::resetLifecycle()
{
    {
        std::lock_guard lock(m_mutex);
        resetForGeneration(m_config.initialGeneration);
        m_lastTransitionSequence.reset();
        m_pendingPurge.reset();
        m_pendingPurgeParticipants = 0;
        m_revision = 0;
    }
    return ::media::Status::success();
}

void MediaDemuxTimestampClockMapper::markStateChanged() noexcept
{
    if (m_revision != std::numeric_limits<std::uint64_t>::max()) {
        ++m_revision;
    }
}

void MediaDemuxTimestampClockMapper::notifyStateChange() const noexcept
{
    if (m_videoNotifier) m_videoNotifier();
    if (m_audioNotifier) m_audioNotifier();
}

void MediaDemuxTimestampClockMapper::resetForGeneration(
    std::uint64_t generation) noexcept
{
    m_streams[0] = StreamState{};
    m_streams[1] = StreamState{};
    m_videoMapper.reset();
    m_audioMapper.reset();
    m_readiness = MediaSourceClockReadiness::Acquiring;
    m_generation = generation;
}

} // namespace media::ffmpeg::graph
