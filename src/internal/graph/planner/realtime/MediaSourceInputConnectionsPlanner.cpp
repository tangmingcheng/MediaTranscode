#include "internal/graph/planner/realtime/MediaSourceInputConnectionsPlanner.h"
#include <new>

namespace media::ffmpeg::graph {
std::optional<MediaStartupRouteRole> MediaSourceInputNodeShape::startupRole(MediaSourceInputRole role) noexcept
{
    switch (role) {
    case MediaSourceInputRole::VideoStartup: return MediaStartupRouteRole::VideoInput;
    case MediaSourceInputRole::AudioStartup: return MediaStartupRouteRole::AudioInput;
    case MediaSourceInputRole::Coordinator: return MediaStartupRouteRole::Coordinator;
    default: return std::nullopt;
    }
}
bool MediaSourceInputNodeShape::has(MediaSourceInputRole role) const noexcept
{
    using R = MediaSourceInputRole;
    const bool audio = startup.members == MediaTranscodeStreamSet::AudioVideo;
    switch (role) {
    case R::RawVideo: return clock == MediaAvSyncSourceClockMode::RtpSenderReports;
    case R::RawAudio: return audio && clock == MediaAvSyncSourceClockMode::RtpSenderReports;
    case R::ProtocolAudio: case R::AudioGate: case R::AudioStartup: return audio;
    case R::DecoderFrame: return startup.readiness == MediaVideoStartupReadiness::DecodedPresentationSpan;
    default: return true;
    }
}
std::optional<MediaNodeKind> MediaSourceInputNodeShape::ownedKind(MediaSourceInputRole role) const noexcept
{
    if (const auto startupNode = startupRole(role)) return startup.nodeKind(*startupNode);
    switch (role) {
    case MediaSourceInputRole::SourceClock: return MediaNodeKind::SourceClockStateFanout;
    case MediaSourceInputRole::VideoGate: case MediaSourceInputRole::AudioGate: return MediaNodeKind::LockedPacketGate;
    case MediaSourceInputRole::StartupClock: return MediaNodeKind::AvStartupClock;
    default: return std::nullopt;
    }
}
::media::Result<std::vector<MediaSourceInputConnection>> MediaSourceInputConnectionsPlanner::plan(
    MediaTranscodeStreamSet members, MediaVideoStartupReadiness readiness,
    MediaAvSyncSourceClockMode clock, const MediaRealtimeEdgePolicySet& policies)
try {
    using R = ::media::Result<std::vector<MediaSourceInputConnection>>;
    using Role = MediaSourceInputRole;
    using Phase = MediaSourceInputConnectionPhase;
    const bool audio = members == MediaTranscodeStreamSet::AudioVideo;
    const bool decoded = readiness == MediaVideoStartupReadiness::DecodedPresentationSpan;
    if ((!audio && members != MediaTranscodeStreamSet::VideoOnly) ||
        (!decoded && readiness != MediaVideoStartupReadiness::IntervalCoverage) ||
        (clock != MediaAvSyncSourceClockMode::RtpSenderReports &&
         clock != MediaAvSyncSourceClockMode::MpegTsPcr &&
         clock != MediaAvSyncSourceClockMode::DemuxTimestamps))
        return R::failure(::media::ErrorInfo::invalidArgument("Source input connections require member, readiness and clock facts"));
    std::vector<MediaSourceInputConnection> result;
    const auto add = [&](Role from, std::string_view output, Role to, std::string_view input,
        MediaPayloadKind payload, MediaStreamKind stream, const MediaEdgePolicy& policy,
        std::string_view label, Phase phase = Phase::Shared) {
        result.push_back({from, output, to, input, payload, stream, policy, label, phase});
    };
    if (clock == MediaAvSyncSourceClockMode::RtpSenderReports) {
        add(Role::RawVideo, "", Role::ProtocolVideo, "packet", MediaPayloadKind::Packet,
            MediaStreamKind::Video, policies.synchronizedPacket, "RTP video packet -> clock binder", Phase::ProtocolPackets);
        if (audio) add(Role::RawAudio, "", Role::ProtocolAudio, "packet", MediaPayloadKind::Packet,
            MediaStreamKind::Audio, policies.synchronizedPacket, "RTP audio packet -> clock binder", Phase::ProtocolPackets);
    }
    const bool demux = clock == MediaAvSyncSourceClockMode::DemuxTimestamps;
    add(Role::ProtocolClock, "", Role::SourceClock, "clock", MediaPayloadKind::GraphEvent,
        MediaStreamKind::Metadata, demux ? policies.atomicMetadata : policies.metadata,
        "protocol clock -> source clock fanout");
    add(Role::SourceClock, "video", Role::VideoGate, "clock", MediaPayloadKind::GraphEvent,
        MediaStreamKind::Metadata, policies.metadata, "source clock -> video generation gate");
    if (audio) add(Role::SourceClock, "audio", Role::AudioGate, "clock", MediaPayloadKind::GraphEvent,
        MediaStreamKind::Metadata, policies.metadata, "source clock -> audio generation gate");
    add(Role::ProtocolVideo, "", Role::VideoGate, "packet", MediaPayloadKind::Packet,
        MediaStreamKind::Video, demux ? policies.atomicVideoPacket : policies.synchronizedPacket,
        "protocol video -> generation gate");
    if (audio) add(Role::ProtocolAudio, "", Role::AudioGate, "packet", MediaPayloadKind::Packet,
        MediaStreamKind::Audio, demux ? policies.atomicAudioPacket : policies.synchronizedPacket,
        "protocol audio -> generation gate");
    if (!decoded) add(Role::VideoGate, "packet", Role::VideoStartup, "in", MediaPayloadKind::Packet,
        MediaStreamKind::Video, policies.synchronizedPacket, "generation-bound video -> canonical input");
    if (audio) add(Role::AudioGate, "packet", Role::AudioStartup, "in", MediaPayloadKind::Packet,
        MediaStreamKind::Audio, policies.synchronizedPacket, "generation-bound audio -> canonical input");
    add(Role::SourceClock, "startup", Role::StartupClock, "clock", MediaPayloadKind::GraphEvent,
        MediaStreamKind::Metadata, policies.metadata, "source clock -> startup clock");
    add(Role::StartupClock, "tick", Role::Coordinator, "clock", MediaPayloadKind::GraphEvent,
        MediaStreamKind::Metadata, policies.metadata, "master clock tick -> startup coordinator");
    if (decoded) add(Role::DecoderFrame, "", Role::VideoStartup, "frame", MediaPayloadKind::Frame,
        MediaStreamKind::Video, policies.synchronizedVideoFrame, "decoded display -> startup input", Phase::DecodedFrame);
    return R::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::vector<MediaSourceInputConnection>>::failure(
        ::media::ErrorInfo::allocationFailed("source input connection plan"));
}
} // namespace media::ffmpeg::graph
