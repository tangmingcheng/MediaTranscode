#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"

#include <atomic>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace media::ffmpeg::graph {

struct MediaRtpUdpTransport::Impl final {
    Impl(std::shared_ptr<MediaSocketRuntime> socketRuntime,
         MediaUdpSocket rtpSocket,
         MediaUdpSocket rtcpSocket,
#ifdef _WIN32
         WSAEVENT cancelEvent,
#endif
         std::size_t datagramBytes,
         int readTimeoutMs)
        : runtime(std::move(socketRuntime))
        , rtp(std::move(rtpSocket))
        , rtcp(std::move(rtcpSocket))
#ifdef _WIN32
        , cancellationEvent(cancelEvent)
#endif
        , maximumDatagramBytes(datagramBytes)
        , cancellableReadTimeoutMs(readTimeoutMs)
        , preferRtcp(false)
        , aborted(false)
    {
    }

    std::shared_ptr<MediaSocketRuntime> runtime;
    MediaUdpSocket rtp;
    MediaUdpSocket rtcp;
#ifdef _WIN32
    WSAEVENT cancellationEvent;
#endif
    std::size_t maximumDatagramBytes;
    int cancellableReadTimeoutMs;
    bool preferRtcp;
    std::atomic_bool aborted;
};

namespace {

::media::Result<MediaRtpUdpTransport> transportError(::media::ErrorInfo error)
{
    return ::media::Result<MediaRtpUdpTransport>::failure(std::move(error));
}

} // namespace

MediaRtpUdpTransport::MediaRtpUdpTransport() noexcept = default;
MediaRtpUdpTransport::MediaRtpUdpTransport(std::unique_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
MediaRtpUdpTransport::~MediaRtpUdpTransport() { close(); }
MediaRtpUdpTransport::MediaRtpUdpTransport(MediaRtpUdpTransport&&) noexcept = default;
MediaRtpUdpTransport& MediaRtpUdpTransport::operator=(MediaRtpUdpTransport&& other) noexcept
{
    if (this != &other) {
        close();
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

::media::Result<MediaRtpUdpTransport> MediaRtpUdpTransport::open(
    const MediaRtpUdpTransportConfig& config)
{
    if (config.bindAddress.empty() || config.receiveBufferBytes <= 0 ||
        config.maximumDatagramBytes == 0 || config.maximumDatagramBytes > 65'535 ||
        config.cancellableReadTimeoutMs <= 0) {
        return transportError(::media::ErrorInfo::invalidArgument(
            "RTP UDP transport requires explicit address, buffers, datagram limit, and read timeout"));
    }
    if (config.rtpPort == 0 || config.rtpPort == 65'535 ||
        config.rtcpPort != static_cast<uint16_t>(config.rtpPort + 1)) {
        return transportError(::media::ErrorInfo::invalidArgument(
            "RTP and RTCP ports must be an explicit even/odd adjacent pair"));
    }
    if ((config.rtpPort % 2) != 0) {
        return transportError(::media::ErrorInfo::invalidArgument("RTP port must be even"));
    }
    auto runtime = MediaSocketRuntime::create();
    if (!runtime) return transportError(runtime.error());
    auto rtp = MediaUdpSocket::bind(runtime.value(), MediaUdpSocketConfig{
        config.addressFamily, config.bindAddress, config.rtpPort, config.receiveBufferBytes});
    if (!rtp) return transportError(rtp.error());
    auto rtcp = MediaUdpSocket::bind(runtime.value(), MediaUdpSocketConfig{
        config.addressFamily, config.bindAddress, config.rtcpPort, config.receiveBufferBytes});
    if (!rtcp) return transportError(rtcp.error());
#ifdef _WIN32
    const WSAEVENT cancellationEvent = WSACreateEvent();
    if (cancellationEvent == WSA_INVALID_EVENT) {
        return transportError(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation event creation failed", WSAGetLastError()));
    }
    return ::media::Result<MediaRtpUdpTransport>::success(MediaRtpUdpTransport(std::make_unique<Impl>(
        runtime.value(), std::move(rtp.value()), std::move(rtcp.value()), cancellationEvent,
        config.maximumDatagramBytes, config.cancellableReadTimeoutMs)));
#else
    return transportError(::media::ErrorInfo::unsupported("RTP UDP event wait is currently implemented for Windows"));
#endif
}

::media::Result<MediaRtpUdpDatagram> MediaRtpUdpTransport::receive()
{
    if (!isOpen()) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    }
    if (m_impl->aborted.load(std::memory_order_acquire)) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::cancelled("RTP UDP transport was aborted"));
    }
#ifdef _WIN32
    MediaUdpSocket* preferred = m_impl->preferRtcp ? &m_impl->rtcp : &m_impl->rtp;
    MediaUdpSocket* secondary = m_impl->preferRtcp ? &m_impl->rtp : &m_impl->rtcp;
    const MediaRtpUdpChannel preferredChannel = m_impl->preferRtcp ? MediaRtpUdpChannel::Rtcp : MediaRtpUdpChannel::Rtp;
    const MediaRtpUdpChannel secondaryChannel = m_impl->preferRtcp ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp;
    const WSAEVENT events[]{m_impl->cancellationEvent,
                            static_cast<WSAEVENT>(preferred->waitHandle()),
                            static_cast<WSAEVENT>(secondary->waitHandle())};
    const DWORD wait = WSAWaitForMultipleEvents(3, events, FALSE,
        static_cast<DWORD>(m_impl->cancellableReadTimeoutMs), FALSE);
    if (wait == WSA_WAIT_TIMEOUT) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::wouldBlock("RTP UDP receive timed out"));
    }
    if (wait == WSA_WAIT_FAILED) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::ioFailure("RTP UDP event wait failed", WSAGetLastError()));
    }
    const DWORD index = wait - WSA_WAIT_EVENT_0;
    if (index == 0) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    MediaUdpSocket* selected = index == 1 ? preferred : secondary;
    const MediaRtpUdpChannel channel = index == 1 ? preferredChannel : secondaryChannel;
    selected->consumeNetworkEvent();
    auto bytes = selected->receive(m_impl->maximumDatagramBytes);
    if (!bytes) return ::media::Result<MediaRtpUdpDatagram>::failure(bytes.error());
    m_impl->preferRtcp = channel == MediaRtpUdpChannel::Rtp;
    return ::media::Result<MediaRtpUdpDatagram>::success(
        MediaRtpUdpDatagram{channel, std::move(bytes.value())});
