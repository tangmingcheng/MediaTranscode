#include "internal/graph/runtime/ffmpeg/MediaPreparedPacketStorage.h"
#include <limits>
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {

::media::Result<std::size_t> preparedMediaPacketStorageBytes(
    std::uint64_t bytes, MediaPacketStorageExtent extent)
{
    using Result = ::media::Result<std::size_t>;
    if (extent != MediaPacketStorageExtent::Payload && extent != MediaPacketStorageExtent::PaddedAllocation)
        return Result::failure(::media::ErrorInfo::invalidArgument("Prepared packet storage extent is invalid"));
    const std::size_t overhead = sizeof(AVPacket) + sizeof(AVBufferRef) +
        (extent == MediaPacketStorageExtent::Payload ? AV_INPUT_BUFFER_PADDING_SIZE : 0);
    if (bytes > (std::numeric_limits<std::size_t>::max)() - overhead)
        return Result::failure(::media::ErrorInfo::invalidArgument("Prepared RTP AU allocation overflows size_t"));
    return Result::success(static_cast<std::size_t>(bytes) + overhead);
}

} // namespace media::ffmpeg::graph
