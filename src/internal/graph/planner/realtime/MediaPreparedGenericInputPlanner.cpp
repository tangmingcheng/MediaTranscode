#include "internal/graph/planner/realtime/MediaPreparedGenericInputPlanner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedGenericInputPlan>
MediaPreparedGenericInputPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaAvSyncStartupPolicy& startup,
    int videoStreamIndex,
    MediaRational videoTimeBase,
    int audioStreamIndex,
    MediaRational audioTimeBase)
{
    if (request.parameters.execution.streamSet !=
            MediaTranscodeStreamSet::AudioVideo ||
        !request.input.readTimeoutMs || *request.input.readTimeoutMs <= 0 ||
        !request.input.openTimeoutMs || *request.input.openTimeoutMs <= 0 ||
        startup.requireVideoKeyFrame != true ||
        !startup.videoCapacity || !startup.audioCapacity ||
        !startup.videoByteCapacity || !startup.audioByteCapacity ||
        !startup.maximumVideoUnitBytes || !startup.maximumAudioUnitBytes ||
        !startup.maximumInitialSkewNs) {
        return ::media::Result<MediaPreparedGenericInputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "prepared generic A/V input requires the shared startup policy"));
    }
    MediaPreparedGenericInputPlan product{
        videoStreamIndex,
        audioStreamIndex,
        videoTimeBase,
        audioTimeBase,
        MediaPreparedLeadingVideoDisposition::
            DiscardUntimedNonKeyBeforeFirstTimedVideo,
        MediaPreparedTimedStartupPrefixDisposition::
            DiscardEarlierCompleteTimedUntilCommonWindow,
        *startup.videoCapacity,
        *startup.audioCapacity,
        *startup.videoByteCapacity,
        *startup.audioByteCapacity,
        *startup.maximumVideoUnitBytes,
        *startup.maximumAudioUnitBytes,
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(*request.input.readTimeoutMs) *
            1'000'000),
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(*request.input.openTimeoutMs) *
            1'000'000),
        *startup.maximumInitialSkewNs};
    if (auto valid = product.validate(); !valid) {
        return ::media::Result<MediaPreparedGenericInputPlan>::failure(
            valid.error());
    }
    return ::media::Result<MediaPreparedGenericInputPlan>::success(
        std::move(product));
}

} // namespace media::ffmpeg::graph
