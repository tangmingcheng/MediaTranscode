#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaRtpUdpBoundLocalEndpoints::MediaRtpUdpBoundLocalEndpoints(
    MediaUdpDatagramEndpoint rtp,
    MediaUdpDatagramEndpoint rtcp) noexcept
    : m_rtp(std::move(rtp)), m_rtcp(std::move(rtcp))
{
}

MediaRtpUdpSenderTransport::OperationGuard::OperationGuard(
    MediaRtpUdpSenderTransport& owner)
    : m_owner(owner), m_entered(false)
{
    std::unique_lock lock(m_owner.m_mutex, std::try_to_lock);
    if (!lock.owns_lock() || m_owner.m_operationActive || m_owner.m_closeActive) {
        return;
    }
    m_owner.m_operationActive = true;
    m_owner.m_operationThread = std::this_thread::get_id();
    m_entered = true;
}

MediaRtpUdpSenderTransport::OperationGuard::~OperationGuard()
{
    if (!m_entered) return;
    {
        std::lock_guard lock(m_owner.m_mutex);
        m_owner.m_operationActive = false;
        m_owner.m_operationThread = std::thread::id{};
    }
    m_owner.m_operationFinished.notify_all();
}

MediaRtpUdpSenderTransport::MediaRtpUdpSenderTransport(
    MediaRtpUdpSenderConfig config,
    std::unique_ptr<MediaUdpDatagramSenderPort> rtpPort,
    std::unique_ptr<MediaUdpDatagramSenderPort> rtcpPort) noexcept
    : m_config(std::move(config)),
      m_rtpPort(std::move(rtpPort)),
      m_rtcpPort(std::move(rtcpPort))
{
}

MediaRtpUdpSenderTransport::~MediaRtpUdpSenderTransport() noexcept
{
    m_rtpPort->close();
    m_rtcpPort->close();
}

::media::Result<std::unique_ptr<MediaRtpUdpSenderTransport>>
MediaRtpUdpSenderTransport::create(
    MediaRtpUdpSenderConfig config,
    MediaUdpDatagramSenderPortFactory& portFactory)
{
    using TransportResult =
        ::media::Result<std::unique_ptr<MediaRtpUdpSenderTransport>>;
    try {
        auto rtp = portFactory.create();
        if (!rtp) return TransportResult::failure(rtp.error());
        if (!rtp.value()) {
            return TransportResult::failure(::media::ErrorInfo::internalError(
                "UDP sender port factory returned no RTP port"));
        }
        auto rtcp = portFactory.create();
        if (!rtcp) return TransportResult::failure(rtcp.error());
        if (!rtcp.value()) {
            return TransportResult::failure(::media::ErrorInfo::internalError(
                "UDP sender port factory returned no RTCP port"));
        }
        auto transport = std::unique_ptr<MediaRtpUdpSenderTransport>(
            new (std::nothrow) MediaRtpUdpSenderTransport(
                std::move(config), std::move(rtp.value()), std::move(rtcp.value())));
        if (!transport) {
            return TransportResult::failure(
                ::media::ErrorInfo::allocationFailed("MediaRtpUdpSenderTransport"));
        }
        return TransportResult::success(std::move(transport));
    } catch (const std::bad_alloc&) {
        return TransportResult::failure(::media::ErrorInfo::allocationFailed(
            "UDP sender port factory operation"));
    } catch (...) {
        return TransportResult::failure(::media::ErrorInfo::internalError(
            "UDP sender port factory threw while creating unopened ports"));
    }
}

