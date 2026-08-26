#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaUInt128 final {
    std::uint64_t high = 0;
    std::uint64_t low = 0;

    friend constexpr bool operator>=(MediaUInt128 left,
                                     MediaUInt128 right) noexcept
    {
        return left.high > right.high ||
               (left.high == right.high && left.low >= right.low);
    }
};

class MediaCheckedArithmetic final {
public:
    static constexpr MediaUInt128 multiplyWide(
        std::uint64_t left, std::uint64_t right) noexcept
    {
        const std::uint64_t leftLow = static_cast<std::uint32_t>(left);
        const std::uint64_t leftHigh = left >> 32;
        const std::uint64_t rightLow = static_cast<std::uint32_t>(right);
        const std::uint64_t rightHigh = right >> 32;
        const std::uint64_t lowProduct = leftLow * rightLow;
        const std::uint64_t firstCross =
            leftHigh * rightLow + (lowProduct >> 32);
        const std::uint64_t secondCross =
            leftLow * rightHigh + static_cast<std::uint32_t>(firstCross);
        return {
            leftHigh * rightHigh + (firstCross >> 32) +
                (secondCross >> 32),
            (secondCross << 32) + static_cast<std::uint32_t>(lowProduct)};
    }

    static constexpr std::uint64_t divideWideBy32(
        MediaUInt128 dividend, std::uint32_t divisor) noexcept
    {
        const std::uint32_t limbs[4] = {
            static_cast<std::uint32_t>(dividend.high >> 32),
            static_cast<std::uint32_t>(dividend.high),
            static_cast<std::uint32_t>(dividend.low >> 32),
            static_cast<std::uint32_t>(dividend.low)};
        std::uint64_t remainder = 0;
        std::uint64_t quotient = 0;
        for (int index = 0; index < 4; ++index) {
            const std::uint64_t partial = (remainder << 32) | limbs[index];
            const auto quotientLimb = static_cast<std::uint32_t>(
                partial / divisor);
            remainder = partial % divisor;
            if (index >= 2) {
                quotient = (quotient << 32) | quotientLimb;
            }
        }
        return quotient;
    }

    static ::media::Result<std::uint64_t> add(
        std::uint64_t left, std::uint64_t right, const char* fact);
    static ::media::Result<std::uint64_t> multiply(
        std::uint64_t left, std::uint64_t right, const char* fact);
    static ::media::Result<std::uint64_t> ceilScale(
        std::uint64_t value,
        std::uint64_t numerator,
        std::uint64_t denominator,
        const char* fact);
    static ::media::Result<std::uint64_t> bytesForResidence(
        std::uint64_t rate,
        std::int64_t residenceNanoseconds,
        const char* fact);
    static ::media::Result<std::int64_t> ceilDurationNanoseconds(
        std::uint64_t bytes,
        std::uint64_t bytesPerSecond,
        const char* fact);

private:
    MediaCheckedArithmetic() = delete;
};

} // namespace media::ffmpeg::graph
