#pragma once

#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaUdpSocketConfig final {
    MediaIpAddressFamily addressFamily;
    std::string bindAddress;
    uint16_t port;
    int receiveBufferBytes;
};

class MediaRtpUdpTransport;

class MediaUdpSocket final {
public:
    MediaUdpSocket() noexcept;
    ~MediaUdpSocket();
    MediaUdpSocket(MediaUdpSocket&&) noexcept;
    MediaUdpSocket& operator=(MediaUdpSocket&&) noexcept;

    MediaUdpSocket(const MediaUdpSocket&) = delete;
    MediaUdpSocket& operator=(const MediaUdpSocket&) = delete;

    static ::media::Result<MediaUdpSocket> bind(
        std::shared_ptr<MediaSocketRuntime> runtime, const MediaUdpSocketConfig& config);

    bool isOpen() const noexcept;
    intptr_t nativeHandle() const noexcept;
    uint16_t localPort() const noexcept;
    int effectiveReceiveBufferBytes() const noexcept;
    ::media::Status sendTo(const std::string& address, uint16_t port,
                           std::span<const uint8_t> datagram) const;
    void close() noexcept;

private:
    struct Impl;
    explicit MediaUdpSocket(std::unique_ptr<Impl> impl) noexcept;
    ::media::Result<std::vector<uint8_t>> receive(std::size_t maximumDatagramBytes);
    intptr_t waitHandle() const noexcept;
    ::media::Status consumeNetworkEvent() noexcept;

    friend class MediaRtpUdpTransport;
    std::unique_ptr<Impl> m_impl;
};

} // namespace media::ffmpeg::graph
