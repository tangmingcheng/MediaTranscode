#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapter.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaWindowsRtpIngressAdapter final : public MediaRtpIngressAdapter {
public:
    ~MediaWindowsRtpIngressAdapter() override;

    static ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>> create(
        MediaRtpUdpTransport transport,
        const MediaRtpIngressPlan& plan);

    MediaRtpIngressAdapterKind kind() const noexcept override;
    ::media::Result<std::size_t> receive(
        MediaRtpIngressStorage& storage,
        int timeoutMilliseconds) override;
    ::media::Status interruptReceive() noexcept override;
    ::media::Status stop() noexcept override;
    ::media::Status abort() noexcept override;

private:
    struct ReceiveState;

    MediaWindowsRtpIngressAdapter(
        MediaRtpUdpTransport transport,
        std::unique_ptr<ReceiveState> receiveState,
        MediaRtpIngressPlanFacts planFacts) noexcept;
    ::media::Status terminate(bool aborting) noexcept;
    void logDiagnostics(const char* stage) noexcept;

    MediaRtpUdpTransport m_transport;
    std::unique_ptr<ReceiveState> m_receiveState;
    MediaRtpIngressPlanFacts m_planFacts;
};

} // namespace media::ffmpeg::graph
