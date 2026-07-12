#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"

#include <algorithm>
#include <mutex>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace media::ffmpeg::graph {

struct MediaRtpUdpTransport::Impl final {
    enum class State { Running, StopRequested, Aborted };
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
        , rtpPortValue(rtp.localPort())
        , rtcpPortValue(rtcp.localPort())
#ifdef _WIN32
        , cancellationEvent(cancelEvent)
#endif
        , maximumDatagramBytes(datagramBytes)
        , cancellableReadTimeoutMs(readTimeoutMs)
        , preferRtcp(false)
        , state(State::Running)
        , cancellationSequence(0)
        , receiveActive(false)
    {
    }

    std::shared_ptr<MediaSocketRuntime> runtime;
    MediaUdpSocket rtp;
    MediaUdpSocket rtcp;
    const uint16_t rtpPortValue;
    const uint16_t rtcpPortValue;
#ifdef _WIN32
    WSAEVENT cancellationEvent;
#endif
    std::size_t maximumDatagramBytes;
    int cancellableReadTimeoutMs;
    bool preferRtcp;
    std::mutex lifecycleMutex;
    std::mutex receiveMutex;
    State state;
    uint64_t cancellationSequence;
    bool receiveActive;
};

namespace {

::media::Result<MediaRtpUdpTransport> transportError(::media::ErrorInfo error)
{
    return ::media::Result<MediaRtpUdpTransport>::failure(std::move(error));
}

} // namespace

MediaRtpUdpTransport::MediaRtpUdpTransport() noexcept = default;
MediaRtpUdpTransport::MediaRtpUdpTransport(std::shared_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
MediaRtpUdpTransport::~MediaRtpUdpTransport() { close(); }
MediaRtpUdpTransport::MediaRtpUdpTransport(MediaRtpUdpTransport&& other) noexcept
{
    std::lock_guard lock(other.m_handleMutex);
    m_impl = std::move(other.m_impl);
}
MediaRtpUdpTransport& MediaRtpUdpTransport::operator=(MediaRtpUdpTransport&& other) noexcept
{
    if (this != &other) {
        close();
        std::scoped_lock lock(m_handleMutex, other.m_handleMutex);
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
    return ::media::Result<MediaRtpUdpTransport>::success(MediaRtpUdpTransport(std::make_shared<Impl>(
        runtime.value(), std::move(rtp.value()), std::move(rtcp.value()), cancellationEvent,
        config.maximumDatagramBytes, config.cancellableReadTimeoutMs)));
#else
    return transportError(::media::ErrorInfo::unsupported("RTP UDP event wait is currently implemented for Windows"));
#endif
}

::media::Result<MediaRtpUdpDatagram> MediaRtpUdpTransport::receive()
{
    const auto impl = snapshot();
    if (!impl) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    }
    return receive(impl->cancellableReadTimeoutMs);
}

::media::Result<MediaRtpUdpDatagram> MediaRtpUdpTransport::receive(int timeoutOverrideMs)
{
    if (timeoutOverrideMs <= 0) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::invalidArgument("RTP UDP receive timeout override must be positive"));
    }
    const auto impl = snapshot();
    if (!impl) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    }
    std::unique_lock receiveLock(impl->receiveMutex);
    if (!impl->rtp.isOpen() || !impl->rtcp.isOpen()) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    }
    uint64_t sequence = 0;
    {
        std::lock_guard lifecycleLock(impl->lifecycleMutex);
        if (impl->state != Impl::State::Running) {
            return ::media::Result<MediaRtpUdpDatagram>::failure(
                ::media::ErrorInfo::cancelled("RTP UDP transport is stopped"));
        }
        impl->receiveActive = true;
        sequence = impl->cancellationSequence;
    }
    auto finishError = [impl, sequence](::media::ErrorInfo error) {
        std::lock_guard lifecycleLock(impl->lifecycleMutex);
        if (impl->state != Impl::State::Running ||
            impl->cancellationSequence != sequence) {
            error = ::media::ErrorInfo::cancelled("RTP UDP receive was cancelled");
        }
        impl->receiveActive = false;
        return ::media::Result<MediaRtpUdpDatagram>::failure(std::move(error));
    };
