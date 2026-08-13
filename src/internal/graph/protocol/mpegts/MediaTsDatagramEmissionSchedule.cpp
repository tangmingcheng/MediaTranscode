#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

struct UInt128 final {
    std::uint64_t high;
    std::uint64_t low;
};

UInt128 multiply(std::uint64_t lhs, std::uint64_t rhs) noexcept
{
    const std::uint64_t lhsLow = static_cast<std::uint32_t>(lhs);
    const std::uint64_t lhsHigh = lhs >> 32;
    const std::uint64_t rhsLow = static_cast<std::uint32_t>(rhs);
    const std::uint64_t rhsHigh = rhs >> 32;
    const std::uint64_t lowProduct = lhsLow * rhsLow;
    const std::uint64_t firstCross =
        lhsHigh * rhsLow + (lowProduct >> 32);
    const std::uint64_t firstCrossLow =
        static_cast<std::uint32_t>(firstCross);
    const std::uint64_t secondCross =
        lhsLow * rhsHigh + firstCrossLow;
    return UInt128{
        lhsHigh * rhsHigh + (firstCross >> 32) + (secondCross >> 32),
        (secondCross << 32) + static_cast<std::uint32_t>(lowProduct)};
}

struct DivisionResult final {
    std::uint64_t quotient;
    std::uint64_t remainder;
    bool quotientOverflow;
};

DivisionResult divide(UInt128 dividend, std::uint64_t divisor) noexcept
{
    std::uint64_t quotient = 0;
    std::uint64_t remainder = 0;
    bool overflow = false;
    for (int bitIndex = 127; bitIndex >= 0; --bitIndex) {
        const std::uint64_t bit = bitIndex >= 64
            ? (dividend.high >> (bitIndex - 64)) & 1u
            : (dividend.low >> bitIndex) & 1u;
        remainder = (remainder << 1) | bit;
        if (remainder < divisor) continue;
        remainder -= divisor;
        if (bitIndex >= 64) {
            overflow = true;
        } else {
            quotient |= std::uint64_t{1} << bitIndex;
        }
    }
    return DivisionResult{quotient, remainder, overflow};
}

::media::Result<MediaRunningTime> rateDuration(
    std::size_t bytes,
    std::int64_t bytesPerSecond,
    bool roundUp)
{
    if (bytesPerSecond <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission duration requires a positive wire rate"));
    }
    const DivisionResult divided = divide(
        multiply(static_cast<std::uint64_t>(bytes), NanosecondsPerSecond),
        static_cast<std::uint64_t>(bytesPerSecond));
    if (divided.quotientOverflow ||
        divided.quotient >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::int64_t>::max)()) ||
        (roundUp && divided.remainder != 0 &&
         divided.quotient ==
             static_cast<std::uint64_t>(
                 (std::numeric_limits<std::int64_t>::max)()))) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission duration is not representable"));
    }
    const std::uint64_t nanoseconds =
        divided.quotient +
        (roundUp && divided.remainder != 0 ? 1u : 0u);
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(nanoseconds)));
}

} // namespace

class MediaTsDatagramEmissionScheduleState final {
public:
    MediaTsDatagramEmissionScheduleState(
        MediaTsDatagramEmissionPlan selectedPlan,
        MediaRunningTime selectedOrigin) noexcept
        : plan(std::move(selectedPlan))
        , origin(selectedOrigin)
        , virtualFinish(selectedOrigin)
    {
    }

    MediaTsDatagramEmissionPlan plan;
    MediaRunningTime origin;
    MediaRunningTime virtualFinish;
    std::optional<std::uint64_t> pendingRevision;
    std::uint64_t nextRevision = 1;
};

MediaTsPreparedDatagramEmission::MediaTsPreparedDatagramEmission(
    std::shared_ptr<MediaTsDatagramEmissionScheduleState> state,
    std::uint64_t revision,
    MediaRunningTime deadline,
    MediaRunningTime plannedWait,
    MediaRunningTime nextVirtualFinish,
    std::size_t wireBytes) noexcept
    : m_state(std::move(state))
    , m_revision(revision)
    , m_deadline(deadline)
    , m_plannedWait(plannedWait)
    , m_nextVirtualFinish(nextVirtualFinish)
    , m_wireBytes(wireBytes)
    , m_active(true)
{
}

MediaTsPreparedDatagramEmission::~MediaTsPreparedDatagramEmission()
{
    cancel();
}

MediaTsPreparedDatagramEmission::MediaTsPreparedDatagramEmission(
    MediaTsPreparedDatagramEmission&& other) noexcept
    : m_state(std::move(other.m_state))
    , m_revision(other.m_revision)
    , m_deadline(other.m_deadline)
    , m_plannedWait(other.m_plannedWait)
    , m_nextVirtualFinish(other.m_nextVirtualFinish)
    , m_wireBytes(other.m_wireBytes)
    , m_active(other.m_active)
{
    other.m_active = false;
}

