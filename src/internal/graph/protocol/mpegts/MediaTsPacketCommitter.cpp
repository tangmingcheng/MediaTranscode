#include "internal/graph/protocol/mpegts/MediaTsPacketCommitter.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaTsPacketCursorCommitter::commit(
    MediaTsPacketCursor& cursor,
    MediaTsPacketCommitToken token)
{
    return cursor.commit(std::move(token));
}

} // namespace media::ffmpeg::graph
