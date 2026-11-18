#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaWireTrafficEnvelope.h"
#include "internal/graph/planner/realtime/MediaRtpProtocolStoragePlan.h"
#include "internal/graph/planner/realtime/MediaDatagramCommitStoragePlan.h"
#include "internal/graph/planner/realtime/MediaRawRtpStoragePlanner.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

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
    std::vector<MediaRtpProtocolStoragePlan> rtpStorageInstances;
    MediaDatagramCommitStoragePlan commitStorage;
    std::vector<MediaRawRtpStoragePlan> rawRtpStorage;
};

struct MediaDatagramTransportPlanTemplateEncoding final {
    std::string sessionKey;
    MediaRealtimeDeploymentEnvelopeEncoding deployment;
    std::vector<MediaDatagramRemoteEndpointFact> remoteEndpoints;
    MediaWireTrafficEnvelope wireTraffic;
};

class MediaDatagramTransportPlanTemplate final {
public:
    static ::media::Result<MediaDatagramTransportPlanTemplate> create(
        std::string sessionKey,
        const MediaRealtimeDeploymentEnvelope& deployment,
        std::vector<MediaDatagramRemoteEndpointFact> remoteEndpoints,
        MediaWireTrafficEnvelope wireTraffic);

    ::media::Result<MediaDatagramTransportPlan> activate(
        std::uint64_t generation) const;
    const std::string& sessionKey() const noexcept;
    const std::string& serviceScopeId() const noexcept;
    const std::vector<MediaDatagramRemoteEndpointFact>& remoteEndpoints()
        const noexcept;
    const MediaDatagramTransportPlanTemplateEncoding& encode() const noexcept
    {
        return m_encoding;
    }
    ::media::Result<std::uint64_t> endpointId(
        MediaDatagramProtocolEndpointRole role) const noexcept;

private:
    explicit MediaDatagramTransportPlanTemplate(
        MediaDatagramTransportPlanTemplateEncoding encoding) noexcept;

    MediaDatagramTransportPlanTemplateEncoding m_encoding;
};

} // namespace media::ffmpeg::graph
