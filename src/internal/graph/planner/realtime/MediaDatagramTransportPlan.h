#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaDatagramTransportExecutionKind : std::uint8_t {
    Unknown = 0,
    UserspaceNonblocking = 1,
    LinuxSocketTxTime = 2
};

enum class MediaDatagramProtocolEndpointRole : std::uint8_t {
    Unknown = 0,
    VideoRtp = 1,
    VideoRtcp = 2,
    AudioRtp = 3,
    AudioRtcp = 4,
    MpegTsUdp = 5,
    MpegTsRtp = 6,
    MpegTsRtcp = 7
};

struct MediaDatagramRemoteEndpointFact final {
    std::uint64_t endpointId;
    MediaDatagramProtocolEndpointRole role;
    MediaIpAddressFamily addressFamily;
    std::string numericAddress;
    std::uint16_t port;
};

struct MediaDatagramLocalEndpointPlan final {
    std::uint64_t endpointId;
    MediaIpAddressFamily addressFamily;
    std::string numericAddress;
    std::uint16_t port;
};

struct MediaDatagramTransportPlan final {
    MediaDatagramShapingPlan shaping;
    std::vector<MediaDatagramLocalEndpointPlan> localEndpoints;
    MediaDatagramTransportExecutionKind execution;
    std::string executionAuthority;
};

struct MediaDatagramTransportPlanTemplateEncoding final {
    std::string sessionKey;
    MediaRealtimeDeploymentEnvelopeEncoding deployment;
    std::vector<MediaDatagramRemoteEndpointFact> remoteEndpoints;
};

class MediaDatagramTransportPlanTemplate final {
public:
    static ::media::Result<MediaDatagramTransportPlanTemplate> create(
        std::string sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        std::vector<MediaDatagramRemoteEndpointFact> remoteEndpoints);

    ::media::Result<MediaDatagramTransportPlan> activate(
        std::uint64_t generation) const;
    const std::string& sessionKey() const noexcept;
    const std::string& serviceScopeId() const noexcept;
    const std::vector<MediaDatagramRemoteEndpointFact>& remoteEndpoints()
        const noexcept;
    ::media::Result<std::uint64_t> endpointId(
        MediaDatagramProtocolEndpointRole role) const noexcept;

private:
    explicit MediaDatagramTransportPlanTemplate(
        MediaDatagramTransportPlanTemplateEncoding encoding) noexcept;

    MediaDatagramTransportPlanTemplateEncoding m_encoding;
};

} // namespace media::ffmpeg::graph
