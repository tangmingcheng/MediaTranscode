#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t TsPacketBytes = 188;
constexpr std::size_t RtpHeaderBytes = 12;

} // namespace

::media::Result<MediaTsDatagramEmissionPlan>
MediaTsDatagramEmissionPlan::create(
    std::int64_t wireBytesPerSecond,
    std::size_t burstWireBytes,
    MediaRunningTime maximumLateness,
    std::size_t maximumPayloadBytes,
    std::size_t perDatagramOverheadBytes)
{
    const bool supportedOverhead =
        perDatagramOverheadBytes == 0 ||
        perDatagramOverheadBytes == RtpHeaderBytes;
    if (wireBytesPerSecond <= 0 ||
        maximumLateness <= MediaRunningTime::fromNanoseconds(0) ||
        maximumPayloadBytes == 0 ||
        (maximumPayloadBytes % TsPacketBytes) != 0 ||
        !supportedOverhead ||
        maximumPayloadBytes >
            (std::numeric_limits<std::size_t>::max)() -
                perDatagramOverheadBytes) {
        return ::media::Result<MediaTsDatagramEmissionPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS datagram emission facts are incomplete or not representable"));
    }
    const std::size_t maximumWireDatagramBytes =
        maximumPayloadBytes + perDatagramOverheadBytes;
    if (burstWireBytes < maximumWireDatagramBytes) {
        return ::media::Result<MediaTsDatagramEmissionPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS datagram emission burst cannot hold one complete datagram"));
    }
    return ::media::Result<MediaTsDatagramEmissionPlan>::success(
        MediaTsDatagramEmissionPlan(
            wireBytesPerSecond, burstWireBytes, maximumLateness,
            maximumPayloadBytes, perDatagramOverheadBytes));
}

MediaTsDatagramEmissionPlan::MediaTsDatagramEmissionPlan(
    std::int64_t wireBytesPerSecond,
    std::size_t burstWireBytes,
    MediaRunningTime maximumLateness,
    std::size_t maximumPayloadBytes,
    std::size_t perDatagramOverheadBytes) noexcept
    : m_wireBytesPerSecond(wireBytesPerSecond)
    , m_burstWireBytes(burstWireBytes)
    , m_maximumLateness(maximumLateness)
    , m_maximumPayloadBytes(maximumPayloadBytes)
    , m_perDatagramOverheadBytes(perDatagramOverheadBytes)
{
}

std::int64_t MediaTsDatagramEmissionPlan::wireBytesPerSecond() const noexcept
{
    return m_wireBytesPerSecond;
}

std::size_t MediaTsDatagramEmissionPlan::burstWireBytes() const noexcept
{
    return m_burstWireBytes;
}

MediaRunningTime MediaTsDatagramEmissionPlan::maximumLateness() const noexcept
{
    return m_maximumLateness;
}

std::size_t MediaTsDatagramEmissionPlan::maximumPayloadBytes() const noexcept
{
    return m_maximumPayloadBytes;
}

std::size_t
MediaTsDatagramEmissionPlan::perDatagramOverheadBytes() const noexcept
{
    return m_perDatagramOverheadBytes;
}

std::size_t
MediaTsDatagramEmissionPlan::maximumWireDatagramBytes() const noexcept
{
    return m_maximumPayloadBytes + m_perDatagramOverheadBytes;
}

} // namespace media::ffmpeg::graph
