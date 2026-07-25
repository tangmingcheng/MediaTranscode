#pragma once

#include "internal/graph/runtime/io/MediaOutputByteSink.h"

#include <memory>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class FFmpegAvioOutputByteSinkBackend;

class FFmpegAvioOutputByteSink final : public MediaOutputByteSink {
public:
    static ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>> open(
        std::string url,
        int writeFlags);
    static ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>> create(
        std::unique_ptr<FFmpegAvioOutputByteSinkBackend> backend);

    ~FFmpegAvioOutputByteSink() noexcept override;

    FFmpegAvioOutputByteSink(const FFmpegAvioOutputByteSink&) = delete;
    FFmpegAvioOutputByteSink& operator=(const FFmpegAvioOutputByteSink&) = delete;

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override;
    ::media::Status flush() override;
    ::media::Status close() override;

private:
    explicit FFmpegAvioOutputByteSink(
        std::unique_ptr<FFmpegAvioOutputByteSinkBackend> backend) noexcept;

    ::media::Status currentStatus() const;
    void preserveFailure(::media::ErrorInfo error);

    std::unique_ptr<FFmpegAvioOutputByteSinkBackend> m_backend;
    bool m_closed = false;
    std::optional<::media::ErrorInfo> m_firstFailure;
};

} // namespace media::ffmpeg::graph
