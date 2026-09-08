#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

struct FFmpegInputStreamSnapshot;
class MediaGraphPayloadReservation;

class PacketNormalizeNode final : public FFmpegNodeRuntime {
public:
    explicit PacketNormalizeNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    void releaseInputSnapshots() noexcept;
    ::media::Status bindInputSnapshots(MediaGraphExecutionContext& context);
    ::media::Status bindSourceStream(MediaGraphExecutionContext& context);
    ::media::Result<MediaBufferRef> normalizePacket(
        const MediaBufferRef& buffer,
        MediaGraphPayloadReservation reservation);
    ::media::Status normalizePacketTimestamps(MediaBufferRef& buffer);

private:
    MediaBufferRef m_inputSnapshotOwner;
    const FFmpegInputStreamSnapshot* m_sourceStream = nullptr;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    int m_sourceStreamIndex = invalidMediaStreamIndex;
    bool m_monotonicPacketTimestamps = false;
    int64_t m_nextPacketDts = invalidMediaTimeValue;
};

} // namespace media::ffmpeg::graph
