#include "internal/graph/nodes/audio/AudioResampleSwrSession.h"

#include "internal/graph/nodes/audio/AudioResampleLineageState.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

namespace media::ffmpeg::graph {

AudioResampleSwrSession::AudioResampleSwrSession(
    std::shared_ptr<AudioResampleLineageState> state) noexcept
    : m_state(std::move(state))
{
}

bool AudioResampleSwrSession::initialized() const noexcept
{
    return m_state && m_state->swr != nullptr;
}

bool AudioResampleSwrSession::frameMatchesTarget(
    const AVFrame& input,
    const AVCodecContext& target) const noexcept
{
    if (input.format != target.sample_fmt ||
        input.sample_rate != target.sample_rate) {
        return false;
    }
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return av_channel_layout_compare(&input.ch_layout, &target.ch_layout) == 0;
#else
    const int inputChannels = input.channels > 0
        ? input.channels
        : av_get_channel_layout_nb_channels(input.channel_layout);
    const int targetChannels = target.channels > 0
        ? target.channels
        : av_get_channel_layout_nb_channels(target.channel_layout);
    return inputChannels == targetChannels &&
           input.channel_layout == target.channel_layout;
#endif
}

::media::Status AudioResampleSwrSession::ensureInitialized(
    const AVFrame& input,
    const AVCodecContext& target)
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleSwrSession requires planned state"));
    }
    if (m_state->swr) return ::media::Status::success();
    if (input.sample_rate <= 0 || target.sample_rate <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleSwrSession requires known sample rates"));
    }
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if (input.ch_layout.nb_channels <= 0 || target.ch_layout.nb_channels <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleSwrSession requires known channel layouts"));
    }
    SwrContext* raw = nullptr;
    const int allocRet = swr_alloc_set_opts2(
        &raw, &target.ch_layout, target.sample_fmt, target.sample_rate,
        &input.ch_layout, static_cast<AVSampleFormat>(input.format),
        input.sample_rate, 0, nullptr);
    ::media::ffmpeg::SwrContextPtr candidate(raw);
    if (allocRet < 0) {
        return FFmpegGraphError::statusFromCode(
            allocRet, "swr_alloc_set_opts2(audio)");
    }
#else
    const int64_t inputLayout = input.channel_layout
        ? input.channel_layout
        : av_get_default_channel_layout(input.channels);
    const int64_t targetLayout = target.channel_layout
        ? target.channel_layout
        : av_get_default_channel_layout(target.channels);
    if (!inputLayout || !targetLayout) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleSwrSession requires known channel layouts"));
    }
    ::media::ffmpeg::SwrContextPtr candidate(swr_alloc_set_opts(
        nullptr, targetLayout, target.sample_fmt, target.sample_rate,
        inputLayout, static_cast<AVSampleFormat>(input.format),
        input.sample_rate, 0, nullptr));
    if (!candidate) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "swr_alloc_set_opts(audio)"));
    }
#endif
    const int initRet = swr_init(candidate.get());
    if (initRet < 0) {
        return FFmpegGraphError::statusFromCode(initRet, "swr_init(audio)");
    }
    m_state->swr = std::move(candidate);
    return ::media::Status::success();
}

::media::Result<::media::ffmpeg::FramePtr>
AudioResampleSwrSession::allocateOutputFrame(
    int capacity,
    const AVCodecContext& target) const
{
    if (capacity <= 0) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioResampleSwrSession output capacity must be positive"));
    }
    auto output = ::media::ffmpeg::makeFrame();
    if (!output) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            ::media::ErrorInfo::allocationFailed(
                "AudioResampleSwrSession failed to allocate output frame"));
    }
    output->format = target.sample_fmt;
    output->sample_rate = target.sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const int layoutRet = av_channel_layout_copy(
        &output->ch_layout, &target.ch_layout);
    if (layoutRet < 0) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            FFmpegGraphError::fromCode(
                layoutRet, "av_channel_layout_copy(audio resample)"));
    }
#else
    output->channel_layout = target.channel_layout;
    output->channels = target.channels;
#endif
    output->nb_samples = capacity;
    const int bufferRet = av_frame_get_buffer(output.get(), 0);
    if (bufferRet < 0) {
        return ::media::Result<::media::ffmpeg::FramePtr>::failure(
            FFmpegGraphError::fromCode(
                bufferRet, "av_frame_get_buffer(audio resample)"));
    }
    return ::media::Result<::media::ffmpeg::FramePtr>::success(
        std::move(output));
}

