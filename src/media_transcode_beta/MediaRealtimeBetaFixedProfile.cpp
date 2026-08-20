#include "media_transcode_beta/MediaRealtimeBetaFixedProfile.h"

#include <string>

namespace media::beta {
namespace {

const MediaRealtimeBetaFixedProfile::Product CurrentProfile{
    "realtime-beta-fixed-v1",
    1U,
    256U,
    128U,
    256U,
    4194304U,
    30000,
    2000,
    5000000,
    5000000,
    1328,
    10000,
    30000,
    100,
    true,
    ffmpeg::graph::MediaTranscodeStreamSet::VideoOnly,
    false,
#ifdef _WIN32
    ffmpeg::graph::MediaHardwareBackendRequest::Auto,
#else
    ffmpeg::graph::MediaHardwareBackendRequest::RKMPP,
#endif
    ffmpeg::graph::RealtimeInputStreamLayout::SeparateStreams,
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
           " metadata_queue=" + std::to_string(profile.metadataQueue) +
           " packet_queue=" + std::to_string(profile.packetQueue) +
           " frame_queue=" + std::to_string(profile.frameQueue) +
           " mux_queue=" + std::to_string(profile.muxQueue) +
           " startup_maximum_video_unit_bytes=" + std::to_string(profile.startupMaximumVideoUnitBytes) +
           " open_timeout_ms=" + std::to_string(profile.openTimeoutMs) +
           " read_timeout_ms=" + std::to_string(profile.readTimeoutMs) +
           " analyze_duration_us=" + std::to_string(profile.analyzeDurationUs) +
           " probe_size_bytes=" + std::to_string(profile.probeSizeBytes) +
           " mpeg_ts_rtp_packet_size_bytes=" + std::to_string(profile.mpegTsRtpPacketSizeBytes) +
           " progress_timeout_ms=" + std::to_string(profile.progressTimeoutMs) +
           " first_output_timeout_ms=" + std::to_string(profile.firstOutputTimeoutMs) +
           " poll_interval_ms=" + std::to_string(profile.pollIntervalMs) +
           " low_latency=" + (profile.lowLatency ? "true" : "false") +
           " stream_set=VideoOnly" +
           " disable_hardware=" + (profile.disableHardware ? "true" : "false") +
           " hardware_backend=" +
               ffmpeg::graph::mediaHardwareBackendRequestName(profile.hardwareBackend) +
           " input_layout=SeparateStreams output_layout=MuxedTransportStream output_transport=RtpAvp";
}

} // namespace media::beta
