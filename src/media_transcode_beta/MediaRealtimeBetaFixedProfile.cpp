#include "media_transcode_beta/MediaRealtimeBetaFixedProfile.h"

#include <string>

namespace media::beta {
namespace {

const MediaRealtimeBetaFixedProfile::Product CurrentProfile{
    "realtime-beta-fixed-v1",
    30000,
    2000,
    5000000,
    5000000,
    10000,
    30000,
    100,
    ffmpeg::graph::MediaTranscodeStreamSet::VideoOnly,
    ffmpeg::graph::RealtimeOutputStreamLayout::MuxedTransportStream,
    ffmpeg::graph::MediaOutputTransportKind::RtpAvp
};

} // namespace

const MediaRealtimeBetaFixedProfile::Product& MediaRealtimeBetaFixedProfile::current() noexcept
{
    return CurrentProfile;
}

std::string MediaRealtimeBetaFixedProfile::diagnosticSummary()
{
    const auto& profile = current();
    return std::string("profile=") + profile.identity +
           " open_timeout_ms=" + std::to_string(profile.openTimeoutMs) +
           " read_timeout_ms=" + std::to_string(profile.readTimeoutMs) +
           " analyze_duration_us=" + std::to_string(profile.analyzeDurationUs) +
           " probe_size_bytes=" + std::to_string(profile.probeSizeBytes) +
           " progress_timeout_ms=" + std::to_string(profile.progressTimeoutMs) +
           " first_output_timeout_ms=" + std::to_string(profile.firstOutputTimeoutMs) +
           " poll_interval_ms=" + std::to_string(profile.pollIntervalMs) +
           " low_latency=planner-realtime-product" +
           " stream_set=VideoOnly" +
           " hardware_backend=planner-highest-score" +
           " input_layout=planner-derived-from-input-type output_layout=MuxedTransportStream output_transport=RtpAvp";
}

} // namespace media::beta
