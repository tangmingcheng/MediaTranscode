#pragma once

#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

enum class MediaInitialGenerationPolicy : std::uint8_t {
    FirstLockedOnlyFailOnChange
};

enum class MediaClockEvidencePolicy : std::uint8_t {
    RequireLockedFailOnDegradedOrReacquire
};

enum class MediaTerminalDurationPolicy : std::uint8_t {
    RepeatLastObservedPositiveDelta
};

enum class MediaRtpCommonEpochPolicy : std::uint8_t {
    EarliestLockedSenderReportSourceTime
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

using MediaAvSyncInputClockPlan =
    std::variant<MediaRtpInputClockAssemblyPlan,
                 MediaMpegTsInputClockAssemblyPlan>;

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

struct MediaCanonicalVideoAssemblyPlan final {
    std::string sourceIdentity;
    MediaCanonicalVideoDurationPlan duration;
    MediaDecodeOrderMode decodeOrder;
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
    friend bool operator==(const MediaCanonicalVideoAssemblyPlan&,
                           const MediaCanonicalVideoAssemblyPlan&) = default;
};

struct MediaCanonicalAudioAssemblyPlan final {
    std::string sourceIdentity;
    MediaCanonicalAudioDurationPlan duration;
    MediaDecodeOrderMode decodeOrder;
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
    friend bool operator==(const MediaCanonicalAudioAssemblyPlan&,
                           const MediaCanonicalAudioAssemblyPlan&) = default;
};

struct MediaRealtimeAvSyncAssemblyPlan final {
    MediaAvSyncInputClockPlan inputClock;
    MediaInitialGenerationPolicy generationPolicy;
    MediaClockEvidencePolicy evidencePolicy;
    MediaCanonicalVideoAssemblyPlan video;
    MediaCanonicalAudioAssemblyPlan audio;
    MediaRunningTime startupClockInterval;
    friend bool operator==(const MediaRealtimeAvSyncAssemblyPlan&,
                           const MediaRealtimeAvSyncAssemblyPlan&) = default;
};

} // namespace media::ffmpeg::graph
