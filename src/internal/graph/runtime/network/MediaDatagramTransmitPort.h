#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/network/MediaUdpDatagramEndpoint.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaDatagramTransmitExecutionMode {
    Unknown = 0,
    UserspaceNonblocking = 1,
    LinuxSocketTxTime = 2
};

enum class MediaDatagramTransmitAttempt {
    Submitted = 1,
    WouldBlock = 2
};

enum class MediaDatagramWritableWaitResult {
    Writable = 1,
    TimedOut = 2,
    Stopped = 3
};

enum class MediaDatagramTransmitTimestampAvailability {
    NotRequested = 1,
    Available = 2,
    Unavailable = 3
};

enum class MediaDatagramTransmitTimestampSource {
    Unknown = 0,
    WindowsPerformanceCounter = 1,
    LinuxSoftwareRealtime = 2,
    WindowsHardwareCounter = 3
};

enum class MediaDatagramTransmitCorrelationMode {
    None = 1,
    CallerSelectedUint32 = 2,
    KernelSequentialUint32 = 3
};

enum class MediaDatagramTransmitPlatformEventKind {
    Timestamp = 1,
    TxTimeMissed = 2,
    TxTimeInvalid = 3
};

enum class MediaDatagramTransmitFailureKind {
    TerminalNoSubmit = 1,
    PartialSubmittedPrefix = 2,
    AmbiguousSubmittedPrefix = 3
};

struct MediaDatagramTransmitError final {
    ::media::ErrorInfo cause;
    MediaDatagramTransmitFailureKind kind;
    std::uint64_t submittedPrefixDatagrams;
};

struct MediaDatagramTransmitKernelSchedulePlan final {
    std::string authority;
    std::uint64_t maximumCorrelationEntries;
    std::uint64_t maximumRunDatagrams;
    MediaRunningTime maximumErrorQueueResidence;
    std::uint64_t maximumScheduleAheadNanoseconds;
};

struct MediaDatagramTransmitPortCapabilities final {
    std::uint64_t requestedSendBufferBytes;
    std::uint64_t effectiveSendBufferBytes;
    MediaDatagramTransmitTimestampAvailability timestampAvailability;
    MediaDatagramTransmitTimestampSource timestampSource;
    std::uint64_t timestampCounterFrequency;
    MediaDatagramTransmitCorrelationMode correlationMode;
    bool kernelTransmitTimeAvailable;
    bool zeroCopyEnabled;
};

struct MediaDatagramTransmitPlatformEvent final {
    std::uint64_t endpointId = 0;
    std::uint64_t generation = 0;
    MediaDatagramTransmitPlatformEventKind kind =
        MediaDatagramTransmitPlatformEventKind::Timestamp;
    std::uint32_t platformCorrelationId = 0;
    MediaDatagramTransmitTimestampSource timestampSource =
        MediaDatagramTransmitTimestampSource::Unknown;
    std::uint64_t rawTimestampCounter = 0;
    std::uint64_t rawTimestampFrequency = 0;
    std::uint32_t launchTimeLowBits = 0;
};

struct MediaDatagramTransmitPortRequest final {
    std::span<const std::uint8_t> bytes;
    std::optional<std::uint32_t> platformCorrelationId;
    std::optional<std::uint64_t> kernelTransmitTimeNanoseconds;
};

struct MediaDatagramTransmitPortOpenRequest final {
    std::string sessionKey;
    std::string serviceScopeId;
    std::uint64_t generation;
    MediaDatagramEndpointPlan endpoint;
    MediaUdpDatagramEndpoint localEndpoint;
    MediaDatagramTransmitExecutionMode executionMode;
    std::optional<MediaDatagramTransmitEvidencePlan> evidence;
    std::optional<MediaDatagramTransmitKernelSchedulePlan> kernelSchedule;
};

using MediaDatagramTransmitSubmitResult =
    ::media::Result<MediaDatagramTransmitAttempt, MediaDatagramTransmitError>;

class MediaDatagramTransmitPort {
public:
    // A port is a single-owner object. open, submit, wait, drain, and close
    // must never execute concurrently or migrate to another owner thread.
    virtual ~MediaDatagramTransmitPort() = default;

    virtual ::media::Result<MediaDatagramTransmitPortCapabilities> open(
        const MediaDatagramTransmitPortOpenRequest& request) = 0;
    virtual MediaDatagramTransmitSubmitResult trySubmit(
        std::span<const MediaDatagramTransmitPortRequest> requests) = 0;
    virtual ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        MediaRunningTime maximumWait,
        std::stop_token stopToken) = 0;
    virtual ::media::Result<std::vector<MediaDatagramTransmitPlatformEvent>>
    drainAvailableEvents(
        std::span<const std::uint32_t> outstandingTimestampIds) = 0;
    virtual ::media::Status close() noexcept = 0;
};

class MediaDatagramTransmitPortFactory {
public:
    virtual ~MediaDatagramTransmitPortFactory() = default;
    virtual ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>
    create() = 0;
};

inline MediaDatagramTransmitError mediaDatagramTransmitError(
    ::media::ErrorInfo cause,
    MediaDatagramTransmitFailureKind kind =
        MediaDatagramTransmitFailureKind::TerminalNoSubmit,
    std::uint64_t submittedPrefixDatagrams = 0)
{
    return MediaDatagramTransmitError{
        std::move(cause), kind, submittedPrefixDatagrams};
}

} // namespace media::ffmpeg::graph
