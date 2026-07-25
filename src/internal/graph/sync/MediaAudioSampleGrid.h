#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {

class MediaAudioSampleGrid final {
public:
    static ::media::Result<MediaAudioSampleGrid> create(int sampleRate)
    {
        if (sampleRate <= 0) {
            return ::media::Result<MediaAudioSampleGrid>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Audio sample grid requires a positive sample rate"));
        }
        return ::media::Result<MediaAudioSampleGrid>::success(
            MediaAudioSampleGrid(sampleRate));
    }

    ::media::Result<std::int64_t> nearestSample(
        MediaRunningTime time) const
    {
        return quantize(time, Quantization::Nearest);
    }

    ::media::Result<std::int64_t> firstSampleAtOrAfter(
        MediaRunningTime time) const
    {
        return quantize(time, Quantization::Ceiling);
    }

    ::media::Result<std::uint32_t> trimLeadingSamples(
        MediaRunningTime packetStart,
        MediaRunningTime epochSourceStart,
        std::uint32_t availableSamples) const
    {
        auto packetSample = nearestSample(packetStart);
        if (!packetSample) {
            return ::media::Result<std::uint32_t>::failure(
                packetSample.error());
        }
        auto epochSample = firstSampleAtOrAfter(epochSourceStart);
        if (!epochSample) {
            return ::media::Result<std::uint32_t>::failure(
                epochSample.error());
        }
        if (epochSample.value() < packetSample.value()) {
            return ::media::Result<std::uint32_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Audio epoch precedes the selected packet sample grid"));
        }
        const auto distance = static_cast<std::uint64_t>(
            epochSample.value()) - static_cast<std::uint64_t>(
                packetSample.value());
        if (distance > availableSamples) {
            return ::media::Result<std::uint32_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Audio epoch trim exceeds the selected packet span"));
        }
        return ::media::Result<std::uint32_t>::success(
            static_cast<std::uint32_t>(distance));
    }

private:
    enum class Quantization : std::uint8_t {
        Nearest,
        Ceiling
    };

    explicit MediaAudioSampleGrid(int sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    ::media::Result<std::int64_t> quantize(
        MediaRunningTime time,
        Quantization quantization) const
    {
        constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
        constexpr std::int64_t HalfSecondInNanoseconds =
            NanosecondsPerSecond / 2;
        const auto nanoseconds = time.nanoseconds();
        const auto wholeSeconds = nanoseconds / NanosecondsPerSecond;
        const auto remainder = nanoseconds % NanosecondsPerSecond;
        if ((wholeSeconds > 0 &&
             wholeSeconds > std::numeric_limits<std::int64_t>::max() /
                                m_sampleRate) ||
            (wholeSeconds < 0 &&
             wholeSeconds < std::numeric_limits<std::int64_t>::min() /
                                m_sampleRate)) {
            return overflow();
        }
        const auto wholeSamples = wholeSeconds * m_sampleRate;
        const auto fractionalProduct = remainder *
            static_cast<std::int64_t>(m_sampleRate);
        std::int64_t fractionalSamples = 0;
        if (quantization == Quantization::Nearest) {
            fractionalSamples = fractionalProduct >= 0
                ? (fractionalProduct + HalfSecondInNanoseconds) /
                      NanosecondsPerSecond
                : (fractionalProduct - HalfSecondInNanoseconds) /
                      NanosecondsPerSecond;
        } else {
            fractionalSamples = fractionalProduct / NanosecondsPerSecond;
            if (fractionalProduct > 0 &&
                fractionalProduct % NanosecondsPerSecond != 0) {
                ++fractionalSamples;
            }
        }
        if ((fractionalSamples > 0 &&
             wholeSamples > std::numeric_limits<std::int64_t>::max() -
                                fractionalSamples) ||
            (fractionalSamples < 0 &&
             wholeSamples < std::numeric_limits<std::int64_t>::min() -
                                fractionalSamples)) {
            return overflow();
        }
        return ::media::Result<std::int64_t>::success(
            wholeSamples + fractionalSamples);
    }

    static ::media::Result<std::int64_t> overflow()
    {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio sample grid quantization is not representable"));
    }

    int m_sampleRate;
};

} // namespace media::ffmpeg::graph
