#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/packet.h>
}

namespace media_transcode::test {

inline ::media::Result<::media::ffmpeg::graph::MediaBufferRef> makePacketBuffer(
    bool keyFrame,
    int64_t pts = 0,
    ::media::ffmpeg::graph::MediaStreamKind streamKind = ::media::ffmpeg::graph::MediaStreamKind::Video)
{
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Result<::media::ffmpeg::graph::MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("test packet allocation failed"));
    }
    packet->flags = keyFrame ? AV_PKT_FLAG_KEY : 0;
    packet->pts = pts;
    packet->dts = pts;
    packet->duration = 1;
    return ::media::ffmpeg::graph::FFmpegBufferFactory::wrapPacket(std::move(packet), streamKind);
}

} // namespace media_transcode::test
