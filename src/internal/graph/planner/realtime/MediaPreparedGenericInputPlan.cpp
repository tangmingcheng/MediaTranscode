#include "internal/graph/planner/realtime/MediaPreparedGenericInputPlan.h"

namespace media::ffmpeg::graph {

::media::Status MediaPreparedGenericInputPlan::validate() const
{
    if (videoStreamIndex < 0 || audioStreamIndex < 0 ||
        videoStreamIndex == audioStreamIndex ||
        videoTimeBase.num <= 0 || videoTimeBase.den <= 0 ||
        audioTimeBase.num <= 0 || audioTimeBase.den <= 0 ||
        timedStartupPrefixDisposition !=
            MediaPreparedTimedStartupPrefixDisposition::
                DiscardEarlierCompleteTimedUntilCommonWindow ||
        videoPacketCapacity == 0 || audioPacketCapacity == 0 ||
        videoByteCapacity == 0 || audioByteCapacity == 0 ||
        maximumVideoPacketBytes == 0 || maximumAudioPacketBytes == 0 ||
        maximumVideoPacketBytes > videoByteCapacity ||
        maximumAudioPacketBytes > audioByteCapacity ||
        maximumPreparedReadDuration <= MediaRunningTime::fromNanoseconds(0) ||
        maximumPreparedHandoffDuration <=
            MediaRunningTime::fromNanoseconds(0) ||
        firstWindowMaximumSkew <= MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "prepared generic input requires a complete positive planner product"));
    }
    return ::media::Status::success();
}

bool operator==(const MediaPreparedGenericInputPlan& left,
                const MediaPreparedGenericInputPlan& right) noexcept
{
    return left.videoStreamIndex == right.videoStreamIndex &&
        left.audioStreamIndex == right.audioStreamIndex &&
        left.videoTimeBase.num == right.videoTimeBase.num &&
        left.videoTimeBase.den == right.videoTimeBase.den &&
        left.audioTimeBase.num == right.audioTimeBase.num &&
        left.audioTimeBase.den == right.audioTimeBase.den &&
        left.leadingVideoDisposition == right.leadingVideoDisposition &&
        left.timedStartupPrefixDisposition ==
            right.timedStartupPrefixDisposition &&
        left.videoPacketCapacity == right.videoPacketCapacity &&
        left.audioPacketCapacity == right.audioPacketCapacity &&
        left.videoByteCapacity == right.videoByteCapacity &&
        left.audioByteCapacity == right.audioByteCapacity &&
        left.maximumVideoPacketBytes == right.maximumVideoPacketBytes &&
        left.maximumAudioPacketBytes == right.maximumAudioPacketBytes &&
        left.maximumPreparedReadDuration == right.maximumPreparedReadDuration &&
        left.maximumPreparedHandoffDuration ==
            right.maximumPreparedHandoffDuration &&
        left.firstWindowMaximumSkew == right.firstWindowMaximumSkew;
}

} // namespace media::ffmpeg::graph
