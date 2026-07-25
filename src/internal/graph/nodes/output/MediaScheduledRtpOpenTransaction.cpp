#include "internal/graph/nodes/output/MediaScheduledRtpOpenTransaction.h"

#include <cstdint>
#include <span>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class OpenTransportRollback final {
public:
    explicit OpenTransportRollback(
        MediaRtpUdpSenderTransport& transport) noexcept
        : m_transport(transport)
    {
    }

    ~OpenTransportRollback()
    {
        if (m_armed) (void)m_transport.close();
    }

    OpenTransportRollback(const OpenTransportRollback&) = delete;
    OpenTransportRollback& operator=(const OpenTransportRollback&) = delete;

    void commit() noexcept { m_armed = false; }

private:
    MediaRtpUdpSenderTransport& m_transport;
    bool m_armed = true;
};

} // namespace

MediaOpenedScheduledRtpSender::MediaOpenedScheduledRtpSender(
    std::unique_ptr<MediaRtpUdpSenderTransport> transport,
    std::unique_ptr<ScheduledRtpSenderSession> sender) noexcept
    : m_transport(std::move(transport)), m_sender(std::move(sender))
{
}

std::unique_ptr<MediaRtpUdpSenderTransport>
MediaOpenedScheduledRtpSender::releaseTransport() noexcept
{
    return std::move(m_transport);
}

std::unique_ptr<ScheduledRtpSenderSession>
MediaOpenedScheduledRtpSender::releaseSender() noexcept
{
    return std::move(m_sender);
}

::media::Result<MediaOpenedScheduledRtpSender>
MediaScheduledRtpOpenTransaction::open(
    MediaRtpUdpSenderConfig transportConfig,
    ScheduledRtpSenderConfig senderConfig,
    MediaUdpDatagramSenderPortFactory& transportFactory,
    ScheduledRtpPacketizerFactory& packetizerFactory)
{
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(transportConfig), transportFactory);
    if (!transport) {
        return ::media::Result<MediaOpenedScheduledRtpSender>::failure(
            transport.error());
    }
    MediaRtpUdpSenderTransport* transportView = transport.value().get();
    auto sender = ScheduledRtpSenderSession::create(
        std::move(senderConfig),
        [transportView](
            std::span<const std::uint8_t> datagram,
            std::size_t) {
            return transportView->sendRtp(datagram);
        },
        [transportView](std::span<const std::uint8_t> datagram) {
            return transportView->sendRtcp(datagram);
        },
        packetizerFactory);
    if (!sender) {
        return ::media::Result<MediaOpenedScheduledRtpSender>::failure(
            sender.error());
    }
    auto transportOpened = transport.value()->open();
    if (!transportOpened) {
        return ::media::Result<MediaOpenedScheduledRtpSender>::failure(
            transportOpened.error());
    }
    OpenTransportRollback rollback(*transport.value());
    auto senderOpened = sender.value()->open();
    if (!senderOpened) {
        return ::media::Result<MediaOpenedScheduledRtpSender>::failure(
            senderOpened.error());
    }
    rollback.commit();
    return ::media::Result<MediaOpenedScheduledRtpSender>::success(
        MediaOpenedScheduledRtpSender(
            std::move(transport).value(), std::move(sender).value()));
}

} // namespace media::ffmpeg::graph
