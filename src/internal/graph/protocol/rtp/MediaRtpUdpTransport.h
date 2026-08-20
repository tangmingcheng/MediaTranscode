#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpChannel.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransportPhaseController.h"
#include "internal/graph/runtime/network/MediaUdpSocket.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtpUdpTransportConfig final {
    MediaIpAddressFamily addressFamily;
    std::string bindAddress;
    uint16_t rtpPort;
    uint16_t rtcpPort;
    int receiveBufferBytes;
    std::size_t maximumDatagramBytes;
    int cancellableReadTimeoutMs;
    std::shared_ptr<MediaRtpUdpTransportPhaseController> phaseController;
};

struct MediaRtpUdpDatagram final {
    MediaRtpUdpChannel channel;
    std::vector<uint8_t> bytes;
};

class MediaRtpUdpIngressReceiveLease final {
public:
    MediaRtpUdpIngressReceiveLease() = delete;
    ~MediaRtpUdpIngressReceiveLease();
    MediaRtpUdpIngressReceiveLease(
        const MediaRtpUdpIngressReceiveLease&) = delete;
    MediaRtpUdpIngressReceiveLease& operator=(
        const MediaRtpUdpIngressReceiveLease&) = delete;
    MediaRtpUdpIngressReceiveLease(
        MediaRtpUdpIngressReceiveLease&& other) noexcept;
    MediaRtpUdpIngressReceiveLease& operator=(
        MediaRtpUdpIngressReceiveLease&& other) noexcept;

    intptr_t rtpHandle() const noexcept;
    intptr_t rtcpHandle() const noexcept;
    intptr_t cancellationHandle() const noexcept;
    int timeoutMilliseconds() const noexcept;
    MediaRtpUdpChannel preferredChannel() const noexcept;
    bool cancelled() const noexcept;
    void markReceived(MediaRtpUdpChannel channel) noexcept;

private:
    friend class MediaRtpUdpTransport;
    using CancelledFn = bool (*)(void*, std::uint64_t) noexcept;
    using MarkReceivedFn = void (*)(void*, MediaRtpUdpChannel) noexcept;
    using ReleaseFn = void (*)(void*) noexcept;

    MediaRtpUdpIngressReceiveLease(
        std::shared_ptr<void> owner,
        std::unique_lock<std::mutex> receiveLock,
        void* state,
        intptr_t rtpHandle,
        intptr_t rtcpHandle,
        intptr_t cancellationHandle,
        int timeoutMilliseconds,
        MediaRtpUdpChannel preferredChannel,
        std::uint64_t cancellationSequence,
        CancelledFn cancelledFn,
        MarkReceivedFn markReceivedFn,
        ReleaseFn releaseFn) noexcept;
    void release() noexcept;

    std::shared_ptr<void> m_owner;
    std::unique_lock<std::mutex> m_receiveLock;
    void* m_state = nullptr;
    intptr_t m_rtpHandle = -1;
    intptr_t m_rtcpHandle = -1;
    intptr_t m_cancellationHandle = -1;
    int m_timeoutMilliseconds = 0;
    MediaRtpUdpChannel m_preferredChannel = MediaRtpUdpChannel::Rtp;
    std::uint64_t m_cancellationSequence = 0;
    CancelledFn m_cancelledFn = nullptr;
    MarkReceivedFn m_markReceivedFn = nullptr;
    ReleaseFn m_releaseFn = nullptr;
};

class MediaRtpUdpTransport final {
public:
    MediaRtpUdpTransport() noexcept;
    ~MediaRtpUdpTransport();
    MediaRtpUdpTransport(MediaRtpUdpTransport&&) noexcept;
    MediaRtpUdpTransport& operator=(MediaRtpUdpTransport&&) noexcept;

    MediaRtpUdpTransport(const MediaRtpUdpTransport&) = delete;
    MediaRtpUdpTransport& operator=(const MediaRtpUdpTransport&) = delete;

    static ::media::Result<MediaRtpUdpTransport> open(const MediaRtpUdpTransportConfig& config);

    ::media::Result<MediaRtpUdpDatagram> receive();
    ::media::Result<MediaRtpUdpDatagram> receive(int timeoutOverrideMs);
    ::media::Status interruptReceive() noexcept;
    ::media::Status stop() noexcept;
    ::media::Status reset() noexcept;
    ::media::Status abort() noexcept;
    void close() noexcept;
    bool isOpen() const noexcept;
    uint16_t rtpPort() const noexcept;
    uint16_t rtcpPort() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    int effectiveReceiveBufferBytes() const noexcept;
    ::media::Result<MediaRtpUdpIngressReceiveLease>
    acquireIngressReceiveLease();

private:
    struct Impl;
    explicit MediaRtpUdpTransport(std::shared_ptr<Impl> impl) noexcept;
    static ::media::Status signalCancellation(const std::shared_ptr<Impl>& impl) noexcept;
    static ::media::Status resetCancellation(const std::shared_ptr<Impl>& impl) noexcept;
    static bool ingressLeaseCancelled(
        void* state, std::uint64_t sequence) noexcept;
    static void ingressLeaseMarkReceived(
        void* state, MediaRtpUdpChannel channel) noexcept;
    static void ingressLeaseRelease(void* state) noexcept;
    std::shared_ptr<Impl> snapshot() const noexcept;

    mutable std::mutex m_handleMutex;
    std::shared_ptr<Impl> m_impl;
};

} // namespace media::ffmpeg::graph
