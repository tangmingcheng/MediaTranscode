#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

::media::ErrorInfo invalid(const char* fact, const char* detail)
{
    return ::media::ErrorInfo::invalidArgument(
        std::string(fact) + " " + detail);
}

} // namespace

::media::Result<std::uint64_t> MediaCheckedArithmetic::add(
    std::uint64_t left, std::uint64_t right, const char* fact)
{
    if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
        return ::media::Result<std::uint64_t>::failure(
            invalid(fact, "is not representable"));
    }
    return ::media::Result<std::uint64_t>::success(left + right);
}

::media::Result<std::uint64_t> MediaCheckedArithmetic::multiply(
    std::uint64_t left, std::uint64_t right, const char* fact)
{
    if (left != 0 &&
        right > (std::numeric_limits<std::uint64_t>::max)() / left) {
        return ::media::Result<std::uint64_t>::failure(
            invalid(fact, "is not representable"));
    }
    return ::media::Result<std::uint64_t>::success(left * right);
}

::media::Result<std::uint64_t> MediaCheckedArithmetic::ceilScale(
    std::uint64_t value,
    std::uint64_t numerator,
    std::uint64_t denominator,
    const char* fact)
{
    if (numerator == 0 || denominator == 0) {
        return ::media::Result<std::uint64_t>::failure(
            invalid(fact, "has an invalid ratio"));
    }
    const auto product = multiplyWide(value, numerator);
    const auto maximumProduct = multiplyWide(
        (std::numeric_limits<std::uint64_t>::max)(), denominator);
    if (!(maximumProduct >= product)) {
        return ::media::Result<std::uint64_t>::failure(
            invalid(fact, "is not representable"));
    }
    std::uint64_t first = 0;
    std::uint64_t last = (std::numeric_limits<std::uint64_t>::max)();
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        if (multiplyWide(middle, denominator) >= product) {
            last = middle;
        } else {
            first = middle + 1;
        }
    }
    return ::media::Result<std::uint64_t>::success(first);
}

::media::Result<std::uint64_t> MediaCheckedArithmetic::bytesForResidence(
    std::uint64_t rate,
    std::int64_t residenceNanoseconds,
    const char* fact)
{
    if (rate == 0 || residenceNanoseconds <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            invalid(fact, "requires a positive rate and residence"));
    }
    const auto nanoseconds = static_cast<std::uint64_t>(residenceNanoseconds);
    const auto seconds = nanoseconds / NanosecondsPerSecond;
    const auto remainder = nanoseconds % NanosecondsPerSecond;
    auto whole = multiply(rate, seconds, fact);
    auto fraction = ceilScale(rate, remainder, NanosecondsPerSecond, fact);
    return whole && fraction
        ? add(whole.value(), fraction.value(), fact)
        : (!whole ? whole : fraction);
}

::media::Result<std::int64_t>
MediaCheckedArithmetic::ceilDurationNanoseconds(
    std::uint64_t bytes,
    std::uint64_t bytesPerSecond,
    const char* fact)
{
    if (bytes == 0 || bytesPerSecond == 0) {
        return ::media::Result<std::int64_t>::failure(
            invalid(fact, "requires positive bytes and rate"));
    }
    const auto required = multiplyWide(bytes, NanosecondsPerSecond);
    const auto maximum = multiplyWide(
        static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)()),
        bytesPerSecond);
    if (!(maximum >= required)) {
        return ::media::Result<std::int64_t>::failure(
            invalid(fact, "is not representable"));
    }
    std::uint64_t first = 1;
    std::uint64_t last = static_cast<std::uint64_t>(
        (std::numeric_limits<std::int64_t>::max)());
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        if (multiplyWide(middle, bytesPerSecond) >= required) {
            last = middle;
        } else {
            first = middle + 1;
        }
    }
    return ::media::Result<std::int64_t>::success(
        static_cast<std::int64_t>(first));
}

} // namespace media::ffmpeg::graph
