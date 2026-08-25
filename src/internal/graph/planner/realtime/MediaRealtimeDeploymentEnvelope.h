#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/model/MediaIpAddressFamily.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeDeploymentServiceScope final {
    MediaDatagramServiceScopeKind kind = MediaDatagramServiceScopeKind::Unknown;
    std::string scopeId;
    std::string coverageAuthority;
    friend bool operator==(const MediaRealtimeDeploymentServiceScope&,
                           const MediaRealtimeDeploymentServiceScope&) = default;
};

struct MediaRealtimeDeploymentResourceBudget final {
    std::uint64_t maximumGraphMemoryBytes = 0;
    std::uint64_t maximumNetworkMemoryBytes = 0;
    std::uint64_t maximumSocketMemoryBytes = 0;
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentResourceBudget&,
                           const MediaRealtimeDeploymentResourceBudget&) = default;
};

struct MediaRealtimeDeploymentLocalPortRange final {
    MediaIpAddressFamily addressFamily = MediaIpAddressFamily::Ipv4;
    std::string numericAddress;
    std::uint16_t firstPort = 0;
    std::uint16_t portCount = 0;
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentLocalPortRange&,
                           const MediaRealtimeDeploymentLocalPortRange&) = default;
};

enum class MediaRealtimeTransmitEvidencePolicy : std::uint8_t {
    Unknown = 0,
    Disabled = 1,
    Report = 2,
    Fail = 3
};

struct MediaRealtimeDeploymentLatencyBudget final {
    MediaRunningTime targetResidence = MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime maximumResidence = MediaRunningTime::fromNanoseconds(0);
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentLatencyBudget&,
                           const MediaRealtimeDeploymentLatencyBudget&) = default;
};

struct MediaRealtimeDeploymentObservationBudget final {
    std::uint64_t maximumRunDatagrams = 0;
    MediaRunningTime maximumDrainResidence = MediaRunningTime::fromNanoseconds(0);
    MediaRealtimeTransmitEvidencePolicy evidencePolicy =
        MediaRealtimeTransmitEvidencePolicy::Unknown;
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentObservationBudget&,
                           const MediaRealtimeDeploymentObservationBudget&) = default;
};

struct MediaRealtimeDeploymentMtuFact final {
    MediaIpAddressFamily addressFamily = MediaIpAddressFamily::Ipv4;
    std::string authority;
    std::uint64_t maximumIpPacketBytes = 0;
    std::uint64_t senderMaximumPayloadBytes = 0;
    friend bool operator==(const MediaRealtimeDeploymentMtuFact&,
                           const MediaRealtimeDeploymentMtuFact&) = default;
};

struct MediaRealtimeDeploymentManagedServiceFact final {
    std::uint64_t sustainedWireBytesPerSecond = 0;
    std::uint64_t peakWireBytesPerSecond = 0;
    std::uint64_t burstWireBytes = 0;
    std::string authority;
    friend bool operator==(const MediaRealtimeDeploymentManagedServiceFact&,
                           const MediaRealtimeDeploymentManagedServiceFact&) = default;
};

struct MediaRealtimeReceiverTimingCapability final {
    MediaRunningTime transportDecodeLead =
        MediaRunningTime::fromNanoseconds(0);
    std::string authority;
    friend bool operator==(const MediaRealtimeReceiverTimingCapability&,
                           const MediaRealtimeReceiverTimingCapability&) = default;
};

struct MediaRealtimeDeploymentEnvelopeEncoding final {
    MediaRealtimeDeploymentServiceScope serviceScope;
    MediaRealtimeDeploymentMtuFact mtu;
    MediaRealtimeDeploymentManagedServiceFact service;
    MediaRealtimeDeploymentResourceBudget resources;
    MediaRealtimeDeploymentLocalPortRange localPorts;
    MediaRealtimeDeploymentLatencyBudget latency;
    MediaRealtimeDeploymentObservationBudget observation;
    std::optional<MediaRealtimeReceiverTimingCapability> receiverTiming;
    friend bool operator==(const MediaRealtimeDeploymentEnvelopeEncoding&,
                           const MediaRealtimeDeploymentEnvelopeEncoding&) = default;
};

class MediaRealtimeDeploymentEnvelope final {
public:
    static ::media::Result<MediaRealtimeDeploymentEnvelope> decode(
        MediaRealtimeDeploymentEnvelopeEncoding encoding);

    const MediaRealtimeDeploymentEnvelopeEncoding& encode() const noexcept
    {
        return m_encoding;
    }

private:
    explicit MediaRealtimeDeploymentEnvelope(
        MediaRealtimeDeploymentEnvelopeEncoding encoding) noexcept;
    MediaRealtimeDeploymentEnvelopeEncoding m_encoding;
};

} // namespace media::ffmpeg::graph
