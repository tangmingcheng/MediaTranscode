#pragma once

#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"

#include <memory>
#include <vector>

namespace media::ffmpeg::graph {

class MediaDatagramTransportPlanBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        const MediaDatagramTransportPlanTemplate& planTemplate,
        std::uint64_t generation);

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::DatagramTransportPlan;
    }
    const MediaDatagramTransportPlan& plan() const noexcept { return m_plan; }
    const std::shared_ptr<MediaWireGlobalSequenceState>& globalSequence()
        const noexcept
    {
        return m_globalSequence;
    }
    ::media::Result<std::uint64_t> endpointId(
        MediaDatagramProtocolEndpointRole role) const noexcept;

private:
    MediaDatagramTransportPlanBuffer(
        MediaDatagramTransportPlan plan,
        std::shared_ptr<MediaWireGlobalSequenceState> globalSequence,
        std::vector<MediaDatagramRemoteEndpointFact> endpoints) noexcept;

    const MediaDatagramTransportPlan m_plan;
    const std::shared_ptr<MediaWireGlobalSequenceState> m_globalSequence;
    const std::vector<MediaDatagramRemoteEndpointFact> m_endpoints;
};

} // namespace media::ffmpeg::graph
