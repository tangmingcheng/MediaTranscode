#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <limits>
#include <string>
#include <utility>

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
    auto source = planSource(queues, audio);
    if (!source) return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(source.error());
    auto encode = checkedCapacitySamples(
        queues.frame, source.value().maximumResamplerOutputBlockSamples, "encode queue");
    auto scheduler = checkedCapacitySamples(
        queues.mux, audio.resolvedOutput->codecFrameSamples(), "scheduler queue");
    if (!encode || !scheduler) return ::media::Result<MediaRealtimeAvSyncComponentBounds>::failure(
        !encode ? encode.error() : scheduler.error());
    return ::media::Result<MediaRealtimeAvSyncComponentBounds>::success(
        MediaSynchronizedAudioFrameTranscodeBounds{
            std::move(source).value(), encode.value(), scheduler.value()});
}

::media::Result<MediaSynchronizedAudioSourceBounds>
MediaRealtimeAvSyncComponentBoundsPlanner::planSource(
    const MediaGraphQueueParameters& queues,
    const MediaAudioPipelinePlan& audio)
{
    if (audio.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<MediaSynchronizedAudioSourceBounds>::failure(
            ::media::ErrorInfo::invalidArgument(
                "source audio correction requires frame transcode"));
    }
    if (!audio.selectedDecoder || !audio.selectedResampler ||
        !audio.resolvedOutput ||
        audio.resolvedOutput->sampleRate() <= 0) {
        return ::media::Result<MediaSynchronizedAudioSourceBounds>::failure(
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
        return ::media::Result<MediaSynchronizedAudioSourceBounds>::failure(
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
        return ::media::Result<MediaSynchronizedAudioSourceBounds>::failure(
            decoderDelay ? decoderBlock.error() : decoderDelay.error());
    }
    const auto resamplerBlock = resampler.maximumOutputBlockSamples;
    auto decode = checkedCapacitySamples(
        queues.packet, decoderBlock.value(), "decode queue");
    auto resample = checkedCapacitySamples(
        queues.frame, decoderBlock.value(), "resample queue");
    auto mailbox = checkedCapacitySamples(
        queues.frame, resamplerBlock, "correction mailbox");
    if (!decode || !resample || !mailbox) {
        return ::media::Result<MediaSynchronizedAudioSourceBounds>::failure(
            !decode ? decode.error() : !resample ? resample.error() :
            mailbox.error());
    }
    return ::media::Result<MediaSynchronizedAudioSourceBounds>::success(
        {decoderDelay.value(), decode.value(), resample.value(), mailbox.value(),
         resamplerBlock, queues.frame});
}

} // namespace media::ffmpeg::graph
