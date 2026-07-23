#pragma once

namespace media::ffmpeg::graph {

struct MediaRtpClockLivenessPolicy final {
    static constexpr int MaximumExpectedSenderReportIntervalMs = 5'500;
    static constexpr int SenderReportDeliveryJitterBudgetMs = 1'500;
    static constexpr int SenderReportTimeoutMs =
        MaximumExpectedSenderReportIntervalMs +
        SenderReportDeliveryJitterBudgetMs;
    static constexpr int ExtrapolationBudgetMs = 2'000;
    static constexpr int MaximumExtrapolationMs =
        SenderReportTimeoutMs + ExtrapolationBudgetMs;
    static constexpr int CnameTimeoutMs = MaximumExtrapolationMs;
};

static_assert(MediaRtpClockLivenessPolicy::SenderReportTimeoutMs == 7'000);
static_assert(MediaRtpClockLivenessPolicy::MaximumExtrapolationMs == 9'000);

} // namespace media::ffmpeg::graph
