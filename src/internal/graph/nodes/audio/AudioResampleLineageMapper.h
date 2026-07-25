#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>

struct AVFrame;

namespace media::ffmpeg::graph {

class AudioResampleLineageState;
class MediaBoundCanonicalAudioBuffer;

class AudioResampleLineageMapper final {
public:
    explicit AudioResampleLineageMapper(
        std::shared_ptr<AudioResampleLineageState> state) noexcept;

    ::media::Status acceptInput(
        const MediaBoundCanonicalAudioBuffer& input,
        const AVFrame& frame,
        int outputSampleRate);
    ::media::Result<MediaBufferRef> bindOutput(
        MediaBufferRef output,
        std::int64_t outputSamples);
    ::media::Status settleDroppedSamples(
        std::int64_t authorizedDroppedSamples);

private:
    std::shared_ptr<AudioResampleLineageState> m_state;
};

} // namespace media::ffmpeg::graph
