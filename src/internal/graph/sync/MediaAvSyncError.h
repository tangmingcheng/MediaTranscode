#pragma once

#include "internal/graph/sync/MediaAvSyncTopology.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

enum class MediaAvSyncErrorCode {
    MissingSourceEvidence,
    EmptySourceIdentity,
    SourceIdentityMismatch,
    GenerationMismatch,
    InvalidGenerationTransition,
    InvalidDuration,
    TimeOverflow,
    StartupTimeout,
    KeyFrameTimeout,
    EofBeforeRelease,
    MissingCanonicalTime,
    AudioTrimLimitExceeded,
    StartupCapacityExceeded,
    StartupInvalidTransition,
    StartupAborted,
    StartupUpstreamError,
    InvalidAudioServoPolicy,
    InvalidAudioServoMeasurement,
    AudioCorrectionQuantizationFailed,
    InvalidVideoSyncPolicy,
    InvalidVideoSyncMeasurement
};

enum class MediaAvSyncErrorState {
    Configuring,
    Mapping,
    Resetting,
    Startup,
    AudioServo,
    VideoSync
};

enum class MediaLockedPacketGateDisposition : std::uint8_t {
    Pass = 0,
    WithholdForReacquisition = 1,
    DropOldGeneration = 2
};

class MediaAvSyncError final {
public:
    MediaAvSyncError(MediaAvSyncErrorCode code,
                     MediaAvSyncTopology topology,
                     MediaAvSyncErrorState state,
                     std::string operation,
                     std::string expectedStreamIdentity,
                     std::string observedStreamIdentity,
                     std::optional<std::uint64_t> expectedGeneration,
                     std::optional<std::uint64_t> observedGeneration,
                     std::optional<MediaRunningTime> observedSourceTime,
                     MediaRunningTime sourceEpoch,
                     MediaRunningTime runningTimeEpoch,
                     std::int64_t minimumRunningTimeNs,
                     std::int64_t maximumRunningTimeNs,
                     std::string detail)
        : m_code(code)
        , m_topology(topology)
        , m_state(state)
        , m_operation(std::move(operation))
        , m_expectedStreamIdentity(std::move(expectedStreamIdentity))
        , m_observedStreamIdentity(std::move(observedStreamIdentity))
        , m_expectedGeneration(expectedGeneration)
        , m_observedGeneration(observedGeneration)
        , m_observedSourceTime(observedSourceTime)
        , m_sourceEpoch(sourceEpoch)
        , m_runningTimeEpoch(runningTimeEpoch)
        , m_minimumRunningTimeNs(minimumRunningTimeNs)
        , m_maximumRunningTimeNs(maximumRunningTimeNs)
        , m_detail(std::move(detail))
    {
    }

    MediaAvSyncErrorCode code() const noexcept { return m_code; }
    MediaAvSyncTopology topology() const noexcept { return m_topology; }
    MediaAvSyncErrorState state() const noexcept { return m_state; }
    const std::string& operation() const noexcept { return m_operation; }
    const std::string& expectedStreamIdentity() const noexcept
    {
        return m_expectedStreamIdentity;
    }
    const std::string& observedStreamIdentity() const noexcept
    {
        return m_observedStreamIdentity;
    }
    std::optional<std::uint64_t> expectedGeneration() const noexcept
    {
        return m_expectedGeneration;
    }
    std::optional<std::uint64_t> observedGeneration() const noexcept
    {
        return m_observedGeneration;
    }
    const std::optional<MediaRunningTime>& observedSourceTime() const noexcept
    {
        return m_observedSourceTime;
    }
    MediaRunningTime sourceEpoch() const noexcept { return m_sourceEpoch; }
    MediaRunningTime runningTimeEpoch() const noexcept { return m_runningTimeEpoch; }
    std::int64_t minimumRunningTimeNs() const noexcept { return m_minimumRunningTimeNs; }
    std::int64_t maximumRunningTimeNs() const noexcept { return m_maximumRunningTimeNs; }
    const std::string& detail() const noexcept { return m_detail; }

    ::media::ErrorInfo toErrorInfo() const
    {
        std::string message = "A/V sync code=" +
                              std::to_string(static_cast<int>(m_code));
        message += " operation=" + m_operation;
        message += " topology=" + std::to_string(static_cast<int>(m_topology));
        message += " state=" + std::to_string(static_cast<int>(m_state));
        message += " expected_stream=" + m_expectedStreamIdentity;
        message += " observed_stream=" + m_observedStreamIdentity;
        if (m_expectedGeneration) {
            message += " expected_generation=" + std::to_string(*m_expectedGeneration);
        }
        if (m_observedGeneration) {
            message += " observed_generation=" + std::to_string(*m_observedGeneration);
        }
        if (m_observedSourceTime) {
            message += " observed_source_ns=" +
                       std::to_string(m_observedSourceTime->nanoseconds());
        }
        message += " source_epoch_ns=" + std::to_string(m_sourceEpoch.nanoseconds());
        message += " running_epoch_ns=" +
                   std::to_string(m_runningTimeEpoch.nanoseconds());
        message += " running_bounds_ns=[" + std::to_string(m_minimumRunningTimeNs) +
                   "," + std::to_string(m_maximumRunningTimeNs) + "]";
        message += " detail=" + m_detail;

        if (m_code == MediaAvSyncErrorCode::MissingSourceEvidence) {
            return ::media::ErrorInfo::notInitialized(std::move(message));
        }
        return ::media::ErrorInfo::invalidArgument(std::move(message));
    }

private:
    MediaAvSyncErrorCode m_code;
    MediaAvSyncTopology m_topology;
    MediaAvSyncErrorState m_state;
    std::string m_operation;
    std::string m_expectedStreamIdentity;
    std::string m_observedStreamIdentity;
    std::optional<std::uint64_t> m_expectedGeneration;
    std::optional<std::uint64_t> m_observedGeneration;
    std::optional<MediaRunningTime> m_observedSourceTime;
    MediaRunningTime m_sourceEpoch;
    MediaRunningTime m_runningTimeEpoch;
    std::int64_t m_minimumRunningTimeNs;
    std::int64_t m_maximumRunningTimeNs;
    std::string m_detail;
};

template <typename T>
using MediaAvSyncResult = ::media::Result<T, MediaAvSyncError>;

using MediaAvSyncStatus = MediaAvSyncResult<void>;

} // namespace media::ffmpeg::graph
