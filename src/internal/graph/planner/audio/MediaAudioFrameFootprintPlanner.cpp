#include "internal/graph/planner/audio/MediaAudioFrameFootprintPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

extern "C" {
#include <libavutil/samplefmt.h>
}

namespace media::ffmpeg::graph {
::media::Result<std::uint64_t> MediaAudioFrameFootprintPlanner::logicalBytes(
    std::int64_t samples,
    int channels,
    const std::string& sampleFormat,
    const char* fact)
{
    const AVSampleFormat format = av_get_sample_fmt(sampleFormat.c_str());
    const int bytesPerSample = av_get_bytes_per_sample(format);
    if (samples <= 0 || channels <= 0 || format == AV_SAMPLE_FMT_NONE ||
        bytesPerSample <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                std::string(fact) + " lacks opened sample geometry"));
    }
    auto sampleBytes = MediaCheckedArithmetic::multiply(
        static_cast<std::uint64_t>(samples),
        static_cast<std::uint64_t>(channels), fact);
    return sampleBytes
        ? MediaCheckedArithmetic::multiply(
              sampleBytes.value(),
              static_cast<std::uint64_t>(bytesPerSample), fact)
        : sampleBytes;
}

::media::Result<MediaAudioSourceFrameFootprints> MediaAudioFrameFootprintPlanner::planSource(
    const MediaAudioPipelinePlan& audio, std::int64_t maximumResamplerOutputSamples)
{
    using Result = ::media::Result<MediaAudioSourceFrameFootprints>;
    if (!audio.selectedDecoder || !audio.selectedResampler || !audio.resolvedOutput)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "audio source frame planning requires opened decoder and resampler products"));
    const auto& decoder = *audio.selectedDecoder;
    const auto& resampler = *audio.selectedResampler;
    if (maximumResamplerOutputSamples < resampler.maximumOutputBlockSamples)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "audio frame footprint is smaller than the prepared resampler block"));
    const auto& output = *audio.resolvedOutput;
    auto decoded = logicalBytes(
        decoder.maximumOutputBlockInputSamples, decoder.outputChannels,
        decoder.outputSampleFormat, "opened decoder output frame bytes");
    auto resampled = logicalBytes(
        maximumResamplerOutputSamples, output.channels(),
        output.sampleFormat(), "prepared resampler output frame bytes");
    if (!decoded || !resampled)
        return Result::failure(!decoded ? decoded.error() : resampled.error());
    return Result::success({decoded.value(), resampled.value()});
}

} // namespace media::ffmpeg::graph
