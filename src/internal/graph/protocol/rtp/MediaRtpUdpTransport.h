#pragma once

#include "internal/graph/runtime/network/MediaUdpSocket.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
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
    ::media::Status stop() noexcept;
    ::media::Status reset() noexcept;
    ::media::Status abort() noexcept;
    void close() noexcept;
    bool isOpen() const noexcept;
    uint16_t rtpPort() const noexcept;
    uint16_t rtcpPort() const noexcept;

private:
    struct Impl;
    explicit MediaRtpUdpTransport(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
};

} // namespace media::ffmpeg::graph
