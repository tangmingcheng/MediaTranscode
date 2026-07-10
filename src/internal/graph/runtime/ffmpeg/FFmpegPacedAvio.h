#pragma once

#include <cstdint>
#include <string>

struct AVIOContext;

namespace media::ffmpeg {

struct FFmpegPacedAvioOptions {
    bool enabled = false;
    int64_t bytesPerSecond = 0;
    int64_t burstBytes = 0;
};

int openPacedWriteAvio(AVIOContext** context,
                       const std::string& url,
                       const FFmpegPacedAvioOptions& options) noexcept;

bool isPacedWriteAvio(const AVIOContext* context) noexcept;

void resetPacedWriteAvio(AVIOContext* context) noexcept;

void closePacedWriteAvio(AVIOContext** context) noexcept;

} // namespace media::ffmpeg
