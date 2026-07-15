#include "internal/graph/planner/audio/capability/MediaAudioResamplerCapabilityProvider.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

struct ResamplerStateBound final {
    std::int64_t delay;
    int outputSamples;
    friend bool operator==(const ResamplerStateBound&,
                           const ResamplerStateBound&) = default;
};

::media::Result<int> steadyStateOutputBound(
    SwrContext* context, const MediaSelectedAudioDecoder& decoder,
    const MediaResolvedAudioOutputPlan& output,
    AVSampleFormat inputFormat, AVSampleFormat outputFormat)
{
    if (decoder.maximumOutputBlockInputSamples >
        std::numeric_limits<int>::max()) {
        return ::media::Result<int>::failure(::media::ErrorInfo::unsupported(
            "selected audio resampler input block exceeds FFmpeg capacity"));
    }
    const int inputSamples =
        static_cast<int>(decoder.maximumOutputBlockInputSamples);
    const std::int64_t phaseProduct =
        static_cast<std::int64_t>(inputSamples) * output.sampleRate();
    const std::int64_t phaseLength = decoder.outputSampleRate /
        std::gcd<std::int64_t>(decoder.outputSampleRate, phaseProduct);
    constexpr std::int64_t MaximumProofPhaseLength = 4096;
    if (phaseLength <= 0 || phaseLength > MaximumProofPhaseLength) {
        return ::media::Result<int>::failure(::media::ErrorInfo::unsupported(
            "selected audio resampler phase bound is not provable"));
    }

    auto input = ::media::ffmpeg::makeFrame();
    if (!input) {
        return ::media::Result<int>::failure(::media::ErrorInfo::allocationFailed(
            "selected audio resampler input proof allocation failed"));
    }
    input->format = inputFormat;
    input->sample_rate = decoder.outputSampleRate;
    input->nb_samples = inputSamples;
    if (av_channel_layout_from_string(
            &input->ch_layout, decoder.outputChannelLayout.c_str()) < 0 ||
        av_frame_get_buffer(input.get(), 0) < 0) {
        return ::media::Result<int>::failure(::media::ErrorInfo::unsupported(
            "selected audio resampler input proof buffer is unavailable"));
    }

    const std::int64_t exactDelayBase = std::lcm<std::int64_t>(
        decoder.outputSampleRate, output.sampleRate());
    const auto runPhase = [&](bool collect)
        -> ::media::Result<std::vector<ResamplerStateBound>> {
        std::vector<ResamplerStateBound> states;
        if (collect) states.reserve(static_cast<std::size_t>(phaseLength));
        for (std::int64_t index = 0; index < phaseLength; ++index) {
            const int bound = swr_get_out_samples(context, inputSamples);
            const auto delay = swr_get_delay(context, exactDelayBase);
            if (bound <= 0 || delay < 0) {
                return ::media::Result<std::vector<ResamplerStateBound>>::failure(
                    ::media::ErrorInfo::unsupported(
                        "selected audio resampler does not publish a state bound"));
            }
            if (collect) states.push_back(ResamplerStateBound{delay, bound});
            auto converted = ::media::ffmpeg::makeFrame();
            if (!converted) {
                return ::media::Result<std::vector<ResamplerStateBound>>::failure(
                    ::media::ErrorInfo::allocationFailed(
                        "selected audio resampler output proof allocation failed"));
            }
            converted->format = outputFormat;
            converted->sample_rate = output.sampleRate();
            converted->nb_samples = bound;
            if (av_channel_layout_from_string(
                    &converted->ch_layout, output.channelLayout().c_str()) < 0 ||
                av_frame_get_buffer(converted.get(), 0) < 0) {
                return ::media::Result<std::vector<ResamplerStateBound>>::failure(
                    ::media::ErrorInfo::unsupported(
                        "selected audio resampler output proof buffer is unavailable"));
            }
            const int produced = swr_convert(
                context, converted->data, bound,
                const_cast<const std::uint8_t**>(input->extended_data),
                inputSamples);
            if (produced < 0 || produced > bound) {
                return ::media::Result<std::vector<ResamplerStateBound>>::failure(
                    ::media::ErrorInfo::unsupported(
                        "selected audio resampler proof conversion failed"));
            }
        }
        return ::media::Result<std::vector<ResamplerStateBound>>::success(
            std::move(states));
    };

    if (auto primed = runPhase(false); !primed) {
        return ::media::Result<int>::failure(primed.error());
    }
    auto first = runPhase(true);
    auto second = runPhase(true);
    if (!first || !second || first.value() != second.value()) {
        return ::media::Result<int>::failure(
            !first ? first.error() : !second ? second.error()
                : ::media::ErrorInfo::unsupported(
                      "selected audio resampler steady-state bound is not periodic"));
    }
    int maximumOutput = 0;
    for (const auto& state : first.value()) {
        maximumOutput = std::max(maximumOutput, state.outputSamples);
    }
    return ::media::Result<int>::success(maximumOutput);
}

} // namespace

::media::Result<MediaSelectedAudioResampler>
MediaAudioResamplerCapabilityProvider::verify(
    const MediaSelectedAudioDecoder& decoder,
    const MediaResolvedAudioOutputPlan& output)
{
    const AVSampleFormat inputFormat = av_get_sample_fmt(
        decoder.outputSampleFormat.c_str());
    const AVSampleFormat outputFormat = av_get_sample_fmt(
        output.sampleFormat().c_str());
    AVChannelLayout inputLayout{};
    AVChannelLayout outputLayout{};
    if (decoder.inputSampleRate <= 0 || decoder.outputSampleRate <= 0 ||
        decoder.maximumOutputBlockInputSamples <= 0 || output.sampleRate() <= 0 ||
        inputFormat == AV_SAMPLE_FMT_NONE || outputFormat == AV_SAMPLE_FMT_NONE ||
        av_channel_layout_from_string(&inputLayout,
                                      decoder.outputChannelLayout.c_str()) < 0 ||
        av_channel_layout_from_string(&outputLayout,
                                      output.channelLayout().c_str()) < 0) {
        av_channel_layout_uninit(&inputLayout);
        av_channel_layout_uninit(&outputLayout);
        return ::media::Result<MediaSelectedAudioResampler>::failure(
            ::media::ErrorInfo::unsupported(
                "selected audio resampler format is incomplete"));
    }
    SwrContext* raw = nullptr;
    const int allocated = swr_alloc_set_opts2(
        &raw, &outputLayout, outputFormat, output.sampleRate(),
        &inputLayout, inputFormat, decoder.outputSampleRate, 0, nullptr);
    av_channel_layout_uninit(&inputLayout);
    av_channel_layout_uninit(&outputLayout);
    ::media::ffmpeg::SwrContextPtr context(raw);
    if (allocated < 0 || !context || swr_init(context.get()) < 0) {
        return ::media::Result<MediaSelectedAudioResampler>::failure(
            ::media::ErrorInfo::unsupported(
                "selected audio resampler cannot initialize"));
    }
    auto maximumOutput = steadyStateOutputBound(
        context.get(), decoder, output, inputFormat, outputFormat);
    if (!maximumOutput) {
        return ::media::Result<MediaSelectedAudioResampler>::failure(
            maximumOutput.error());
    }
    return ::media::Result<MediaSelectedAudioResampler>::success(
        MediaSelectedAudioResampler{
            decoder.outputSampleRate, output.sampleRate(),
            decoder.maximumOutputBlockInputSamples, maximumOutput.value()});
}

} // namespace media::ffmpeg::graph
