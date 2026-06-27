#pragma once

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/HardwareFrameBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {

class FFmpegBufferFactory final {
public:
    static ::media::Result<MediaBufferRef> wrapInputFormatContext(::media::ffmpeg::InputFormatContextPtr context);
    static ::media::Result<MediaBufferRef> wrapOutputFormatContext(::media::ffmpeg::OutputFormatContextPtr context);
    static ::media::Result<MediaBufferRef> borrowFormatContext(AVFormatContext* context);

    static ::media::Result<MediaBufferRef> wrapPacket(::media::ffmpeg::PacketPtr packet,
                                                       MediaStreamKind streamKind = MediaStreamKind::Unknown);
    static ::media::Result<MediaBufferRef> clonePacket(const AVPacket* packet,
                                                       MediaStreamKind streamKind = MediaStreamKind::Unknown);

    static ::media::Result<MediaBufferRef> wrapFrame(::media::ffmpeg::FramePtr frame,
                                                      MediaStreamKind streamKind = MediaStreamKind::Unknown);
    static ::media::Result<MediaBufferRef> cloneFrame(const AVFrame* frame,
                                                      MediaStreamKind streamKind = MediaStreamKind::Unknown);

    static ::media::Result<MediaBufferRef> wrapHardwareFrame(::media::ffmpeg::FramePtr frame,
                                                              MediaHardwareDescriptor hardware,
                                                              MediaStreamKind streamKind = MediaStreamKind::Video);
};

} // namespace media::ffmpeg::graph
