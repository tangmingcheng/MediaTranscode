#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>

namespace media::ffmpeg::graph {

enum class MediaRtpUdpSenderTransportState {
    Created,
    Open,
    Closed,
    Poisoned
};

class MediaRtpUdpBoundLocalEndpoints final {
public:
    const MediaUdpDatagramEndpoint& rtp() const noexcept { return m_rtp; }
    const MediaUdpDatagramEndpoint& rtcp() const noexcept { return m_rtcp; }

private:
    friend class MediaRtpUdpSenderTransport;
    MediaRtpUdpBoundLocalEndpoints(MediaUdpDatagramEndpoint rtp,
                                   MediaUdpDatagramEndpoint rtcp) noexcept;

    MediaUdpDatagramEndpoint m_rtp;
    MediaUdpDatagramEndpoint m_rtcp;
};

class MediaRtpUdpSenderTransport final {
public:
    ~MediaRtpUdpSenderTransport();

    MediaRtpUdpSenderTransport(const MediaRtpUdpSenderTransport&) = delete;
    MediaRtpUdpSenderTransport& operator=(const MediaRtpUdpSenderTransport&) = delete;

    static ::media::Result<std::unique_ptr<MediaRtpUdpSenderTransport>> create(
        MediaRtpUdpSenderConfig config,
        MediaUdpDatagramSenderPortFactory& portFactory);

    ::media::Status open();
    ::media::Status sendRtp(std::span<const std::uint8_t> datagram);
    ::media::Status sendRtcp(std::span<const std::uint8_t> datagram);
    ::media::Status close() noexcept;

    MediaRtpUdpSenderTransportState state() const;
    std::optional<MediaRtpUdpBoundLocalEndpoints> boundLocalEndpoints() const;
    MediaUdpDatagramEndpoint remoteRtpEndpoint() const;
    MediaUdpDatagramEndpoint remoteRtcpEndpoint() const;

private:
    class OperationGuard final {
    public:
        explicit OperationGuard(MediaRtpUdpSenderTransport& owner);
        ~OperationGuard();

        bool entered() const noexcept { return m_entered; }

    private:
        MediaRtpUdpSenderTransport& m_owner;
        bool m_entered;
    };

    MediaRtpUdpSenderTransport(
        MediaRtpUdpSenderConfig config,
        std::unique_ptr<MediaUdpDatagramSenderPort> rtpPort,
        std::unique_ptr<MediaUdpDatagramSenderPort> rtcpPort) noexcept;

    ::media::Status send(
        MediaUdpDatagramSenderPort& port,
        std::span<const std::uint8_t> datagram);
    ::media::Status operationRejected() const;
    void poisonAndClose(::media::ErrorInfo cause) noexcept;
    ::media::Status validateBoundEndpoints(
        const MediaUdpDatagramEndpoint& rtp,
        const MediaUdpDatagramEndpoint& rtcp) const;
    ::media::Result<MediaUdpDatagramSenderPortOpenRequest> makeOpenRequest(
        bool rtp) const;

    MediaRtpUdpSenderConfig m_config;
    std::unique_ptr<MediaUdpDatagramSenderPort> m_rtpPort;
    std::unique_ptr<MediaUdpDatagramSenderPort> m_rtcpPort;
    mutable std::mutex m_mutex;
    std::condition_variable m_operationFinished;
    bool m_operationActive = false;
    std::thread::id m_operationThread;
    bool m_closeActive = false;
    std::thread::id m_closeThread;
    MediaRtpUdpSenderTransportState m_state =
        MediaRtpUdpSenderTransportState::Created;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    std::optional<MediaRtpUdpBoundLocalEndpoints> m_boundLocalEndpoints;
};

} // namespace media::ffmpeg::graph
