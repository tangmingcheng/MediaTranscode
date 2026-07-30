#include "internal/graph/planner/capability/MediaAudioCapabilityProbe.h"

#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <limits>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

::media::Result<MediaAudioCapability> MediaAudioCapabilityProbe::inspect(AVFormatContext& inputContext)
{
    const int audioIndex = av_find_best_stream(&inputContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioIndex == AVERROR_STREAM_NOT_FOUND) {
        return ::media::Result<MediaAudioCapability>::success({});
    }
    if (audioIndex < 0) {
        return ::media::Result<MediaAudioCapability>::failure(
            ::media::ErrorInfo::ffmpegFailure("av_find_best_stream(audio)", audioIndex));
    }

    AVStream* audioStream = inputContext.streams[audioIndex];
    const AVCodecParameters* audioParams = audioStream ? audioStream->codecpar : nullptr;
    const char* audioCodecName = audioParams ? avcodec_get_name(audioParams->codec_id) : nullptr;
    if (!audioCodecName || std::string(audioCodecName) == "unknown") {
        return ::media::Result<MediaAudioCapability>::failure(
            ::media::ErrorInfo::unsupported("input audio codec is unknown"));
    }

    const MediaFormatDescriptor descriptor = FFmpegDescriptorMapper::fromStream(audioStream);
    MediaAudioCapability capability;
    capability.present = true;
    capability.stream.streamIndex = audioIndex;
    capability.stream.codecName = descriptor.codec.codecName.empty()
                                      ? canonicalCodecName(audioCodecName)
                                      : canonicalCodecName(descriptor.codec.codecName);
    capability.stream.sampleRate = descriptor.audio.sampleRate;
    capability.stream.channels = descriptor.audio.channels;
    capability.stream.channelLayout = descriptor.audio.channelLayout;
    capability.stream.sampleFormat = descriptor.audio.sampleFormat;
    auto profile = MediaAudioProfile::fromCodecProfile(
        capability.stream.codecName, descriptor.codec.profile);
    if (!profile) {
        return ::media::Result<MediaAudioCapability>::failure(profile.error());
    }
    capability.stream.profile = profile.value();
    capability.stream.bitrateBitsPerSecond = descriptor.codec.bitrate;
    const bool isMpegTsAac =
        audioParams->codec_id == AV_CODEC_ID_AAC &&
        inputContext.iformat &&
        inputContext.iformat->name &&
        std::string_view(inputContext.iformat->name) == "mpegts";
    auto decoder = isMpegTsAac
        ? MediaAudioDecoderCapabilityProvider::verifyAacAdts(*audioParams)
        : MediaAudioDecoderCapabilityProvider::verifyDemuxedStream(*audioParams);
    if (!decoder) {
        return ::media::Result<MediaAudioCapability>::failure(
            decoder.error());
    }
    if (decoder.value().maximumOutputBlockInputSamples >
        std::numeric_limits<int>::max()) {
        return ::media::Result<MediaAudioCapability>::failure(
            ::media::ErrorInfo::unsupported(
                "demuxed audio access-unit sample bound exceeds planner capacity"));
    }
    capability.stream.maximumAccessUnitSamples = static_cast<int>(
        decoder.value().maximumOutputBlockInputSamples);
    capability.stream.selectedDecoder = std::move(decoder).value();
    return ::media::Result<MediaAudioCapability>::success(std::move(capability));
}

} // namespace media::ffmpeg::graph