#ifdef _WIN32
    MediaUdpSocket* preferred = impl->preferRtcp ? &impl->rtcp : &impl->rtp;
    MediaUdpSocket* secondary = impl->preferRtcp ? &impl->rtp : &impl->rtcp;
    const MediaRtpUdpChannel preferredChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtcp : MediaRtpUdpChannel::Rtp;
    const MediaRtpUdpChannel secondaryChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp;
    const WSAEVENT events[]{impl->cancellationEvent,
                            static_cast<WSAEVENT>(preferred->waitHandle()),
                            static_cast<WSAEVENT>(secondary->waitHandle())};
    const DWORD wait = WSAWaitForMultipleEvents(3, events, FALSE,
        static_cast<DWORD>((std::min)(timeoutOverrideMs, impl->cancellableReadTimeoutMs)), FALSE);
    if (wait == WSA_WAIT_TIMEOUT) {
        return finishError(::media::ErrorInfo::wouldBlock("RTP UDP receive timed out"));
    }
    if (wait == WSA_WAIT_FAILED) {
        return finishError(::media::ErrorInfo::ioFailure("RTP UDP event wait failed", WSAGetLastError()));
    }
    const DWORD index = wait - WSA_WAIT_EVENT_0;
    if (index == 0) {
        return finishError(::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    MediaUdpSocket* selected = index == 1 ? preferred : secondary;
    const MediaRtpUdpChannel channel = index == 1 ? preferredChannel : secondaryChannel;
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state != Impl::State::Running || impl->cancellationSequence != sequence) {
        impl->receiveActive = false;
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    auto networkEvent = selected->consumeNetworkEvent();
    if (!networkEvent) {
        impl->receiveActive = false;
        return ::media::Result<MediaRtpUdpDatagram>::failure(networkEvent.error());
    }
    auto bytes = selected->receive(impl->maximumDatagramBytes);
    if (!bytes) {
        impl->receiveActive = false;
        return ::media::Result<MediaRtpUdpDatagram>::failure(bytes.error());
    }
    impl->preferRtcp = channel == MediaRtpUdpChannel::Rtp;
    impl->receiveActive = false;
    return ::media::Result<MediaRtpUdpDatagram>::success(
        MediaRtpUdpDatagram{channel, std::move(bytes.value())});
#else
    return ::media::Result<MediaRtpUdpDatagram>::failure(
        ::media::ErrorInfo::unsupported("RTP UDP receive is unavailable"));
#endif
}

::media::Status MediaRtpUdpTransport::stop() noexcept
{
#ifdef _WIN32
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state == Impl::State::Running) {
        impl->state = Impl::State::StopRequested;
        ++impl->cancellationSequence;
    }
    if (!WSASetEvent(impl->cancellationEvent)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation signal failed", WSAGetLastError()));
    }
#endif
    return ::media::Status::success();
}

::media::Status MediaRtpUdpTransport::reset() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::lock_guard receiveLock(impl->receiveMutex);
    if (!impl->rtp.isOpen() || !impl->rtcp.isOpen()) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state == Impl::State::Aborted) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled("Aborted RTP UDP transport cannot be reset"));
    }
#ifdef _WIN32
    if (impl->state != Impl::State::StopRequested || impl->receiveActive) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP UDP reset requires stopped transport with no active receive"));
    }
    if (!WSAResetEvent(impl->cancellationEvent)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation reset failed", WSAGetLastError()));
    }
    impl->state = Impl::State::Running;
#endif
    return ::media::Status::success();
}

::media::Status MediaRtpUdpTransport::abort() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
#ifdef _WIN32
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    impl->state = Impl::State::Aborted;
    ++impl->cancellationSequence;
    if (!WSASetEvent(impl->cancellationEvent)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP abort signal failed", WSAGetLastError()));
    }
#endif
    return ::media::Status::success();
}

void MediaRtpUdpTransport::close() noexcept
{
    std::shared_ptr<Impl> impl;
    {
        std::lock_guard handleLock(m_handleMutex);
        impl = std::move(m_impl);
    }
    if (!impl) return;
#ifdef _WIN32
    {
        std::lock_guard lifecycleLock(impl->lifecycleMutex);
        impl->state = Impl::State::Aborted;
        ++impl->cancellationSequence;
        (void)WSASetEvent(impl->cancellationEvent);
    }
#endif
    std::unique_lock receiveLock(impl->receiveMutex);
    impl->rtp.close();
    impl->rtcp.close();
#ifdef _WIN32
    if (impl->cancellationEvent != WSA_INVALID_EVENT) {
        WSACloseEvent(impl->cancellationEvent);
        impl->cancellationEvent = WSA_INVALID_EVENT;
    }
#endif
}

bool MediaRtpUdpTransport::isOpen() const noexcept
{
    const auto impl = snapshot();
    if (!impl) return false;
    std::lock_guard lock(impl->receiveMutex);
    return impl->rtp.isOpen() && impl->rtcp.isOpen();
}

uint16_t MediaRtpUdpTransport::rtpPort() const noexcept
{
    const auto impl = snapshot();
    return impl ? impl->rtpPortValue : 0;
}

uint16_t MediaRtpUdpTransport::rtcpPort() const noexcept
{
    const auto impl = snapshot();
    return impl ? impl->rtcpPortValue : 0;
}

std::shared_ptr<MediaRtpUdpTransport::Impl> MediaRtpUdpTransport::snapshot() const noexcept
{
    std::lock_guard lock(m_handleMutex);
    return m_impl;
}

} // namespace media::ffmpeg::graph
