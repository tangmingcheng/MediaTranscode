#pragma once

#include "internal/graph/runtime/buffer/FFmpegCodecParametersSnapshot.h"
#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

namespace media::ffmpeg::graph {

struct FFmpegInputStreamSnapshot final {
    int index = -1;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    FFmpegCodecParametersSnapshot codec;
    MediaFormatDescriptor format;
    MediaTimeDescriptor time;

    bool codecParametersComplete() const noexcept;
    ::media::Result<::media::ffmpeg::CodecParametersPtr> cloneCodecParameters() const;
};

} // namespace media::ffmpeg::graph
