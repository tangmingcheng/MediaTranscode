#pragma once

#include "media_transcode/Result.h"
#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaPacketStorageExtent { Payload, PaddedAllocation };

// Existing preparation lease domain: backing plus public packet/reference
// headers. Excludes lease controls, wrappers and FFmpeg-private allocator state.
::media::Result<std::size_t> preparedMediaPacketStorageBytes(
    std::uint64_t bytes, MediaPacketStorageExtent extent);

} // namespace media::ffmpeg::graph