::media::Status MediaRtpUdpSenderTransport::open()
{
    OperationGuard guard(*this);
    if (!guard.entered()) return operationRejected();
    {
        std::lock_guard lock(m_mutex);
        if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
        if (m_state != MediaRtpUdpSenderTransportState::Created) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "RTP UDP sender transport can be opened exactly once"));
        }
    }
    const auto failOpen = [this](::media::ErrorInfo error) {
        poisonAndClose(error);
        return ::media::Status::failure(std::move(error));
    };
    try {
        auto rtpRequest = makeOpenRequest(true);
        if (!rtpRequest) return failOpen(rtpRequest.error());
        auto rtcpRequest = makeOpenRequest(false);
        if (!rtcpRequest) return failOpen(rtcpRequest.error());
        auto opened = m_rtpPort->open(rtpRequest.value());
        if (!opened) return failOpen(opened.error());
        opened = m_rtcpPort->open(rtcpRequest.value());
        if (!opened) return failOpen(opened.error());
        auto rtpLocal = m_rtpPort->localEndpoint();
        auto rtcpLocal = m_rtcpPort->localEndpoint();
        auto rtpRemote = m_rtpPort->remoteEndpoint();
        auto rtcpRemote = m_rtcpPort->remoteEndpoint();
        if (!rtpLocal || !rtcpLocal || !rtpRemote || !rtcpRemote ||
            *rtpRemote != m_config.m_remoteRtpEndpoint ||
            *rtcpRemote != m_config.m_remoteRtcpEndpoint) {
            return failOpen(::media::ErrorInfo::internalError(
                "UDP sender ports did not publish the planned endpoints"));
        }
        auto valid = validateBoundEndpoints(*rtpLocal, *rtcpLocal);
        if (!valid) return failOpen(valid.error());
        {
            std::lock_guard lock(m_mutex);
            m_boundLocalEndpoints = MediaRtpUdpBoundLocalEndpoints(
                std::move(*rtpLocal), std::move(*rtcpLocal));
            m_state = MediaRtpUdpSenderTransportState::Open;
        }
        return ::media::Status::success();
    } catch (const std::bad_alloc&) {
        return failOpen(::media::ErrorInfo::allocationFailed(
            "RTP UDP sender open transaction"));
    } catch (...) {
        return failOpen(::media::ErrorInfo::internalError(
            "UDP sender port threw during the open transaction"));
    }
}

::media::Status MediaRtpUdpSenderTransport::sendRtp(
    std::span<const std::uint8_t> datagram)
{
    return send(*m_rtpPort, datagram);
}

::media::Status MediaRtpUdpSenderTransport::sendRtcp(
    std::span<const std::uint8_t> datagram)
{
    return send(*m_rtcpPort, datagram);
}

::media::Status MediaRtpUdpSenderTransport::send(
    MediaUdpDatagramSenderPort& port,
    std::span<const std::uint8_t> datagram)
{
    OperationGuard guard(*this);
    if (!guard.entered()) return operationRejected();
    {
        std::lock_guard lock(m_mutex);
        if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
        if (m_state != MediaRtpUdpSenderTransportState::Open) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "RTP UDP sender transport is not open"));
        }
    }
    if (datagram.empty() || datagram.size() > m_config.m_maximumDatagramBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "UDP datagram is empty or exceeds the planned maximum"));
    }
    const auto outcome = [&]() -> MediaUdpDatagramSendOutcome {
        try {
            return port.send(datagram);
        } catch (...) {
            const auto cause = ::media::ErrorInfo::internalError(
                "UDP sender port threw with unknown delivery certainty");
            poisonAndClose(cause);
            throw MediaUdpAmbiguousDeliveryError(cause, 0);
        }
    }();
    if (outcome.kind() == MediaUdpDatagramSendKind::Accepted &&
        outcome.acceptedBytes() == datagram.size()) {
        return ::media::Status::success();
    }
    if (outcome.kind() == MediaUdpDatagramSendKind::NotAccepted &&
        outcome.acceptedBytes() == 0 && outcome.error()) {
        return ::media::Status::failure(*outcome.error());
    }
    const auto cause = outcome.error()
        ? *outcome.error()
        : ::media::ErrorInfo::internalError(
              "UDP sender port returned an inconsistent delivery outcome");
    const auto acceptedBytes = outcome.acceptedBytes();
    poisonAndClose(cause);
    throw MediaUdpAmbiguousDeliveryError(cause, acceptedBytes);
}

