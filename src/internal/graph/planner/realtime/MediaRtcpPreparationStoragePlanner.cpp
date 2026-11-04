#include "internal/graph/planner/realtime/MediaRtcpPreparationStoragePlanner.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRtcpPreparationStoragePlan> MediaRtcpPreparationStoragePlanner::plan(
    std::span<const std::size_t> datagrams, std::size_t queued, MediaRtcpPreparationUse use)
{
    using R = ::media::Result<MediaRtcpPreparationStoragePlan>;
    if (datagrams.empty() || (use != MediaRtcpPreparationUse::ParseOnly &&
        use != MediaRtcpPreparationUse::SourceClock) ||
        (use == MediaRtcpPreparationUse::ParseOnly && queued))
        return R::failure(::media::ErrorInfo::invalidArgument("RTCP storage requires explicit receiver ownership"));
    std::size_t cnameBytes = 0;
    for (const auto maximum : datagrams) {
        if (!maximum) return R::failure(::media::ErrorInfo::invalidArgument("RTCP storage requires datagram bounds"));
        // RFC 3550 SDES: common header(4), SSRC(4), item type/length(2),
        // terminator(1), then word alignment; item length itself is 8 bits.
        const auto aligned = maximum - maximum % 4;
        if (aligned >= 12) cnameBytes = (std::max)(cnameBytes,
            (std::min)(static_cast<std::size_t>((std::numeric_limits<std::uint8_t>::max)()), aligned - 11));
    }
    // Parser owns only views/scalars on the call stack; neither raw capture nor
    // protocol observation allocates a descriptor tree or copies item payloads.
    if (use == MediaRtcpPreparationUse::ParseOnly || !cnameBytes)
        return R::success({0, cnameBytes, 0, 0});
    const auto streams = datagrams.size();
    if (streams > 2) return R::failure(::media::ErrorInfo::invalidArgument(
        "Source clock storage requires its video and optional audio members"));
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::add(a, b, "RTCP owning CNAME overlap");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    const auto mul = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::multiply(a, b, "RTCP owning CNAME overlap");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    // Per stream: tracker current+pending, mapper calibration, validator
    // evidence+calibration. Each binder additionally retains the whole group
    // snapshot (group CNAME plus each member calibration).
    const auto tracker = mul(streams, 2);
    const auto mapper = streams;
    const auto validator = mul(streams, 2);
    const auto binders = mul(streams, add(streams, 1));
    auto owners = add(add(tracker, mapper), add(validator, binders));
    owners = add(owners, queued);
    // Source mutex serializes callbacks. Publication overlaps its event owner,
    // local snapshot (members+group CNAME), and one copy-assignment replacement.
    // The other stages need at most one tracker replacement or three owners
    // (event, returned calibration, validator evidence replacement).
    const auto publication = add(add(streams, 1), 2);
    owners = add(owners, (std::max)(std::uint64_t{3}, publication));
    auto allocation = MediaPreparationMemoryResource::allocationFootprint(cnameBytes, alignof(std::uint8_t));
    if (!allocation) return R::failure(allocation.error());
    const auto peak = mul(owners, allocation.value());
    if (failure || peak > (std::numeric_limits<std::size_t>::max)() ||
        owners > (std::numeric_limits<std::size_t>::max)())
        return R::failure(failure ? failure : ::media::ErrorInfo::allocationFailed("RTCP storage size overflows"));
    return R::success({0, cnameBytes, static_cast<std::size_t>(owners), static_cast<std::size_t>(peak)});
}

} // namespace media::ffmpeg::graph