::media::Result<AudioResampleSwrLiveConversion>
AudioResampleSwrSession::convertLive(
    const uint8_t** inputData,
    int inputSamples,
    int maximumOutputSamples,
    const AVCodecContext& target)
{
    if (!initialized() || !inputData || inputSamples < 0 ||
        maximumOutputSamples < 0) {
        return ::media::Result<AudioResampleSwrLiveConversion>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioResampleSwrSession live conversion requires input and initialized bounds"));
    }
    const int available = swr_get_out_samples(m_state->swr.get(), inputSamples);
    if (available < 0) {
        return ::media::Result<AudioResampleSwrLiveConversion>::failure(
            FFmpegGraphError::fromCode(
                available, "swr_get_out_samples(audio)"));
    }
    AudioResampleSwrLiveConversion result;
    result.capacity = std::min(available, maximumOutputSamples);
    if (result.capacity <= 0) {
        return ::media::Result<AudioResampleSwrLiveConversion>::success(
            std::move(result));
    }
    auto output = allocateOutputFrame(result.capacity, target);
    if (!output) {
        return ::media::Result<AudioResampleSwrLiveConversion>::failure(
            output.error());
    }
    result.output = std::move(output).value();
    result.produced = swr_convert(
        m_state->swr.get(), result.output->data, result.capacity,
        inputData, inputSamples);
    if (result.produced < 0) {
        return ::media::Result<AudioResampleSwrLiveConversion>::failure(
            FFmpegGraphError::fromCode(
                result.produced, "swr_convert(audio)"));
    }
    result.output->nb_samples = result.produced;
    return ::media::Result<AudioResampleSwrLiveConversion>::success(
        std::move(result));
}

::media::Result<AudioSwrDrainEvidence>
AudioResampleSwrSession::inspectDrainEvidence(
    int inputSampleRate,
    int outputSampleRate) const
{
    if (!initialized() || inputSampleRate <= 0 || outputSampleRate <= 0) {
        return ::media::Result<AudioSwrDrainEvidence>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioResampleSwrSession drain evidence requires valid SWR rates"));
    }
    const auto divisor = std::gcd(inputSampleRate, outputSampleRate);
    const auto reducedInput = static_cast<std::int64_t>(
        inputSampleRate / divisor);
    if (reducedInput > std::numeric_limits<std::int64_t>::max() /
                           outputSampleRate) {
        return ::media::Result<AudioSwrDrainEvidence>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioResampleSwrSession exact drain evidence base overflows"));
    }
    const auto exactBase = reducedInput * outputSampleRate;
    const auto delay = swr_get_delay(m_state->swr.get(), exactBase);
    if (delay < 0) {
        return ::media::Result<AudioSwrDrainEvidence>::failure(
            ::media::ErrorInfo::internalError(
                "AudioResampleSwrSession drain delay is negative"));
    }
    return ::media::Result<AudioSwrDrainEvidence>::success(
        delay == 0 ? AudioSwrDrainEvidence::NoDelay
                   : AudioSwrDrainEvidence::MayProduce);
}

::media::Result<AudioResampleSwrDrainConversion>
AudioResampleSwrSession::drainQuantum(
    int maximumOutputSamples,
    const AVCodecContext& target)
{
    if (!initialized() || maximumOutputSamples <= 0) {
        return ::media::Result<AudioResampleSwrDrainConversion>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioResampleSwrSession drain requires positive planned capacity"));
    }
    const int upperBound = swr_get_out_samples(m_state->swr.get(), 0);
    if (upperBound < 0) {
        return ::media::Result<AudioResampleSwrDrainConversion>::failure(
            FFmpegGraphError::fromCode(
                upperBound, "swr_get_out_samples(audio drain)"));
    }
    AudioResampleSwrDrainConversion result;
    result.capacity = std::min(std::max(upperBound, 1), maximumOutputSamples);
    auto output = allocateOutputFrame(result.capacity, target);
    if (!output) {
        return ::media::Result<AudioResampleSwrDrainConversion>::failure(
            output.error());
    }
    result.output = std::move(output).value();
    result.produced = swr_convert(
        m_state->swr.get(), result.output->data, result.capacity,
        nullptr, 0);
    if (result.produced < 0) {
        return ::media::Result<AudioResampleSwrDrainConversion>::failure(
            FFmpegGraphError::fromCode(
                result.produced, "swr_convert(audio drain)"));
    }
    if (result.produced == 0) {
        result.exhausted = AudioSwrResamplerExhausted{};
    }
    result.output->nb_samples = result.produced;
    return ::media::Result<AudioResampleSwrDrainConversion>::success(
        std::move(result));
}

} // namespace media::ffmpeg::graph
