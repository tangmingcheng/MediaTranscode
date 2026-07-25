#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

struct AVIOContext;

namespace media::ffmpeg::graph {

using FFmpegDatagramSink =
    std::function<::media::Status(std::span<const std::uint8_t>)>;

class FFmpegDatagramWriteAvioConfig final {
public:
    static ::media::Result<FFmpegDatagramWriteAvioConfig> create(
        int maximumDatagramBytes) noexcept;

    int maximumDatagramBytes() const noexcept { return m_maximumDatagramBytes; }

private:
    explicit FFmpegDatagramWriteAvioConfig(int maximumDatagramBytes) noexcept;

    int m_maximumDatagramBytes;
};

class FFmpegDatagramWriteAvio final {
public:
    static ::media::Result<std::unique_ptr<FFmpegDatagramWriteAvio>> create(
        FFmpegDatagramWriteAvioConfig config,
        FFmpegDatagramSink sink);

    ~FFmpegDatagramWriteAvio();

    FFmpegDatagramWriteAvio(const FFmpegDatagramWriteAvio&) = delete;
    FFmpegDatagramWriteAvio& operator=(const FFmpegDatagramWriteAvio&) = delete;

    ::media::Status open() noexcept;
    ::media::Status close() noexcept;
    ::media::Status reset() noexcept;

    AVIOContext* context() const noexcept { return m_context; }
    const std::optional<::media::ErrorInfo>& sinkFailure() const noexcept
    {
        return m_sinkFailure;
    }

private:
    FFmpegDatagramWriteAvio(FFmpegDatagramWriteAvioConfig config,
                            FFmpegDatagramSink sink);

    static int writePacket(void* opaque,
                           const std::uint8_t* bytes,
                           int size) noexcept;
    int deliver(std::span<const std::uint8_t> bytes) noexcept;
    void release() noexcept;

    FFmpegDatagramWriteAvioConfig m_config;
    FFmpegDatagramSink m_sink;
    AVIOContext* m_context = nullptr;
    std::optional<::media::ErrorInfo> m_sinkFailure;
};

} // namespace media::ffmpeg::graph
