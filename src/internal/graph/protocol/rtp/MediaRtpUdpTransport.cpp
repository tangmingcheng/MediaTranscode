#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace media::ffmpeg::graph {

struct MediaRtpUdpTransport::Impl final {
    enum class State { Running, StopRequested, Aborted };
    Impl(std::shared_ptr<MediaSocketRuntime> socketRuntime,
         MediaUdpSocket rtpSocket,
         MediaUdpSocket rtcpSocket,
#ifdef _WIN32
         WSAEVENT cancelEvent,
#else
         int cancelReadFd,
         int cancelWriteFd,
#endif
         std::size_t datagramBytes,
         int readTimeoutMs,
         std::shared_ptr<MediaRtpUdpTransportPhaseController> controller)
        : runtime(std::move(socketRuntime))
        , rtp(std::move(rtpSocket))
        , rtcp(std::move(rtcpSocket))
        , rtpPortValue(rtp.localPort())
        , rtcpPortValue(rtcp.localPort())
#ifdef _WIN32
        , cancellationEvent(cancelEvent)
#else
        , cancellationRead(cancelReadFd)
        , cancellationWrite(cancelWriteFd)
#endif
        , maximumDatagramBytes(datagramBytes)
        , cancellableReadTimeoutMs(readTimeoutMs)
        , phaseController(std::move(controller))
        , preferRtcp(false)
        , state(State::Running)
        , cancellationSequence(0)
        , receiveActive(false)
    {
    }

    ~Impl()
    {
#ifdef _WIN32
        if (cancellationEvent != WSA_INVALID_EVENT) {
            WSACloseEvent(cancellationEvent);
        }
#else
        if (cancellationRead >= 0) ::close(cancellationRead);
        if (cancellationWrite >= 0) ::close(cancellationWrite);
#endif
    }

    std::shared_ptr<MediaSocketRuntime> runtime;
    MediaUdpSocket rtp;
    MediaUdpSocket rtcp;
    const uint16_t rtpPortValue;
    const uint16_t rtcpPortValue;
#ifdef _WIN32
    const WSAEVENT cancellationEvent;
#else
    const int cancellationRead;
    const int cancellationWrite;
#endif
    std::size_t maximumDatagramBytes;
    int cancellableReadTimeoutMs;
    std::shared_ptr<MediaRtpUdpTransportPhaseController> phaseController;
    bool preferRtcp;
    std::mutex lifecycleMutex;
    std::mutex receiveMutex;
    std::atomic<State> state;
    std::atomic_uint64_t cancellationSequence;
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
        config.maximumDatagramBytes, config.cancellableReadTimeoutMs,
        config.phaseController)));
#else
    int cancellationPipe[2]{-1, -1};
    if (pipe(cancellationPipe) != 0) {
        return transportError(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation pipe creation failed", errno));
    }
    const int readFlags = fcntl(cancellationPipe[0], F_GETFL, 0);
    const int writeFlags = fcntl(cancellationPipe[1], F_GETFL, 0);
    if (readFlags < 0 || writeFlags < 0 ||
        fcntl(cancellationPipe[0], F_SETFL, readFlags | O_NONBLOCK) != 0 ||
        fcntl(cancellationPipe[1], F_SETFL, writeFlags | O_NONBLOCK) != 0) {
        const int error = errno;
        ::close(cancellationPipe[0]);
        ::close(cancellationPipe[1]);
        return transportError(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation pipe configuration failed", error));
    }
    return ::media::Result<MediaRtpUdpTransport>::success(MediaRtpUdpTransport(std::make_shared<Impl>(
        runtime.value(), std::move(rtp.value()), std::move(rtcp.value()),
        cancellationPipe[0], cancellationPipe[1], config.maximumDatagramBytes,
        config.cancellableReadTimeoutMs, config.phaseController)));
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
        if (impl->state.load(std::memory_order_acquire) !=
            Impl::State::Running) {
            return ::media::Result<MediaRtpUdpDatagram>::failure(
                ::media::ErrorInfo::cancelled("RTP UDP transport is stopped"));
        }
        impl->receiveActive = true;
        sequence = impl->cancellationSequence.load(
            std::memory_order_acquire);
    }
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::ReceiveProtected);
    }
    auto finishError = [impl, sequence](::media::ErrorInfo error) {
        std::lock_guard lifecycleLock(impl->lifecycleMutex);
        if (impl->state.load(std::memory_order_acquire) !=
                Impl::State::Running ||
            impl->cancellationSequence.load(std::memory_order_acquire) !=
                sequence) {
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
                            reinterpret_cast<WSAEVENT>(preferred->waitHandle()),
                            reinterpret_cast<WSAEVENT>(secondary->waitHandle())};
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
    if (impl->state.load(std::memory_order_acquire) !=
            Impl::State::Running ||
        impl->cancellationSequence.load(std::memory_order_acquire) !=
            sequence) {
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
    MediaUdpSocket* preferred = impl->preferRtcp ? &impl->rtcp : &impl->rtp;
    MediaUdpSocket* secondary = impl->preferRtcp ? &impl->rtp : &impl->rtcp;
    const MediaRtpUdpChannel preferredChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtcp : MediaRtpUdpChannel::Rtp;
    const MediaRtpUdpChannel secondaryChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp;
    pollfd descriptors[]{
        {impl->cancellationRead, POLLIN, 0},
        {static_cast<int>(preferred->waitHandle()), POLLIN, 0},
        {static_cast<int>(secondary->waitHandle()), POLLIN, 0}};
    const int wait = poll(descriptors, 3,
        (std::min)(timeoutOverrideMs, impl->cancellableReadTimeoutMs));
    if (wait == 0) {
        return finishError(::media::ErrorInfo::wouldBlock("RTP UDP receive timed out"));
    }
    if (wait < 0) {
        return finishError(::media::ErrorInfo::ioFailure("RTP UDP poll failed", errno));
    }
    if ((descriptors[0].revents & POLLIN) != 0) {
        return finishError(::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    const short socketFailure = POLLERR | POLLHUP | POLLNVAL;
    if ((descriptors[1].revents & socketFailure) != 0 ||
        (descriptors[2].revents & socketFailure) != 0) {
        return finishError(::media::ErrorInfo::ioFailure("RTP UDP socket poll failed", EIO));
    }
    const bool preferredReadable = (descriptors[1].revents & POLLIN) != 0;
    const bool secondaryReadable = (descriptors[2].revents & POLLIN) != 0;
    if (!preferredReadable && !secondaryReadable) {
        return finishError(::media::ErrorInfo::wouldBlock("RTP UDP poll had no readable datagram"));
    }
    MediaUdpSocket* selected = preferredReadable ? preferred : secondary;
    const MediaRtpUdpChannel channel = preferredReadable ? preferredChannel : secondaryChannel;
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state.load(std::memory_order_acquire) != Impl::State::Running ||
        impl->cancellationSequence.load(std::memory_order_acquire) != sequence) {
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
#endif
}

::media::Status MediaRtpUdpTransport::interruptReceive() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    Impl::State expected = Impl::State::Running;
    if (impl->state.compare_exchange_strong(
            expected, Impl::State::StopRequested,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        impl->cancellationSequence.fetch_add(
            1, std::memory_order_acq_rel);
    } else if (expected == Impl::State::Aborted) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled(
                "RTP UDP transport was aborted during receive interruption"));
    }
    return signalCancellation(impl);
}

::media::Status MediaRtpUdpTransport::stop() noexcept
{
    const auto impl = snapshot();
    if (!impl) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP UDP transport is closed"));
    }
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::StopLifetimeAcquired);
    }
    return interruptReceive();
}

