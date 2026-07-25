#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshot.h"

namespace media::ffmpeg::graph {

bool FFmpegInputStreamSnapshot::codecParametersComplete() const noexcept
{
    return codec.complete();
}

::media::Result<::media::ffmpeg::CodecParametersPtr>
FFmpegInputStreamSnapshot::cloneCodecParameters() const
{
    return codec.cloneCodecParameters();
}

} // namespace media::ffmpeg::graph
