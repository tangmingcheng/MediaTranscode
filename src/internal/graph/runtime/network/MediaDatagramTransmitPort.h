#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderPort.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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
    TimedOut = 2
};

enum class MediaDatagramTransmitTimestampAvailability {
    NotRequested = 1,
    Available = 2,
    Unavailable = 3
};

struct MediaDatagramTransmitPortCapabilities final {
    std::uint64_t requestedSendBufferBytes;
    std::uint64_t effectiveSendBufferBytes;
    MediaDatagramTransmitTimestampAvailability timestampAvailability;
    bool kernelTransmitTimeAvailable;
    bool zeroCopyEnabled;
};

struct MediaDatagramTransmitEvidence final {
    std::uint64_t endpointId;
    std::uint64_t generation;
    std::uint64_t evidenceId;
    std::uint64_t platformTimestampNanoseconds;
};

struct MediaDatagramTransmitRequest final {
    std::span<const std::uint8_t> bytes;
    std::uint64_t evidenceId;
    MediaRunningTime enqueueNotAfter;
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
};

class MediaDatagramTransmitPort {
public:
    virtual ~MediaDatagramTransmitPort() = default;

    virtual ::media::Result<MediaDatagramTransmitPortCapabilities> open(
        const MediaDatagramTransmitPortOpenRequest& request) = 0;
    virtual ::media::Result<MediaDatagramTransmitAttempt> trySubmit(
        std::span<const MediaDatagramTransmitRequest> requests) = 0;
    virtual ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        MediaRunningTime maximumWait) = 0;
    virtual ::media::Result<std::vector<MediaDatagramTransmitEvidence>>
    drainAvailableEvidence() = 0;
    virtual ::media::Status close() noexcept = 0;
};

class MediaDatagramTransmitPortFactory {
public:
    virtual ~MediaDatagramTransmitPortFactory() = default;
    virtual ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>
    create() = 0;
};

} // namespace media::ffmpeg::graph
