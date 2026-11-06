#include "internal/graph/model/MediaDecoderInputRetention.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Status MediaDecoderInputRetention::validate() const
{
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (threadCount <= 0 || threadType < 0 || authority.empty() ||
        mainHandoffPackets > maximum ||
        frameWorkerPackets > maximum - mainHandoffPackets ||
        serialPrivatePackets > maximum - mainHandoffPackets - frameWorkerPackets ||
        maximumInternalPackets() == 0)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Decoder options require valid prepared input retention facts"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
