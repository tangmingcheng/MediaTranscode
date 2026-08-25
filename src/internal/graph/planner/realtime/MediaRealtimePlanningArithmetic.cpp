#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

::media::ErrorInfo overflow(const char* fact)
{
    return ::media::ErrorInfo::invalidArgument(
        std::string(fact) + " is not representable");
}

} // namespace

::media::Result<std::uint64_t> MediaRealtimePlanningArithmetic::add(
    std::uint64_t left, std::uint64_t right, const char* fact)
{
    if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
        return ::media::Result<std::uint64_t>::failure(overflow(fact));
    }
    return ::media::Result<std::uint64_t>::success(left + right);
}

::media::Result<std::uint64_t> MediaRealtimePlanningArithmetic::multiply(
    std::uint64_t left, std::uint64_t right, const char* fact)
{
    if (left != 0 &&
        right > (std::numeric_limits<std::uint64_t>::max)() / left) {
        return ::media::Result<std::uint64_t>::failure(overflow(fact));
    }
    return ::media::Result<std::uint64_t>::success(left * right);
}

::media::Result<std::uint64_t> MediaRealtimePlanningArithmetic::ceilScale(
    std::uint64_t value,
    std::uint64_t numerator,
    std::uint64_t denominator,
    const char* fact)
{
    if (numerator == 0 || denominator == 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " has an invalid ratio"));
    }
    auto product = multiply(value, numerator, fact);
    if (!product) return product;
    return ::media::Result<std::uint64_t>::success(
        product.value() / denominator +
        (product.value() % denominator != 0 ? 1U : 0U));
}

::media::Result<std::uint64_t>
MediaRealtimePlanningArithmetic::bytesForResidence(
    std::uint64_t rate,
    MediaRunningTime residence,
    const char* fact)
{
    if (rate == 0 || residence.nanoseconds() <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " requires a positive rate and residence"));
    }
    const auto nanoseconds = static_cast<std::uint64_t>(
        residence.nanoseconds());
    const auto seconds = nanoseconds / NanosecondsPerSecond;
    const auto remainder = nanoseconds % NanosecondsPerSecond;
    auto whole = multiply(rate, seconds, fact);
    auto fraction = ceilScale(rate, remainder, NanosecondsPerSecond, fact);
    return whole && fraction
        ? add(whole.value(), fraction.value(), fact)
        : (!whole ? whole : fraction);
}

} // namespace media::ffmpeg::graph