::media::Status MediaRtpUdpSenderTransport::close() noexcept
{
    try {
        std::unique_lock lock(m_mutex);
        const auto caller = std::this_thread::get_id();
        for (;;) {
            while (m_closeActive) {
                if (m_closeThread == caller) return operationRejected();
                m_operationFinished.wait(lock);
            }
            if (!m_operationActive) break;
            if (m_operationThread == caller) return operationRejected();
            m_operationFinished.wait(lock);
        }
        if (m_state == MediaRtpUdpSenderTransportState::Closed) {
            return ::media::Status::success();
        }
        const bool poisoned = m_state == MediaRtpUdpSenderTransportState::Poisoned;
        m_closeActive = true;
        m_closeThread = caller;
        m_boundLocalEndpoints.reset();
        lock.unlock();

        m_rtpPort->close();
        m_rtcpPort->close();

        lock.lock();
        if (!poisoned) m_state = MediaRtpUdpSenderTransportState::Closed;
        m_closeActive = false;
        m_closeThread = std::thread::id{};
        lock.unlock();
        m_operationFinished.notify_all();
        return ::media::Status::success();
    } catch (...) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "RTP UDP sender transport close synchronization failed"));
    }
}

MediaRtpUdpSenderTransportState MediaRtpUdpSenderTransport::state() const
{
    std::lock_guard lock(m_mutex);
    return m_state;
}

std::optional<MediaRtpUdpBoundLocalEndpoints>
MediaRtpUdpSenderTransport::boundLocalEndpoints() const
{
    std::lock_guard lock(m_mutex);
    return m_boundLocalEndpoints;
}

MediaUdpDatagramEndpoint MediaRtpUdpSenderTransport::remoteRtpEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_config.m_remoteRtpEndpoint;
}

MediaUdpDatagramEndpoint MediaRtpUdpSenderTransport::remoteRtcpEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_config.m_remoteRtcpEndpoint;
}

::media::Status MediaRtpUdpSenderTransport::operationRejected() const
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "RTP UDP sender transport operation is concurrent or reentrant"));
}

void MediaRtpUdpSenderTransport::poisonAndClose(::media::ErrorInfo cause) noexcept
{
    m_rtpPort->close();
    m_rtcpPort->close();
    try {
        std::lock_guard lock(m_mutex);
        m_boundLocalEndpoints.reset();
        m_state = MediaRtpUdpSenderTransportState::Poisoned;
        if (!m_terminalFailure) m_terminalFailure = std::move(cause);
    } catch (...) {
    }
}

::media::Status MediaRtpUdpSenderTransport::validateBoundEndpoints(
    const MediaUdpDatagramEndpoint& rtp,
    const MediaUdpDatagramEndpoint& rtcp) const
{
    if (rtp.addressFamily() != m_config.m_addressFamily ||
        rtcp.addressFamily() != m_config.m_addressFamily ||
        rtp.numericAddress() != m_config.m_localNumericAddress ||
        rtcp.numericAddress() != m_config.m_localNumericAddress) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "UDP sender bound endpoint differs from the planned address"));
    }
    if (m_config.m_localPortPolicy.kind() ==
        MediaRtpUdpLocalPortPolicyKind::FixedAdjacent) {
        if (rtp.port() != *m_config.m_localPortPolicy.rtpPort() ||
            rtcp.port() != *m_config.m_localPortPolicy.rtcpPort()) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "UDP sender did not preserve the fixed local port pair"));
        }
    } else if (rtp.port() == 0 || rtcp.port() == 0 || rtp.port() == rtcp.port()) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "OS-assigned UDP sender ports must be nonzero and distinct"));
    }
    return ::media::Status::success();
}

::media::Result<MediaUdpDatagramSenderPortOpenRequest>
MediaRtpUdpSenderTransport::makeOpenRequest(bool rtp) const
{
    const std::uint16_t localPort =
        m_config.m_localPortPolicy.kind() ==
                MediaRtpUdpLocalPortPolicyKind::FixedAdjacent
            ? *(rtp ? m_config.m_localPortPolicy.rtpPort()
                    : m_config.m_localPortPolicy.rtcpPort())
            : 0;
    auto local = MediaUdpDatagramEndpoint::create(
        m_config.m_addressFamily, m_config.m_localNumericAddress, localPort);
    if (!local) {
        return ::media::Result<MediaUdpDatagramSenderPortOpenRequest>::failure(
            local.error());
    }
    return MediaUdpDatagramSenderPortOpenRequest::create(
        std::move(local.value()),
        rtp ? m_config.m_remoteRtpEndpoint : m_config.m_remoteRtcpEndpoint,
        m_config.m_sendBufferBytes,
        m_config.m_maximumDatagramBytes,
        m_config.m_ioBehavior);
}

} // namespace media::ffmpeg::graph