::media::Status MediaRtpUdpTransport::reset() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::lock_guard receiveLock(impl->receiveMutex);
    if (!impl->rtp.isOpen() || !impl->rtcp.isOpen()) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state.load(std::memory_order_acquire) == Impl::State::Aborted) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled("Aborted RTP UDP transport cannot be reset"));
    }
    if (impl->state.load(std::memory_order_acquire) !=
            Impl::State::StopRequested ||
        impl->receiveActive) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP UDP reset requires stopped transport with no active receive"));
    }
    if (auto reset = resetCancellation(impl); !reset) return reset;
    impl->state.store(Impl::State::Running, std::memory_order_release);
    return ::media::Status::success();
}

::media::Status MediaRtpUdpTransport::abort() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::AbortLifetimeAcquired);
    }
    if (impl->state.exchange(
            Impl::State::Aborted, std::memory_order_acq_rel) ==
        Impl::State::Aborted) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled("RTP UDP transport was closed during abort"));
    }
    impl->cancellationSequence.fetch_add(1, std::memory_order_acq_rel);
    return signalCancellation(impl);
}

void MediaRtpUdpTransport::close() noexcept
{
    std::shared_ptr<Impl> impl;
    {
        std::lock_guard handleLock(m_handleMutex);
        impl = std::move(m_impl);
    }
    if (!impl) return;
    impl->state.store(Impl::State::Aborted, std::memory_order_release);
    impl->cancellationSequence.fetch_add(1, std::memory_order_acq_rel);
    (void)signalCancellation(impl);
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::CloseReceiveWait);
    }
    std::unique_lock receiveLock(impl->receiveMutex);
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::CloseReceiveAcquired);
    }
    impl->rtp.close();
    impl->rtcp.close();
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

::media::Status MediaRtpUdpTransport::signalCancellation(
    const std::shared_ptr<Impl>& impl) noexcept
{
#ifdef _WIN32
    if (!WSASetEvent(impl->cancellationEvent)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation signal failed", WSAGetLastError()));
    }
#else
    const uint8_t signal = 1;
    const ssize_t written = write(impl->cancellationWrite, &signal, sizeof(signal));
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation signal failed", errno));
    }
#endif
    return ::media::Status::success();
}

::media::Status MediaRtpUdpTransport::resetCancellation(
    const std::shared_ptr<Impl>& impl) noexcept
{
#ifdef _WIN32
    if (!WSAResetEvent(impl->cancellationEvent)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation reset failed", WSAGetLastError()));
    }
#else
    uint8_t buffer[64];
    while (read(impl->cancellationRead, buffer, sizeof(buffer)) > 0) {
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation reset failed", errno));
    }
#endif
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