MediaTsPreparedDatagramEmission&
MediaTsPreparedDatagramEmission::operator=(
    MediaTsPreparedDatagramEmission&& other) noexcept
{
    if (this == &other) return *this;
    cancel();
    m_state = std::move(other.m_state);
    m_revision = other.m_revision;
    m_deadline = other.m_deadline;
    m_plannedWait = other.m_plannedWait;
    m_nextVirtualFinish = other.m_nextVirtualFinish;
    m_wireBytes = other.m_wireBytes;
    m_active = other.m_active;
    other.m_active = false;
    return *this;
}

MediaRunningTime
MediaTsPreparedDatagramEmission::deadline() const noexcept
{
    return m_deadline;
}

MediaRunningTime
MediaTsPreparedDatagramEmission::plannedWait() const noexcept
{
    return m_plannedWait;
}

std::size_t MediaTsPreparedDatagramEmission::wireBytes() const noexcept
{
    return m_wireBytes;
}

void MediaTsPreparedDatagramEmission::cancel() noexcept
{
    if (m_active && m_state &&
        m_state->pendingRevision == m_revision) {
        m_state->pendingRevision.reset();
    }
    m_active = false;
    m_state.reset();
}

::media::Result<MediaTsDatagramEmissionSchedule>
MediaTsDatagramEmissionSchedule::create(
    MediaTsDatagramEmissionPlan plan,
    MediaRunningTime origin)
{
    return ::media::Result<MediaTsDatagramEmissionSchedule>::success(
        MediaTsDatagramEmissionSchedule(
            std::make_shared<MediaTsDatagramEmissionScheduleState>(
                std::move(plan), origin)));
}

MediaTsDatagramEmissionSchedule::MediaTsDatagramEmissionSchedule(
    std::shared_ptr<MediaTsDatagramEmissionScheduleState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Result<MediaTsPreparedDatagramEmission>
MediaTsDatagramEmissionSchedule::prepare(
    std::size_t payloadBytes,
    MediaRunningTime notBefore)
{
    using Result = ::media::Result<MediaTsPreparedDatagramEmission>;
    if (!m_state || m_state->pendingRevision) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS emission schedule permits one pending transaction"));
    }
    const auto& selectedPlan = m_state->plan;
    if (payloadBytes == 0 ||
        payloadBytes > selectedPlan.maximumPayloadBytes() ||
        payloadBytes >
            (std::numeric_limits<std::size_t>::max)() -
                selectedPlan.perDatagramOverheadBytes() ||
        notBefore < m_state->origin) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS emission preparation contradicts its plan"));
    }
    const std::size_t wireBytes =
        payloadBytes + selectedPlan.perDatagramOverheadBytes();
    auto cost = rateDuration(
        wireBytes, selectedPlan.wireBytesPerSecond(), true);
    auto allowance = rateDuration(
        selectedPlan.burstWireBytes() - wireBytes,
        selectedPlan.wireBytesPerSecond(), false);
    if (!cost || !allowance) {
        return Result::failure(!cost ? cost.error() : allowance.error());
    }

    auto elapsed = m_state->virtualFinish.checkedSubtract(m_state->origin);
    if (!elapsed) return Result::failure(elapsed.error());
    MediaRunningTime earliest = m_state->origin;
    if (elapsed.value() > allowance.value()) {
        auto reduced = m_state->virtualFinish.checkedSubtract(
            allowance.value());
        if (!reduced) return Result::failure(reduced.error());
        earliest = reduced.value();
    }
    const MediaRunningTime deadline = (std::max)(notBefore, earliest);
    auto plannedWait = deadline.checkedSubtract(notBefore);
    if (!plannedWait) return Result::failure(plannedWait.error());
    if (plannedWait.value() > selectedPlan.maximumLateness()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS emission deadline exceeds planned maximum lateness"));
    }
    const MediaRunningTime virtualBase =
        (std::max)(m_state->virtualFinish, deadline);
    auto nextVirtualFinish = virtualBase.checkedAdd(cost.value());
    if (!nextVirtualFinish) {
        return Result::failure(nextVirtualFinish.error());
    }
    if (m_state->nextRevision == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS emission revision space is exhausted"));
    }
    const std::uint64_t revision = m_state->nextRevision++;
    m_state->pendingRevision = revision;
    return Result::success(MediaTsPreparedDatagramEmission(
        m_state, revision, deadline, plannedWait.value(),
        nextVirtualFinish.value(), wireBytes));
}

::media::Status MediaTsDatagramEmissionSchedule::commit(
    MediaTsPreparedDatagramEmission&& prepared)
{
    if (!m_state || !prepared.m_active ||
        prepared.m_state.get() != m_state.get() ||
        m_state->pendingRevision != prepared.m_revision) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission commit rejects a stale or foreign transaction"));
    }
    m_state->virtualFinish = prepared.m_nextVirtualFinish;
    m_state->pendingRevision.reset();
    prepared.m_active = false;
    prepared.m_state.reset();
    return ::media::Status::success();
}

const MediaTsDatagramEmissionPlan&
MediaTsDatagramEmissionSchedule::plan() const noexcept
{
    return m_state->plan;
}

} // namespace media::ffmpeg::graph
