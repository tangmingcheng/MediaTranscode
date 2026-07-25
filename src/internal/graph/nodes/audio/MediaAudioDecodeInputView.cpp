#include "internal/graph/nodes/audio/MediaAudioDecodeInputView.h"

#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {

::media::Result<MediaAudioDecodeInputView> resolveMediaAudioDecodeInput(
    const MediaBufferRef& input,
    MediaAudioLineageExecutionMode mode)
{
    const auto* released = dynamic_cast<const MediaAvReleasedAudioBuffer*>(input.get());
    if (mode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        if (!released || !FFmpegPacketView::packet(released->media())) {
            return ::media::Result<MediaAudioDecodeInputView>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized audio decode requires a released canonical packet"));
        }
        return ::media::Result<MediaAudioDecodeInputView>::success(
            MediaAudioDecodeInputView{
                released->media(),
                MediaSynchronizedAudioDecodeInput{
                    released->media()->lineage(),
                    *released->media()->audioSampleInterval(),
                    released->audioOrigin(),
                    released->trimLeadingSamples()}});
    }
    if (mode != MediaAudioLineageExecutionMode::LegacyPlainPacket || released ||
        !FFmpegPacketView::packet(input)) {
        return ::media::Result<MediaAudioDecodeInputView>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Legacy audio decode requires a plain packet"));
    }
    return ::media::Result<MediaAudioDecodeInputView>::success(
        MediaAudioDecodeInputView{input, std::nullopt});
}

} // namespace media::ffmpeg::graph
