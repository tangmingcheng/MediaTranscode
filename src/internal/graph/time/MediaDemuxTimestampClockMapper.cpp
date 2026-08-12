#include "internal/graph/time/MediaDemuxTimestampClockMapper.h"

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

MediaDemuxTimestampOutputCommitReservation::
MediaDemuxTimestampOutputCommitReservation(
    std::unique_lock<std::mutex> transaction) noexcept
    : m_transaction(std::move(transaction))
{
}

MediaDemuxTimestampOutputCommitReservation::
MediaDemuxTimestampOutputCommitReservation(
    MediaDemuxTimestampOutputCommitReservation&&) noexcept = default;

MediaDemuxTimestampOutputCommitReservation&
MediaDemuxTimestampOutputCommitReservation::operator=(
    MediaDemuxTimestampOutputCommitReservation&&) noexcept = default;

MediaDemuxTimestampOutputCommitReservation::
~MediaDemuxTimestampOutputCommitReservation() = default;

::media::Result<std::shared_ptr<MediaDemuxTimestampClockMapper>>
MediaDemuxTimestampClockMapper::create(
    MediaDemuxTimestampClockMapperConfig config)
{
    if (config.videoTimeBase.num <= 0 || config.videoTimeBase.den <= 0 ||
        config.audioTimeBase.num <= 0 || config.audioTimeBase.den <= 0 ||
        config.firstWindowMaximumSkew <=
            MediaRunningTime::fromNanoseconds(0) ||
        config.discontinuityThreshold <=
            MediaRunningTime::fromNanoseconds(0) ||
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

::media::Status MediaDemuxTimestampClockMapper::validatePacketInput(
    MediaScheduledStream stream,
    std::int64_t pts,
    std::int64_t dts,
    AVRational timeBase,
    std::int64_t duration,
    std::uint64_t generation) const
{
    const MediaRational& planned = plannedTimeBase(stream);
    if (pts == AV_NOPTS_VALUE || dts == AV_NOPTS_VALUE || duration <= 0 ||
        timeBase.num != planned.num || timeBase.den != planned.den) {
        return ::media::Status::failure(
            invalid("Demux timestamp mapper rejects absent packet timing or unplanned time base"));
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

::media::Status MediaDemuxTimestampClockMapper::validateCommonWindow(
    const StreamState (&streams)[2]) const
{
    if (!streams[0].firstPresentation || !streams[0].firstDecode ||
        !streams[0].firstDuration || !streams[1].firstPresentation ||
        !streams[1].firstDecode || !streams[1].firstDuration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Demux timestamp mapper is acquiring the first common A/V window"));
    }
    auto skew = positiveDistance(
        *streams[0].firstPresentation,
        *streams[1].firstPresentation,
        "first-window skew");
    if (!skew) return ::media::Status::failure(skew.error());
    if (skew.value() > m_config.firstWindowMaximumSkew) {
        return ::media::Status::failure(
            invalid("Demux timestamp mapper rejects excessive first-window A/V skew"));
    }
    return ::media::Status::success();
}

::media::Status
MediaDemuxTimestampClockMapper::validateTimelineContinuity(
    MediaRunningTime current,
    MediaRunningTime latest,
    MediaRunningTime latestDuration,
    const char* field) const
{
    if (current < latest) {
        return ::media::Status::failure(
            ::media::ErrorInfo::wouldBlock(
                std::string("Demux ") + field +
                " regression is forbidden by the strict clock"));
    }
    if (current == latest) return ::media::Status::success();
    auto expectedNext = latest.checkedAdd(latestDuration);
    if (!expectedNext) {
        return ::media::Status::failure(expectedNext.error());
    }
    if (current > expectedNext.value()) {
        auto gap = current.checkedSubtract(expectedNext.value());
        if (!gap) return ::media::Status::failure(gap.error());
        if (gap.value() > m_config.discontinuityThreshold) {
            return ::media::Status::failure(
                ::media::ErrorInfo::wouldBlock(
                    std::string("Demux ") + field +
                    " discontinuity requires planned reacquisition"));
        }
    }
    return ::media::Status::success();
}

::media::Result<MediaMappedTimestamp>
MediaDemuxTimestampClockMapper::mapWith(
    const MediaCanonicalTimeMapper& mapper,
    MediaScheduledStream stream,
    MediaRunningTime presentation,
    MediaRunningTime decode,
    MediaRunningTime duration,
    std::uint64_t generation) const
{
    auto mapped = mapper.map(MediaCanonicalSourceTimestamp(
        presentation,
        decode,
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
MediaDemuxTimestampClockMapper::mapPacket(
    MediaScheduledStream stream,
    std::int64_t pts,
    std::int64_t dts,
    AVRational timeBase,
    std::int64_t duration,
    std::uint64_t generation)
{
    std::unique_lock transaction(m_transactionMutex);
    bool notify = false;
    ::media::Result<MediaMappedTimestamp> result =
        ::media::Result<MediaMappedTimestamp>::failure(
            ::media::ErrorInfo::internalError(
                "Demux timestamp packet transaction produced no result"));
    {
        std::lock_guard lock(m_mutex);
        if (auto valid = validatePacketInput(
                stream, pts, dts, timeBase, duration, generation);
            !valid) {
            return ::media::Result<MediaMappedTimestamp>::failure(
                valid.error());
        }
        auto presentation = rescale(pts, timeBase, "PTS");
        auto decode = rescale(dts, timeBase, "DTS");
        auto mappedDuration = rescale(duration, timeBase, "duration");
        if (!presentation || !decode || !mappedDuration ||
            mappedDuration.value() <= MediaRunningTime::fromNanoseconds(0)) {
            return ::media::Result<MediaMappedTimestamp>::failure(
                !presentation ? presentation.error()
                : !decode ? decode.error()
                : !mappedDuration ? mappedDuration.error()
                : invalid("Demux timestamp mapper requires positive mapped duration"));
        }
        const std::size_t index = streamIndex(stream);
        StreamState candidate = m_streams[index];
        if (m_readiness == MediaSourceClockReadiness::Acquiring) {
            if (!candidate.firstPresentation) {
                candidate.firstPresentation = presentation.value();
                candidate.firstDecode = decode.value();
                candidate.firstDuration = mappedDuration.value();
            } else if (*candidate.firstPresentation != presentation.value() ||
                       *candidate.firstDecode != decode.value() ||
                       *candidate.firstDuration != mappedDuration.value()) {
                return ::media::Result<
                    MediaMappedTimestamp>::failure(
                    invalid("Demux mapper retains exactly one complete first packet per stream"));
            }
            StreamState firstWindow[2]{m_streams[0], m_streams[1]};
            firstWindow[index] = candidate;
            auto common = validateCommonWindow(firstWindow);
            if (!common) {
                if (common.error().code ==
                    ::media::ErrorCode::NotInitialized) {
                    m_streams[index] = std::move(candidate);
                    markStateChanged();
                    notify = true;
                }
                result =
                    ::media::Result<MediaMappedTimestamp>::failure(
                        common.error());
            } else {
                const MediaRunningTime sourceEpoch = std::min(
                    *firstWindow[0].firstPresentation,
                    *firstWindow[1].firstPresentation);
                auto video = MediaCanonicalTimeMapper::create(
                    MediaCanonicalTimeMapperConfig{
                        sourceEpoch,
                        m_config.canonicalTargetEpoch,
                        MediaAvSyncSourceClockMode::DemuxTimestamps,
                        sourceIdentity(MediaScheduledStream::Video),
                        generation});
                auto audio = MediaCanonicalTimeMapper::create(
                    MediaCanonicalTimeMapperConfig{
                        sourceEpoch,
                        m_config.canonicalTargetEpoch,
                        MediaAvSyncSourceClockMode::DemuxTimestamps,
                        sourceIdentity(MediaScheduledStream::Audio),
                        generation});
                if (!video || !audio) {
                    return ::media::Result<
                        MediaMappedTimestamp>::failure(
                        !video ? video.error().toErrorInfo()
                               : audio.error().toErrorInfo());
                }
                const auto& selected =
                    stream == MediaScheduledStream::Video
                    ? video.value()
                    : audio.value();
                auto mapped = mapWith(
                    selected, stream, presentation.value(), decode.value(),
                    mappedDuration.value(), generation);
                if (!mapped) {
                    return ::media::Result<
                        MediaMappedTimestamp>::failure(
                        mapped.error());
                }
                for (StreamState& state : firstWindow) {
                    state.latestPresentation = state.firstPresentation;
                    state.latestDecode = state.firstDecode;
                    state.latestDuration = state.firstDuration;
                }
                m_streams[0] = std::move(firstWindow[0]);
                m_streams[1] = std::move(firstWindow[1]);
                m_videoMapper.emplace(std::move(video).value());
                m_audioMapper.emplace(std::move(audio).value());
                m_readiness = MediaSourceClockReadiness::Locked;
                markStateChanged();
                notify = true;
                result =
                    ::media::Result<MediaMappedTimestamp>::success(
                        std::move(mapped).value());
            }
        } else {
            if (!candidate.latestPresentation || !candidate.latestDecode ||
                !candidate.latestDuration) {
                return ::media::Result<
                    MediaMappedTimestamp>::failure(
                    ::media::ErrorInfo::internalError(
                        "Demux mapper lost its committed stream timeline"));
            }
            auto presentationContinuity = validateTimelineContinuity(
                presentation.value(),
                *candidate.latestPresentation,
                *candidate.latestDuration,
                "PTS");
            auto decodeContinuity = validateTimelineContinuity(
                decode.value(),
                *candidate.latestDecode,
                *candidate.latestDuration,
                "DTS");
            if (!presentationContinuity || !decodeContinuity) {
                requireReacquisition();
                notify = true;
                result =
                    ::media::Result<MediaMappedTimestamp>::failure(
                        !presentationContinuity
                        ? presentationContinuity.error()
                        : decodeContinuity.error());
            } else {
                const auto& mapper =
                    stream == MediaScheduledStream::Video
                    ? m_videoMapper
                    : m_audioMapper;
                if (!mapper) {
                    return ::media::Result<
                        MediaMappedTimestamp>::failure(
                        ::media::ErrorInfo::internalError(
                            "Demux mapper has no locked canonical projection"));
                }
                auto mapped = mapWith(
                    *mapper, stream, presentation.value(), decode.value(),
                    mappedDuration.value(), generation);
                if (!mapped) {
                    return ::media::Result<
                        MediaMappedTimestamp>::failure(
                        mapped.error());
                }
                candidate.latestPresentation = presentation.value();
                candidate.latestDecode = decode.value();
                candidate.latestDuration = mappedDuration.value();
                m_streams[index] = std::move(candidate);
                result =
                    ::media::Result<MediaMappedTimestamp>::success(
                        std::move(mapped).value());
            }
        }
    }
    transaction.unlock();
    if (notify) notifyStateChange();
    return result;
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

bool MediaDemuxTimestampClockMapper::outputIsCurrent(
    MediaDemuxTimestampOutputCommitEvidence evidence) const noexcept
{
    std::lock_guard lock(m_mutex);
    return !m_pendingPurge &&
        evidence.readiness == m_readiness &&
        evidence.generation == m_generation;
}

::media::Result<MediaDemuxTimestampOutputCommitReservation>
MediaDemuxTimestampClockMapper::reserveOutputCommit(
    MediaDemuxTimestampOutputCommitEvidence evidence)
{
    std::unique_lock transaction(m_transactionMutex);
    {
        std::lock_guard lock(m_mutex);
        if (m_pendingPurge ||
            evidence.readiness != m_readiness ||
            evidence.generation != m_generation) {
            return ::media::Result<
                MediaDemuxTimestampOutputCommitReservation>::failure(
                ::media::ErrorInfo::cancelled(
                    "Demux mapper rejects stale generation output"));
        }
    }
    return ::media::Result<
        MediaDemuxTimestampOutputCommitReservation>::success(
        MediaDemuxTimestampOutputCommitReservation(
            std::move(transaction)));
}

::media::Status MediaDemuxTimestampClockMapper::purgeParticipant(
    MediaScheduledStream stream,
    const MediaAvGenerationPurge& purge)
{
    std::unique_lock transaction(m_transactionMutex);
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
    transaction.unlock();
    notifyStateChange();
    return ::media::Status::success();
}

::media::Status MediaDemuxTimestampClockMapper::resetLifecycle()
{
    std::lock_guard transaction(m_transactionMutex);
    std::lock_guard lock(m_mutex);
    resetForGeneration(m_config.initialGeneration);
    m_lastTransitionSequence.reset();
    m_pendingPurge.reset();
    m_pendingPurgeParticipants = 0;
    m_revision = 0;
    return ::media::Status::success();
}

void MediaDemuxTimestampClockMapper::requireReacquisition() noexcept
{
    m_readiness = MediaSourceClockReadiness::ReacquireRequired;
    markStateChanged();
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
