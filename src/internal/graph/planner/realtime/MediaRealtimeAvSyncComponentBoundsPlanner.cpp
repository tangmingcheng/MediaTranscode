#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::int64_t> checkedCapacitySamples(
    std::size_t capacity, std::int64_t samples, const char* owner)
{
    if (capacity == 0 || samples <= 0 ||
        capacity > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max() / samples)) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(owner) + " bound is not representable"));
    }
    return ::media::Result<std::int64_t>::success(
        static_cast<std::int64_t>(capacity) * samples);
}

::media::Result<std::int64_t> checkedOutputSamples(
    std::int64_t sourceSamples, int sourceRate, int outputRate,
    const char* owner)
{
    if (sourceSamples < 0 || sourceRate <= 0 || outputRate <= 0 ||
        sourceSamples > (std::numeric_limits<std::int64_t>::max() -
                         sourceRate + 1) / outputRate) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(owner) + " conversion is not representable"));
    }
    return ::media::Result<std::int64_t>::success(
        (sourceSamples * outputRate + sourceRate - 1) / sourceRate);
}

} // namespace

::media::Result<MediaRealtimeAvSyncComponentBounds>
MediaRealtimeAvSyncComponentBoundsPlanner::plan(
    const MediaGraphQueueParameters& queues,
    const MediaAudioPipelinePlan& audio)
{
    if (audio.branchMode == MediaBranchMode::CopyPacket) {
        if (!audio.maximumAccessUnitSamples ||
            *audio.maximumAccessUnitSamples <= 0 ||
            audio.selectedDecoder || audio.selectedResampler) {
            return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
                ::media::ErrorInfo::notInitialized(
                    "synchronized packet copy requires only an access-unit sample bound"));
        }
        auto scheduler = checkedCapacitySamples(
            queues.mux, *audio.maximumAccessUnitSamples, "scheduler queue");
        if (!scheduler) {
            return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
                scheduler.error());
        }
        return ::media::Result<MediaRealtimeAvSyncComponentBounds>::success(
            MediaSynchronizedAudioPacketCopyBounds{
                *audio.maximumAccessUnitSamples, scheduler.value()});
    }
    if (audio.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
            ::media::ErrorInfo::invalidArgument(
                "synchronized audio requires copy or frame transcode"));
    }
    if (!audio.selectedDecoder || !audio.selectedResampler ||
        !audio.resolvedOutput ||
        audio.resolvedOutput->codecFrameSamples() <= 0) {
        return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
            ::media::ErrorInfo::notInitialized(
                "synchronized audio components did not publish timing bounds"));
    }
    const auto& decoder = *audio.selectedDecoder;
    const auto& resampler = *audio.selectedResampler;
    const int outputRate = audio.resolvedOutput->sampleRate();
    if (decoder.outputSampleRate != resampler.inputSampleRate ||
        decoder.maximumOutputBlockInputSamples !=
            resampler.maximumInputBlockSamples ||
        resampler.outputSampleRate != outputRate) {
        return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
            ::media::ErrorInfo::invalidArgument(
                "selected decoder, resampler, and encoder sample domains conflict"));
    }
    auto decoderDelay = checkedOutputSamples(
        decoder.delayOutputSamples, decoder.outputSampleRate, outputRate,
        "decoder delay");
    auto decoderBlock = checkedOutputSamples(
        decoder.maximumOutputBlockInputSamples, decoder.outputSampleRate,
        outputRate, "decoder output block");
    if (!decoderDelay || !decoderBlock) {
        return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
            decoderDelay ? decoderBlock.error() : decoderDelay.error());
    }
    const auto resamplerBlock = resampler.maximumOutputBlockSamples;
    const auto encoderBlock =
        audio.resolvedOutput->codecFrameSamples();
    auto decode = checkedCapacitySamples(
        queues.packet, decoderBlock.value(), "decode queue");
    auto resample = checkedCapacitySamples(
        queues.frame, decoderBlock.value(), "resample queue");
    auto encode = checkedCapacitySamples(
        queues.frame, resamplerBlock, "encode queue");
    auto scheduler = checkedCapacitySamples(
        queues.mux, encoderBlock, "scheduler queue");
    auto mailbox = checkedCapacitySamples(
        queues.metadata, resamplerBlock, "correction mailbox");
    if (!decode || !resample || !encode || !scheduler || !mailbox) {
        return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
            !decode ? decode.error() : !resample ? resample.error() :
            !encode ? encode.error() : !scheduler ? scheduler.error() :
            mailbox.error());
    }
    return ::media::Result<MediaRealtimeAvSyncComponentBounds>::success(
        MediaSynchronizedAudioFrameTranscodeBounds{
            decoderDelay.value(), decode.value(), resample.value(),
            encode.value(), scheduler.value(), mailbox.value(),
            resamplerBlock, queues.metadata});
}

} // namespace media::ffmpeg::graph
