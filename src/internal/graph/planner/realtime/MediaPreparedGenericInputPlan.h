#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaPreparedLeadingVideoDisposition : std::uint8_t {
    RejectUntimed = 0,
    DiscardUntimedNonKeyBeforeFirstTimedVideo = 1
};

enum class MediaPreparedTimedStartupPrefixDisposition : std::uint8_t {
    RejectUnpairedFirstPackets = 0,
    DiscardEarlierCompleteTimedUntilCommonWindow = 1
};

struct MediaPreparedDemuxFirstPacketEvidence final {
    int streamIndex;
    MediaRational timeBase;
    std::int64_t pts;
    std::int64_t dts;
    std::int64_t duration;
    std::uint64_t preparedReadOrdinal;

    friend bool operator==(
        const MediaPreparedDemuxFirstPacketEvidence& left,
        const MediaPreparedDemuxFirstPacketEvidence& right) noexcept
    {
        return left.streamIndex == right.streamIndex &&
            left.timeBase.num == right.timeBase.num &&
            left.timeBase.den == right.timeBase.den &&
            left.pts == right.pts && left.dts == right.dts &&
            left.duration == right.duration &&
            left.preparedReadOrdinal == right.preparedReadOrdinal;
    }
};

struct MediaPreparedGenericInputPlan final {
    int videoStreamIndex;
    int audioStreamIndex;
    MediaRational videoTimeBase;
    MediaRational audioTimeBase;
    MediaPreparedLeadingVideoDisposition leadingVideoDisposition;
    MediaPreparedTimedStartupPrefixDisposition timedStartupPrefixDisposition;
    std::size_t videoPacketCapacity;
    std::size_t audioPacketCapacity;
    std::uint64_t videoByteCapacity;
    std::uint64_t audioByteCapacity;
    std::uint64_t maximumVideoPacketBytes;
    std::uint64_t maximumAudioPacketBytes;
    std::size_t handoffVideoPacketCapacity;
    std::size_t handoffAudioPacketCapacity;
    std::uint64_t handoffVideoByteCapacity;
    std::uint64_t handoffAudioByteCapacity;
    MediaRunningTime maximumPreparedReadDuration;
    MediaRunningTime maximumPreparedHandoffDuration;
    MediaRunningTime firstWindowMaximumSkew;

    ::media::Status validate() const;
    friend bool operator==(const MediaPreparedGenericInputPlan& left,
                           const MediaPreparedGenericInputPlan& right) noexcept;
};

struct MediaPreparedGenericInputEvidence final {
    MediaPreparedDemuxFirstPacketEvidence firstVideo;
    MediaPreparedDemuxFirstPacketEvidence firstAudio;
    std::size_t discardedLeadingVideoPackets;
    std::uint64_t discardedLeadingVideoBytes;
    std::size_t discardedTimedVideoPrefixPackets;
    std::uint64_t discardedTimedVideoPrefixBytes;
    std::size_t discardedTimedAudioPrefixPackets;
    std::uint64_t discardedTimedAudioPrefixBytes;

    friend bool operator==(
        const MediaPreparedGenericInputEvidence& left,
        const MediaPreparedGenericInputEvidence& right) noexcept = default;
};

struct MediaPreparedTimedPacketCandidate final {
    MediaPreparedDemuxFirstPacketEvidence packet;
    std::uint64_t payloadBytes;
};

} // namespace media::ffmpeg::graph
