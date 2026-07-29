#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/time/MediaCanonicalTimeMapper.h"
#include "internal/graph/time/MediaMappedTimestamp.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavutil/rational.h>
}

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaDemuxTimestampClockMapperConfig final {
    MediaRational videoTimeBase;
    MediaRational audioTimeBase;
    MediaRunningTime firstWindowMaximumSkew;
    MediaRunningTime timestampRegressionLimit;
    MediaRunningTime discontinuityThreshold;
    std::uint64_t initialGeneration;
    std::string videoSourceIdentity;
    std::string audioSourceIdentity;
    MediaRunningTime canonicalTargetEpoch;

    friend bool operator==(
        const MediaDemuxTimestampClockMapperConfig& left,
        const MediaDemuxTimestampClockMapperConfig& right) noexcept
    {
        return left.videoTimeBase.num == right.videoTimeBase.num &&
            left.videoTimeBase.den == right.videoTimeBase.den &&
            left.audioTimeBase.num == right.audioTimeBase.num &&
            left.audioTimeBase.den == right.audioTimeBase.den &&
            left.firstWindowMaximumSkew == right.firstWindowMaximumSkew &&
            left.timestampRegressionLimit == right.timestampRegressionLimit &&
            left.discontinuityThreshold == right.discontinuityThreshold &&
            left.initialGeneration == right.initialGeneration &&
            left.videoSourceIdentity == right.videoSourceIdentity &&
            left.audioSourceIdentity == right.audioSourceIdentity &&
            left.canonicalTargetEpoch == right.canonicalTargetEpoch;
    }
};

struct MediaDemuxTimestampClockSnapshot final {
    MediaSourceClockReadiness readiness;
    std::uint64_t generation;
    std::uint64_t revision;
    bool hasTimestampEvidence;
    bool transitionPending;
};

class MediaDemuxTimestampClockMapper final {
public:
    static ::media::Result<std::shared_ptr<MediaDemuxTimestampClockMapper>>
    create(MediaDemuxTimestampClockMapperConfig config);

    ::media::Result<MediaMappedTimestamp> map(
        MediaScheduledStream stream,
        std::int64_t pts,
        AVRational timeBase,
        std::int64_t duration,
        std::uint64_t generation);

    ::media::Result<MediaRunningTime> projectDecodeTime(
        MediaScheduledStream stream,
        std::int64_t dts,
        AVRational timeBase,
        std::uint64_t generation) const;

    ::media::Status bindStateChangeNotifiers(
        std::function<void()> videoNotifier,
        std::function<void()> audioNotifier);
    MediaDemuxTimestampClockSnapshot snapshot() const noexcept;
    std::mutex& transactionMutex() noexcept { return m_transactionMutex; }
    ::media::Status purgeParticipant(
        MediaScheduledStream stream,
        const MediaAvGenerationPurge& purge);
    const MediaDemuxTimestampClockMapperConfig& config() const noexcept
    {
        return m_config;
    }
    ::media::Status resetLifecycle();

private:
    struct StreamState final {
        std::optional<MediaRunningTime> firstPresentation;
        std::optional<MediaRunningTime> firstDuration;
        std::optional<MediaRunningTime> latestPresentation;
        std::optional<MediaRunningTime> latestDuration;
    };

    explicit MediaDemuxTimestampClockMapper(
        MediaDemuxTimestampClockMapperConfig config) noexcept;

    static ::media::Result<MediaRunningTime> rescale(
        std::int64_t timestamp,
        AVRational timeBase,
        const char* field);
    static std::size_t streamIndex(MediaScheduledStream stream) noexcept;
    const std::string& sourceIdentity(MediaScheduledStream stream) const
        noexcept;
    const MediaRational& plannedTimeBase(MediaScheduledStream stream) const
        noexcept;
    ::media::Status validateMapInput(
        MediaScheduledStream stream,
        std::int64_t pts,
        AVRational timeBase,
        std::int64_t duration,
        std::uint64_t generation) const;
    ::media::Status establishCommonEpoch();
    ::media::Status validateContinuity(
        MediaScheduledStream stream,
        MediaRunningTime presentation,
        MediaRunningTime duration);
    ::media::Result<MediaMappedTimestamp> mapLocked(
        MediaScheduledStream stream,
        MediaRunningTime presentation,
        MediaRunningTime duration,
        std::uint64_t generation) const;
    ::media::Result<MediaRunningTime> projectLocked(
        MediaScheduledStream stream,
        MediaRunningTime sourceTime,
        std::uint64_t generation) const;
    void markStateChanged() noexcept;
    void notifyStateChange() const noexcept;
    void resetForGeneration(std::uint64_t generation) noexcept;

    const MediaDemuxTimestampClockMapperConfig m_config;
    std::mutex m_transactionMutex;
    mutable std::mutex m_mutex;
    StreamState m_streams[2];
    std::optional<MediaCanonicalTimeMapper> m_videoMapper;
    std::optional<MediaCanonicalTimeMapper> m_audioMapper;
    MediaSourceClockReadiness m_readiness =
        MediaSourceClockReadiness::Acquiring;
    std::uint64_t m_generation = 0;
    std::uint64_t m_revision = 0;
    std::optional<std::uint64_t> m_lastTransitionSequence;
    std::optional<MediaAvGenerationPurge> m_pendingPurge;
    unsigned m_pendingPurgeParticipants = 0;
    std::function<void()> m_videoNotifier;
    std::function<void()> m_audioNotifier;
    bool m_notifiersBound = false;
};

} // namespace media::ffmpeg::graph
