#pragma once

#include "internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.h"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaDatagramTransmitEndpointBinding final {
    std::uint64_t endpointId;
    MediaUdpDatagramEndpoint localEndpoint;
};

struct MediaDatagramTransmitExecutionPlan final {
    MediaDatagramTransmitExecutionMode mode;
    std::string authority;
};

class MediaDatagramTransmitSession final {
public:
    ~MediaDatagramTransmitSession() noexcept;

    MediaDatagramTransmitSession(const MediaDatagramTransmitSession&) = delete;
    MediaDatagramTransmitSession& operator=(
        const MediaDatagramTransmitSession&) = delete;

    static ::media::Result<std::unique_ptr<MediaDatagramTransmitSession>> create(
        const MediaDatagramShapingPlan& plan,
        std::vector<MediaDatagramTransmitEndpointBinding> bindings,
        MediaDatagramTransmitExecutionPlan execution,
        MediaDatagramTransmitPortFactory& portFactory);

    ::media::Result<MediaDatagramTransmitAttempt> trySubmit(
        std::uint64_t endpointId,
        std::span<const MediaDatagramTransmitRequest> requests,
        MediaRunningTime submittedAt) noexcept;
    ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        std::uint64_t endpointId,
        MediaRunningTime maximumWait) noexcept;
    ::media::Status drainAvailableEvidence(MediaRunningTime now) noexcept;
    ::media::Status abort(::media::ErrorInfo cause) noexcept;
    ::media::Status close() noexcept;

    const MediaDatagramTransmitEvidenceTelemetry& evidenceTelemetry() const noexcept;
    const MediaDatagramTransmitPortCapabilities* capabilities(
        std::uint64_t endpointId) const noexcept;
    const std::optional<::media::ErrorInfo>& terminalFailure() const noexcept
    {
        return m_terminalFailure;
    }

private:
    struct EndpointState final {
        std::uint64_t maximumDatagramBytes;
        std::uint64_t wireOverheadBytes;
        std::unique_ptr<MediaDatagramTransmitPort> port;
        MediaDatagramTransmitPortCapabilities capabilities;
    };

    MediaDatagramTransmitSession(std::string sessionKey,
                                 std::string serviceScopeId,
                                 std::uint64_t generation) noexcept;
    ::media::Status terminate(::media::ErrorInfo error) noexcept;

    std::string m_sessionKey;
    std::string m_serviceScopeId;
    std::uint64_t m_generation;
    std::uint64_t m_burstWireBytes = 0;
    std::unordered_map<std::uint64_t, EndpointState> m_endpoints;
    std::unique_ptr<MediaDatagramTransmitEvidenceCollector> m_evidence;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
