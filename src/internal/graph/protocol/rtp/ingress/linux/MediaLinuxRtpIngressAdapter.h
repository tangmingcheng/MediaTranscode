#pragma once

#include "internal/graph/planner/realtime/MediaRtpReceiveGeometry.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapter.h"

#include "internal/graph/runtime/storage/MediaLineageUniquePtr.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaLinuxRtpIngressAdapter final : public MediaRtpIngressAdapter {
public:
    ~MediaLinuxRtpIngressAdapter() override;

    static ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>> create(
        std::shared_ptr<MediaRtpUdpTransport> transport,
        const MediaRtpReceiveGeometry& plan,
        MediaLineageAllocationScope allocation);

    static ::media::Result<std::size_t> preparationStorageBytes(std::size_t descriptors);

    MediaRtpIngressAdapterKind kind() const noexcept override;
    ::media::Result<std::size_t> receive(
        MediaRtpIngressStorage& storage,
        int timeoutMilliseconds) override;
    ::media::Status interruptReceive() noexcept override;
    ::media::Status stop() noexcept override;
    ::media::Status abort() noexcept override;

private:
    struct ReceiveState;
    struct ConstructionKey final {};

public:
    MediaLinuxRtpIngressAdapter(
        ConstructionKey,
        std::shared_ptr<MediaRtpUdpTransport> transport,
        MediaLineageUniquePtr<ReceiveState> receiveState,
        MediaRtpReceiveGeometryFacts planFacts) noexcept;

private:
    void logDiagnostics(const char* stage) noexcept;

    std::shared_ptr<MediaRtpUdpTransport> m_transport;
    MediaLineageUniquePtr<ReceiveState> m_receiveState;
    MediaRtpReceiveGeometryFacts m_planFacts;
};

} // namespace media::ffmpeg::graph
