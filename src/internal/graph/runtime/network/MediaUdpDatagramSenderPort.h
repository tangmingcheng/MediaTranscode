#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace media::ffmpeg::graph {

inline constexpr std::size_t kMediaUdpMaximumPayloadBytes = 65'507;

enum class MediaUdpSenderIoBehavior {
    NonBlockingRejectOnPressure
};

class MediaUdpDatagramEndpoint final {
public:
    static ::media::Result<MediaUdpDatagramEndpoint> create(
        MediaIpAddressFamily addressFamily,
        std::string numericAddress,
        std::uint16_t port);

    MediaIpAddressFamily addressFamily() const noexcept { return m_addressFamily; }
    const std::string& numericAddress() const noexcept { return m_numericAddress; }
    std::uint16_t port() const noexcept { return m_port; }

    friend bool operator==(const MediaUdpDatagramEndpoint&,
                           const MediaUdpDatagramEndpoint&) = default;

private:
    MediaUdpDatagramEndpoint(MediaIpAddressFamily addressFamily,
                             std::string numericAddress,
                             std::uint16_t port) noexcept;

    MediaIpAddressFamily m_addressFamily;
    std::string m_numericAddress;
    std::uint16_t m_port;
};

class MediaUdpDatagramSenderPortOpenRequest final {
public:
    static ::media::Result<MediaUdpDatagramSenderPortOpenRequest> create(
        MediaUdpDatagramEndpoint localEndpoint,
        MediaUdpDatagramEndpoint remoteEndpoint,
        int sendBufferBytes,
        std::size_t maximumDatagramBytes,
        MediaUdpSenderIoBehavior ioBehavior);

    const MediaUdpDatagramEndpoint& localEndpoint() const noexcept
    {
        return m_localEndpoint;
    }
    const MediaUdpDatagramEndpoint& remoteEndpoint() const noexcept
    {
        return m_remoteEndpoint;
    }
    int sendBufferBytes() const noexcept { return m_sendBufferBytes; }
    std::size_t maximumDatagramBytes() const noexcept
    {
        return m_maximumDatagramBytes;
    }
    MediaUdpSenderIoBehavior ioBehavior() const noexcept { return m_ioBehavior; }

private:
    MediaUdpDatagramSenderPortOpenRequest(
        MediaUdpDatagramEndpoint localEndpoint,
        MediaUdpDatagramEndpoint remoteEndpoint,
        int sendBufferBytes,
        std::size_t maximumDatagramBytes,
        MediaUdpSenderIoBehavior ioBehavior) noexcept;

    MediaUdpDatagramEndpoint m_localEndpoint;
    MediaUdpDatagramEndpoint m_remoteEndpoint;
    int m_sendBufferBytes;
    std::size_t m_maximumDatagramBytes;
    MediaUdpSenderIoBehavior m_ioBehavior;
};

enum class MediaUdpDatagramSendKind {
    Accepted,
    NotAccepted,
    AmbiguousPartial
};

class MediaUdpDatagramSendOutcome final {
public:
    static MediaUdpDatagramSendOutcome accepted(std::size_t acceptedBytes);
    static MediaUdpDatagramSendOutcome notAccepted(::media::ErrorInfo error);
    static MediaUdpDatagramSendOutcome ambiguousPartial(
        ::media::ErrorInfo error, std::size_t acceptedBytes);

    MediaUdpDatagramSendKind kind() const noexcept { return m_kind; }
    std::size_t acceptedBytes() const noexcept { return m_acceptedBytes; }
    const ::media::ErrorInfo* error() const noexcept
    {
        return m_error ? &*m_error : nullptr;
    }

private:
    MediaUdpDatagramSendOutcome(MediaUdpDatagramSendKind kind,
                                std::size_t acceptedBytes,
                                std::optional<::media::ErrorInfo> error);

    MediaUdpDatagramSendKind m_kind;
    std::size_t m_acceptedBytes;
    std::optional<::media::ErrorInfo> m_error;
};

class MediaUdpAmbiguousDeliveryError final : public std::runtime_error {
public:
    MediaUdpAmbiguousDeliveryError(::media::ErrorInfo cause,
                                   std::size_t acceptedBytes);

    const ::media::ErrorInfo& cause() const noexcept { return m_cause; }
    std::size_t acceptedBytes() const noexcept { return m_acceptedBytes; }

private:
    ::media::ErrorInfo m_cause;
    std::size_t m_acceptedBytes;
};

class MediaUdpDatagramSenderPort {
public:
    virtual ~MediaUdpDatagramSenderPort() = default;

    virtual ::media::Status open(
        const MediaUdpDatagramSenderPortOpenRequest& request) = 0;
    virtual MediaUdpDatagramSendOutcome send(
        std::span<const std::uint8_t> datagram) = 0;
    virtual std::optional<MediaUdpDatagramEndpoint> localEndpoint() const = 0;
    virtual std::optional<MediaUdpDatagramEndpoint> remoteEndpoint() const = 0;
    virtual void close() noexcept = 0;
};

class MediaUdpDatagramSenderPortFactory {
public:
    virtual ~MediaUdpDatagramSenderPortFactory() = default;
    virtual ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>> create() = 0;
};

} // namespace media::ffmpeg::graph