#else
    return ::media::Result<MediaRtpUdpDatagram>::failure(
        ::media::ErrorInfo::unsupported("RTP UDP receive is unavailable"));
#endif
}

void MediaRtpUdpTransport::stop() noexcept
{
#ifdef _WIN32
    if (m_impl) WSASetEvent(m_impl->cancellationEvent);
#endif
}

::media::Status MediaRtpUdpTransport::reset() noexcept
{
    if (!isOpen()) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    if (m_impl->aborted.load(std::memory_order_acquire)) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled("Aborted RTP UDP transport cannot be reset"));
    }
#ifdef _WIN32
    if (!WSAResetEvent(m_impl->cancellationEvent)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation reset failed", WSAGetLastError()));
    }
#endif
    return ::media::Status::success();
}

void MediaRtpUdpTransport::abort() noexcept
{
    if (m_impl) {
        m_impl->aborted.store(true, std::memory_order_release);
        stop();
    }
}

void MediaRtpUdpTransport::close() noexcept
{
    if (!m_impl) return;
    abort();
    m_impl->rtp.close();
    m_impl->rtcp.close();
#ifdef _WIN32
    if (m_impl->cancellationEvent != WSA_INVALID_EVENT) {
        WSACloseEvent(m_impl->cancellationEvent);
        m_impl->cancellationEvent = WSA_INVALID_EVENT;
    }
#endif
    m_impl.reset();
}

bool MediaRtpUdpTransport::isOpen() const noexcept
{
    return m_impl && m_impl->rtp.isOpen() && m_impl->rtcp.isOpen();
}

uint16_t MediaRtpUdpTransport::rtpPort() const noexcept
{
    return m_impl ? m_impl->rtp.localPort() : 0;
}

uint16_t MediaRtpUdpTransport::rtcpPort() const noexcept
{
    return m_impl ? m_impl->rtcp.localPort() : 0;
}

} // namespace media::ffmpeg::graph
