#include "internal/FFmpegAudioPipelineStrategy.h"

namespace media::ffmpeg {

Status IFFmpegAudioPipelineStrategy::sendFrame(AVFrame* frame)
{
    (void)frame;
    return Status::failure(ErrorInfo::unsupported(
        "audio pipeline strategy does not support direct frame input"));
}

Result<int> IFFmpegAudioPipelineStrategy::receiveAndWritePackets(
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    (void)onPacketWritten;
    return Result<int>::failure(ErrorInfo::unsupported(
        "audio pipeline strategy does not support direct packet receiving"));
}

} // namespace media::ffmpeg
