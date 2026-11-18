#include "internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h"

#include <limits>
#include "internal/graph/time/MediaSourceClockProjection.h"

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
    if (auto status = snapshot.validateMembers(snapshot.members); !status)
        return ::media::Result<MediaPacketSourceTiming>::failure(status.error());
    if ((stream != MediaScheduledStream::Video && stream != MediaScheduledStream::Audio) ||
        (stream == MediaScheduledStream::Audio && snapshot.members != MediaTranscodeStreamSet::AudioVideo) ||
        snapshot.state != MediaRtpClockGroupState::Locked || !snapshot.locked ||
        snapshot.groupGeneration == 0) {
        return ::media::Result<MediaPacketSourceTiming>::failure(
            invalid("RTP packet clock projection requires a nonzero locked group snapshot"));
    }
    const MediaRtpSourceClockCalibration& calibration =
        stream == MediaScheduledStream::Video
        ? snapshot.locked->video
        : *snapshot.locked->audio;
    if (calibration.confidence != MediaRtpSourceClockConfidence::Locked ||
        calibration.rateDenominatorTicks <= 0 ||
        extendedRtpTimestamp >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return ::media::Result<MediaPacketSourceTiming>::failure(
            invalid("RTP packet clock projection requires locked representable calibration"));
    }
    const MediaSourceClockProjection projection{
        calibration.extendedRtpAnchor, calibration.continuousSourceAnchor,
        calibration.rateNumeratorNs, calibration.rateDenominatorTicks,
        snapshot.locked->commonSourceEpoch};
    auto projected = projection.project(static_cast<std::int64_t>(extendedRtpTimestamp));
    if (!projected)
        return ::media::Result<MediaPacketSourceTiming>::failure(projected.error());
    const auto nanoseconds = projected.value().nanoseconds();
    return ::media::Result<MediaPacketSourceTiming>::success(
        MediaPacketSourceTiming{
            nanoseconds,
            nanoseconds,
            MediaSourceClockReadiness::Locked,
            snapshot.groupGeneration});
}

} // namespace media::ffmpeg::graph
