#include "internal/graph/protocol/rtp/MediaRtpClockGroupPolicy.h"

namespace media::ffmpeg::graph {

const char* mediaRtpCommonEpochPolicyOptionValue(
    MediaRtpCommonEpochPolicy policy) noexcept
{
    switch (policy) {
    case MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime:
        return "earliest_locked_sender_report_source_time";
    case MediaRtpCommonEpochPolicy::Unknown:
    default:
        return nullptr;
    }
}

::media::Result<MediaRtpCommonEpochPolicy> parseMediaRtpCommonEpochPolicy(
    std::string_view value)
{
    if (value == "earliest_locked_sender_report_source_time") {
        return ::media::Result<MediaRtpCommonEpochPolicy>::success(
            MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime);
    }
    return ::media::Result<MediaRtpCommonEpochPolicy>::failure(
        ::media::ErrorInfo::invalidArgument(
            "RTP common epoch policy is missing or unsupported"));
}

} // namespace media::ffmpeg::graph
