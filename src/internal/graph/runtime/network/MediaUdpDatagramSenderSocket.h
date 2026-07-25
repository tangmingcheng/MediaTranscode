#pragma once

#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderPort.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaUdpDatagramSenderSocket final : public MediaUdpDatagramSenderPort {
public:
    explicit MediaUdpDatagramSenderSocket(
        std::shared_ptr<MediaSocketRuntime> runtime);
    ~MediaUdpDatagramSenderSocket() override;

    MediaUdpDatagramSenderSocket(const MediaUdpDatagramSenderSocket&) = delete;
    MediaUdpDatagramSenderSocket& operator=(const MediaUdpDatagramSenderSocket&) = delete;

    ::media::Status open(
        const MediaUdpDatagramSenderPortOpenRequest& request) override;
    MediaUdpDatagramSendOutcome send(
        std::span<const std::uint8_t> datagram) override;
    std::optional<MediaUdpDatagramEndpoint> localEndpoint() const override;
    std::optional<MediaUdpDatagramEndpoint> remoteEndpoint() const override;
    void close() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

class MediaUdpDatagramSenderSocketFactory final
    : public MediaUdpDatagramSenderPortFactory {
public:
    explicit MediaUdpDatagramSenderSocketFactory(
        std::shared_ptr<MediaSocketRuntime> runtime);

    ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>> create() override;

private:
    std::shared_ptr<MediaSocketRuntime> m_runtime;
};

} // namespace media::ffmpeg::graph
