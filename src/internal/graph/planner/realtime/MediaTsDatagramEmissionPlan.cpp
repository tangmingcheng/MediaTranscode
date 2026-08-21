#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h"

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t RtpHeaderBytes = 12;

} // namespace

::media::Result<MediaTsDatagramEmissionPlan>
MediaTsDatagramEmissionPlan::create(
    const MediaTsMuxPlan& muxPlan,
    MediaRunningTime videoAccessUnitCadence,
    std::optional<MediaRunningTime> audioAccessUnitCadence,
    std::uint64_t maximumQueuedBytes,
    std::optional<std::int64_t> scheduledWireBytesPerSecond)
{
    const auto& mux = muxPlan.parameters();
    const std::size_t maximumPayloadBytes =
        static_cast<std::size_t>(mux.maximumPacketsPerDatagram) *
        mux.packetSize;
    const std::size_t perDatagramOverheadBytes =
        mux.transportKind == MediaOutputTransportKind::RtpAvp
            ? RtpHeaderBytes
            : 0;
    const bool hasAudio = muxPlan.audioVideoProgram() != nullptr;
    const std::int64_t streamCount = hasAudio ? 2 : 1;
    const auto fairServiceWindow = MediaRunningTime::fromNanoseconds(
        mux.transportDecodeLead.nanoseconds() / streamCount);
    if (mux.transportDecodeLead.nanoseconds() <= 0 ||
        fairServiceWindow.nanoseconds() <= 0 ||
        videoAccessUnitCadence.nanoseconds() <= 0 ||
        hasAudio != audioAccessUnitCadence.has_value() ||
        (audioAccessUnitCadence &&
         audioAccessUnitCadence->nanoseconds() <= 0) ||
         maximumPayloadBytes == 0 ||
        maximumQueuedBytes == 0 ||
        ((mux.transportKind == MediaOutputTransportKind::RtpAvp) !=
         scheduledWireBytesPerSecond.has_value()) ||
        (scheduledWireBytesPerSecond &&
         *scheduledWireBytesPerSecond <= 0) ||
        maximumPayloadBytes >
            (std::numeric_limits<std::size_t>::max)() -
                perDatagramOverheadBytes) {
        return ::media::Result<MediaTsDatagramEmissionPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS datagram emission facts are incomplete or not representable"));
    }
    return ::media::Result<MediaTsDatagramEmissionPlan>::success(
        MediaTsDatagramEmissionPlan(
            mux.transportDecodeLead,
            (std::min)(videoAccessUnitCadence, fairServiceWindow),
            audioAccessUnitCadence
                ? std::optional<MediaRunningTime>((std::min)(
                      *audioAccessUnitCadence, fairServiceWindow))
                : std::nullopt,
            mux.packetSize,
            maximumPayloadBytes, perDatagramOverheadBytes,
            mux.transportKind == MediaOutputTransportKind::RtpAvp,
            maximumQueuedBytes,
            scheduledWireBytesPerSecond));
}

MediaTsDatagramEmissionPlan::MediaTsDatagramEmissionPlan(
    MediaRunningTime accessUnitWindow,
    MediaRunningTime videoInitialServiceWindow,
    std::optional<MediaRunningTime> audioInitialServiceWindow,
    std::size_t packetSizeBytes,
    std::size_t maximumPayloadBytes,
    std::size_t perDatagramOverheadBytes,
    bool scheduledDatagramOutput,
    std::uint64_t maximumQueuedBytes,
    std::optional<std::int64_t> scheduledWireBytesPerSecond) noexcept
    : m_accessUnitWindow(accessUnitWindow)
    , m_videoInitialServiceWindow(videoInitialServiceWindow)
    , m_audioInitialServiceWindow(std::move(audioInitialServiceWindow))
    , m_packetSizeBytes(packetSizeBytes)
    , m_maximumPayloadBytes(maximumPayloadBytes)
    , m_perDatagramOverheadBytes(perDatagramOverheadBytes)
    , m_scheduledDatagramOutput(scheduledDatagramOutput)
    , m_maximumQueuedBytes(maximumQueuedBytes)
    , m_scheduledWireBytesPerSecond(scheduledWireBytesPerSecond)
{
}

MediaRunningTime MediaTsDatagramEmissionPlan::accessUnitWindow() const noexcept
{
    return m_accessUnitWindow;
}

std::size_t MediaTsDatagramEmissionPlan::packetSizeBytes() const noexcept
{
    return m_packetSizeBytes;
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

bool MediaTsDatagramEmissionPlan::usesScheduledDatagramOutput() const noexcept
{
    return m_scheduledDatagramOutput;
}

MediaRunningTime
MediaTsDatagramEmissionPlan::videoInitialServiceWindow() const noexcept
{
    return m_videoInitialServiceWindow;
}

const std::optional<MediaRunningTime>&
MediaTsDatagramEmissionPlan::audioInitialServiceWindow() const noexcept
{
    return m_audioInitialServiceWindow;
}

std::uint64_t MediaTsDatagramEmissionPlan::maximumQueuedBytes() const noexcept
{
    return m_maximumQueuedBytes;
}

const std::optional<std::int64_t>&
MediaTsDatagramEmissionPlan::scheduledWireBytesPerSecond() const noexcept
{
    return m_scheduledWireBytesPerSecond;
}

} // namespace media::ffmpeg::graph
