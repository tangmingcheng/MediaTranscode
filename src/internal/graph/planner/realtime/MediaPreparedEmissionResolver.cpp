#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

extern "C" {
#include <libavutil/samplefmt.h>
}

#include <algorithm>

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> frameBytes(
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

::media::Result<MediaPreparedAudioFrameFootprintEnvelope>
resolveAudioFrames(const MediaAudioPipelinePlan& audio)
{
    using Result =
        ::media::Result<MediaPreparedAudioFrameFootprintEnvelope>;
    if (!audio.selectedDecoder || !audio.selectedResampler ||
        !audio.resolvedOutput || !audio.preparedEmission) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "audio frame planning requires opened decoder, resampler, and encoder products"));
    }
    const auto& decoder = *audio.selectedDecoder;
    const auto& resampler = *audio.selectedResampler;
    const auto& output = *audio.resolvedOutput;
    auto decoded = frameBytes(
        decoder.maximumOutputBlockInputSamples, decoder.outputChannels,
        decoder.outputSampleFormat, "opened decoder output frame bytes");
    auto resampled = frameBytes(
        resampler.maximumOutputBlockSamples, output.channels(),
        output.sampleFormat(), "prepared resampler output frame bytes");
    auto encoder = frameBytes(
        audio.preparedEmission->frameSizeSamples, output.channels(),
        output.sampleFormat(), "opened encoder input frame bytes");
    if (!decoded || !resampled || !encoder) {
        return Result::failure(
            !decoded ? decoded.error() :
            !resampled ? resampled.error() : encoder.error());
    }
    return Result::success(MediaPreparedAudioFrameFootprintEnvelope{
        decoded.value(), resampled.value(), encoder.value(),
        (std::max)({decoded.value(), resampled.value(), encoder.value()}),
        "opened-decoder-readback+prepared-resampler-ratio-delay+opened-encoder-readback"});
}

} // namespace

::media::Result<MediaPreparedRealtimeEmissionSet>
MediaPreparedEmissionResolver::resolve(
    const MediaPipelinePlan& videoPipeline,
    MediaRational outputFrameRate,
    const MediaAudioPipelinePlan* audioPipeline)
{
    const auto& video = videoPipeline.selected.encoder.preparedEmission;
    if (!videoPipeline.enabled ||
        videoPipeline.branchMode != MediaBranchMode::TranscodeFrame ||
        !video || video->sustainedPayloadBytesPerSecond == 0 ||
        video->peakPayloadBytesPerSecond <
            video->sustainedPayloadBytesPerSecond ||
        video->maximumAccessUnitPayloadBytes == 0 ||
        video->maximumBurstPayloadBytes == 0 ||
        video->maximumEncoderRetainedFrames == 0 ||
        !video->encodedPacketLayout ||
        video->authority.empty() || video->backend.empty() ||
        !outputFrameRate.isKnown() || outputFrameRate.num <= 0 ||
        outputFrameRate.den <= 0 ||
        video->accessUnitsPerSecondNumerator !=
            static_cast<std::uint64_t>(outputFrameRate.num) ||
        video->accessUnitsPerSecondDenominator !=
            static_cast<std::uint64_t>(outputFrameRate.den)) {
        return ::media::Result<MediaPreparedRealtimeEmissionSet>::failure(
            ::media::ErrorInfo::notInitialized(
                "wire planning requires authoritative opened video emission readback"));
    }

    MediaPreparedRealtimeEmissionSet resolved{
        *video, std::nullopt, std::nullopt, std::nullopt};
    if (!audioPipeline) {
        return ::media::Result<MediaPreparedRealtimeEmissionSet>::success(
            std::move(resolved));
    }
    const auto& audio = audioPipeline->preparedEmission;
    if (!audioPipeline->enabled ||
        audioPipeline->branchMode != MediaBranchMode::TranscodeFrame ||
        !audio || audio->sustainedPayloadBytesPerSecond == 0 ||
        audio->peakPayloadBytesPerSecond <
            audio->sustainedPayloadBytesPerSecond ||
        audio->maximumAccessUnitPayloadBytes == 0 ||
        audio->maximumBurstPayloadBytes == 0 ||
        audio->accessUnitsPerSecondNumerator == 0 ||
        audio->accessUnitsPerSecondDenominator == 0 ||
        audio->frameSizeSamples <= 0 || audio->authority.empty() ||
        audio->backend.empty()) {
        return ::media::Result<MediaPreparedRealtimeEmissionSet>::failure(
            ::media::ErrorInfo::notInitialized(
                "wire planning requires authoritative opened audio emission readback"));
    }
    resolved.audio = *audio;
    auto audioFrames = resolveAudioFrames(*audioPipeline);
    if (!audioFrames) {
        return ::media::Result<MediaPreparedRealtimeEmissionSet>::failure(
            audioFrames.error());
    }
    resolved.audioFrames = std::move(audioFrames).value();
    return ::media::Result<MediaPreparedRealtimeEmissionSet>::success(
        std::move(resolved));
}

} // namespace media::ffmpeg::graph
