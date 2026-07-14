#pragma once

#include "internal/graph/runtime/io/MediaOutputByteSink.h"

#include <memory>
#include <optional>
#include <string>

struct AVIOContext;

namespace media::ffmpeg::graph {

class FFmpegAvioOutputByteSink final : public MediaOutputByteSink {
public:
    static ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>> open(
        std::string url,
        int writeFlags);

    ~FFmpegAvioOutputByteSink() noexcept override;

    FFmpegAvioOutputByteSink(const FFmpegAvioOutputByteSink&) = delete;
    FFmpegAvioOutputByteSink& operator=(const FFmpegAvioOutputByteSink&) = delete;

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override;
    ::media::Status flush() override;
    ::media::Status close() override;

private:
    explicit FFmpegAvioOutputByteSink(AVIOContext* context) noexcept;

    ::media::Status currentStatus() const;
    void preserveFailure(::media::ErrorInfo error);

    AVIOContext* m_context = nullptr;
    bool m_closed = false;
    std::optional<::media::ErrorInfo> m_firstFailure;
};

} // namespace media::ffmpeg::graph
