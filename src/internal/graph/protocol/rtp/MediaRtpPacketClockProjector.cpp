#include "internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

} // namespace

::media::Result<MediaPacketSourceTiming> MediaRtpPacketClockProjector::project(
    const MediaRtpClockGroupSnapshot& snapshot,
    MediaScheduledStream stream,
    std::uint64_t extendedRtpTimestamp) const
{
    if (snapshot.state != MediaRtpClockGroupState::Locked || !snapshot.locked ||
        snapshot.groupGeneration == 0) {
        return ::media::Result<MediaPacketSourceTiming>::failure(
            invalid("RTP packet clock projection requires a nonzero locked group snapshot"));
    }
    const MediaRtpSourceClockCalibration& calibration =
        stream == MediaScheduledStream::Video
        ? snapshot.locked->video
        : snapshot.locked->audio;
    if (calibration.confidence != MediaRtpSourceClockConfidence::Locked ||
        calibration.rateDenominatorTicks <= 0 ||
        extendedRtpTimestamp >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return ::media::Result<MediaPacketSourceTiming>::failure(
            invalid("RTP packet clock projection requires locked representable calibration"));
    }
    const long double deltaTicks = static_cast<long double>(
        static_cast<std::int64_t>(extendedRtpTimestamp) -
        calibration.extendedRtpAnchor);
    const long double sourceNanoseconds =
        calibration.continuousSourceAnchor.nanoseconds() +
        deltaTicks * calibration.rateNumeratorNs /
            calibration.rateDenominatorTicks;
    const long double normalized = sourceNanoseconds -
        snapshot.locked->commonSourceEpoch.nanoseconds();
    if (normalized < std::numeric_limits<std::int64_t>::min() ||
        normalized > std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaPacketSourceTiming>::failure(
            invalid("RTP packet clock projection is not representable"));
    }
    const auto nanoseconds = static_cast<std::int64_t>(normalized);
    return ::media::Result<MediaPacketSourceTiming>::success(
        MediaPacketSourceTiming{
            nanoseconds,
            nanoseconds,
            MediaSourceClockReadiness::Locked,
            snapshot.groupGeneration});
}

} // namespace media::ffmpeg::graph
