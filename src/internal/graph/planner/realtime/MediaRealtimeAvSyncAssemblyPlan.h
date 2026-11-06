#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupPolicy.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/planner/realtime/MediaPreparedGenericInputPlan.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaInitialGenerationPolicy : std::uint8_t {
    FirstLockedOnlyFailOnChange
};

inline constexpr std::uint64_t MediaFirstLockedSourceGeneration = 1;

enum class MediaClockEvidencePolicy : std::uint8_t {
    RequireLockedFailOnDegradedOrReacquire
};

enum class MediaTerminalDurationPolicy : std::uint8_t {
    RepeatLastObservedPositiveDelta
};

struct MediaRtpInputClockAssemblyPlan final {
    MediaRtpCommonEpochPolicy commonEpochPolicy;
    friend bool operator==(const MediaRtpInputClockAssemblyPlan&,
                           const MediaRtpInputClockAssemblyPlan&) = default;
};

struct MediaMpegTsInputClockAssemblyPlan final {
    friend bool operator==(const MediaMpegTsInputClockAssemblyPlan&,
                           const MediaMpegTsInputClockAssemblyPlan&) = default;
};

struct MediaDemuxTimestampInputClockAssemblyPlan final {
    MediaRational videoTimeBase;
    MediaRational audioTimeBase;
    MediaRunningTime firstWindowMaximumSkew;
    MediaRunningTime discontinuityThreshold;
    std::uint64_t initialGeneration;
    std::string videoSourceIdentity;
    std::string audioSourceIdentity;
    MediaRunningTime canonicalTargetEpoch;
    MediaPreparedGenericInputPlan preparedInput;
    MediaPreparedGenericInputEvidence preparedEvidence;
    friend bool operator==(
        const MediaDemuxTimestampInputClockAssemblyPlan& left,
        const MediaDemuxTimestampInputClockAssemblyPlan& right)
    {
        return left.videoTimeBase.num == right.videoTimeBase.num &&
            left.videoTimeBase.den == right.videoTimeBase.den &&
            left.audioTimeBase.num == right.audioTimeBase.num &&
            left.audioTimeBase.den == right.audioTimeBase.den &&
            left.firstWindowMaximumSkew == right.firstWindowMaximumSkew &&
            left.discontinuityThreshold == right.discontinuityThreshold &&
            left.initialGeneration == right.initialGeneration &&
            left.videoSourceIdentity == right.videoSourceIdentity &&
            left.audioSourceIdentity == right.audioSourceIdentity &&
            left.canonicalTargetEpoch == right.canonicalTargetEpoch &&
            left.preparedInput == right.preparedInput &&
            left.preparedEvidence == right.preparedEvidence;
    }
};

using MediaAvSyncInputClockPlan =
    std::variant<MediaRtpInputClockAssemblyPlan,
                 MediaMpegTsInputClockAssemblyPlan,
                 MediaDemuxTimestampInputClockAssemblyPlan>;

struct MediaRtpTimestampDeltaDurationPlan final {
    int clockRate;
    MediaTerminalDurationPolicy terminalPolicy;
    friend bool operator==(const MediaRtpTimestampDeltaDurationPlan&,
                           const MediaRtpTimestampDeltaDurationPlan&) = default;
};

struct MediaPacketDurationPlan final {
    bool requirePositiveDuration;
    friend bool operator==(const MediaPacketDurationPlan&,
                           const MediaPacketDurationPlan&) = default;
};

struct MediaPlannedAudioSamplesDurationPlan final {
    int sampleRate;
    std::uint32_t samplesPerAccessUnit;
    friend bool operator==(const MediaPlannedAudioSamplesDurationPlan&,
                           const MediaPlannedAudioSamplesDurationPlan&) = default;
};

using MediaCanonicalVideoDurationPlan =
    std::variant<MediaRtpTimestampDeltaDurationPlan,
                 MediaPacketDurationPlan>;

using MediaCanonicalAudioDurationPlan =
    std::variant<MediaPacketDurationPlan,
                 MediaPlannedAudioSamplesDurationPlan>;

struct MediaCanonicalVideoAssemblyTiming final {
    MediaCanonicalVideoDurationPlan duration;
    MediaDecodeOrderMode decodeOrder;
    friend bool operator==(const MediaCanonicalVideoAssemblyTiming&,
                           const MediaCanonicalVideoAssemblyTiming&) = default;
};

// The decoder establishes ordered display points; no packet duration or DTS
// policy is implied by this startup representation.
struct MediaDecodedVideoAssemblyTiming final {
    friend bool operator==(const MediaDecodedVideoAssemblyTiming&,
                           const MediaDecodedVideoAssemblyTiming&) = default;
};
using MediaVideoInputAssemblyTiming = std::variant<
    MediaCanonicalVideoAssemblyTiming, MediaDecodedVideoAssemblyTiming>;

// Fixed declarations have no acquiring quantities and cannot become runtime
// assemblies until the planner binds actual capacity facts.
struct MediaVideoInputAssemblyFixedPlan {
    std::string sourceIdentity;
    MediaVideoInputAssemblyTiming timing;
    bool operator==(const MediaVideoInputAssemblyFixedPlan&) const = default;
};
struct MediaVideoInputAssemblyPlan final : MediaVideoInputAssemblyFixedPlan {
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
    bool operator==(const MediaVideoInputAssemblyPlan&) const = default;
};
struct MediaCanonicalAudioAssemblyFixedPlan {
    std::string sourceIdentity;
    MediaCanonicalAudioDurationPlan duration;
    MediaDecodeOrderMode decodeOrder;
    bool operator==(const MediaCanonicalAudioAssemblyFixedPlan&) const = default;
};
struct MediaCanonicalAudioAssemblyPlan final : MediaCanonicalAudioAssemblyFixedPlan {
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
    bool operator==(const MediaCanonicalAudioAssemblyPlan&) const = default;
};
struct MediaRealtimeAvSyncAssemblyFixedFields {
    MediaAvSyncInputClockPlan inputClock;
    MediaInitialGenerationPolicy generationPolicy;
    std::uint64_t initialGeneration;
    MediaClockEvidencePolicy evidencePolicy;
    bool operator==(const MediaRealtimeAvSyncAssemblyFixedFields&) const = default;
};
struct MediaRealtimeAvSyncAssemblyFixedPlan final : MediaRealtimeAvSyncAssemblyFixedFields {
    MediaVideoInputAssemblyFixedPlan video;
    std::optional<MediaCanonicalAudioAssemblyFixedPlan> audio;
    bool operator==(const MediaRealtimeAvSyncAssemblyFixedPlan&) const = default;
};
struct MediaRealtimeAvSyncAssemblyPlan final : MediaRealtimeAvSyncAssemblyFixedFields {
    MediaVideoInputAssemblyPlan video;
    std::optional<MediaCanonicalAudioAssemblyPlan> audio;
    bool operator==(const MediaRealtimeAvSyncAssemblyPlan&) const = default;
};

} // namespace media::ffmpeg::graph
