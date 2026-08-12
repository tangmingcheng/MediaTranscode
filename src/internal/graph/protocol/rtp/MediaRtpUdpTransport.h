#pragma once

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

enum class MediaRtpUdpChannel {
    Rtp,
    Rtcp
};

struct MediaRtpUdpDatagram final {
    MediaRtpUdpChannel channel;
    std::vector<uint8_t> bytes;
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

private:
    struct Impl;
    explicit MediaRtpUdpTransport(std::shared_ptr<Impl> impl) noexcept;
    std::shared_ptr<Impl> snapshot() const noexcept;

    mutable std::mutex m_handleMutex;
    std::shared_ptr<Impl> m_impl;
};

} // namespace media::ffmpeg::graph
