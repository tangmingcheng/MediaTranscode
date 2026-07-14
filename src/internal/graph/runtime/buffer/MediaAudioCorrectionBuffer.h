#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/sync/MediaAudioCorrection.h"

namespace media::ffmpeg::graph {

class MediaAudioCorrectionBuffer final : public MediaBuffer {
public:
    explicit MediaAudioCorrectionBuffer(MediaAudioCompensationCommand command);

    MediaBufferType type() const noexcept override;
    const MediaAudioCompensationCommand& command() const noexcept;

private:
    MediaAudioCompensationCommand m_command;
};

} // namespace media::ffmpeg::graph
