#include "internal/graph/planner/audio/MediaAudioBranchConnectionsPlanner.h"
#include "internal/graph/planner/MediaConnectionPlanWriter.h"
#include <new>

namespace media::ffmpeg::graph {
MediaAudioBranchShape MediaAudioBranchConnectionsPlanner::outputEncoder(bool synchronized) noexcept
{
    using Role=MediaAudioBranchRole;
    MediaAudioBranchShape shape{};
    for(const auto role:{Role::CodecResolver,Role::OutputCodec,Role::Encode})
        shape.present[static_cast<std::size_t>(role)]=true;
    shape.present[static_cast<std::size_t>(Role::Canonicalizer)]=synchronized;
    return shape;
}
std::optional<MediaNodeKind> MediaAudioBranchShape::ownedKind(MediaAudioBranchRole role) noexcept
{
    switch (role) {
    case MediaAudioBranchRole::Normalize: return MediaNodeKind::PacketNormalize;
    case MediaAudioBranchRole::CodecResolver: return MediaNodeKind::AudioCodecResolver;
    case MediaAudioBranchRole::Decode: return MediaNodeKind::AudioDecode;
    case MediaAudioBranchRole::Trim: return MediaNodeKind::AudioStartupTrim;
    case MediaAudioBranchRole::Drift: return MediaNodeKind::AudioDriftController;
    case MediaAudioBranchRole::Resample: return MediaNodeKind::AudioResample;
    case MediaAudioBranchRole::Encode: return MediaNodeKind::AudioEncode;
    case MediaAudioBranchRole::Canonicalizer: return MediaNodeKind::EncodedAudioCanonicalizer;
    default: return std::nullopt;
    }
}
MediaAudioBranchShape MediaAudioBranchConnectionsPlanner::source(bool normalizePackets, bool synchronized, bool externalCorrection) noexcept
{
    using Role = MediaAudioBranchRole;
    MediaAudioBranchShape shape{{}, externalCorrection};
    for (const auto role : {Role::Format, Role::Packet, Role::OutputCodec, Role::CodecResolver, Role::Decode, Role::Resample})
        shape.present[static_cast<std::size_t>(role)] = true;
    shape.present[static_cast<std::size_t>(Role::Normalize)] = normalizePackets;
    shape.present[static_cast<std::size_t>(Role::Trim)] = synchronized;
    shape.present[static_cast<std::size_t>(Role::Drift)] = synchronized;
    return shape;
}
namespace {
template<bool Updating> ::media::Status compileConnections(
    const MediaAudioBranchShape& shape, const MediaRealtimeEdgePolicySet& policies,
    MediaConnectionPlanWriter<MediaAudioBranchConnection, Updating>& writer)
{
    using Role = MediaAudioBranchRole;
    const auto add = [&](Role from, std::string_view output, Role to, std::string_view input,
        MediaPayloadKind payload, const MediaEdgePolicy& policy, std::string_view label, MediaStreamKind stream) {
        if (!shape.has(from) || !shape.has(to)) {
            writer.reject(::media::ErrorInfo::invalidArgument("Branch connection references a missing role"));
            return;
        }
        writer.append({from, output, to, input, payload, stream, policy, label});
    };
    const auto& packet = shape.has(Role::Trim) ? policies.startupAudioRelease : policies.audioPacket;
    if (shape.has(Role::Decode)) {
        if (shape.has(Role::Normalize)) {
            add(Role::Format, "", Role::Normalize, "format", MediaPayloadKind::FormatContext,
                policies.metadata, ".format -> packet_normalize.format", MediaStreamKind::Metadata);
            add(Role::Packet, "", Role::Normalize, "packet", MediaPayloadKind::Packet,
                packet, ".packet -> packet_normalize.packet", MediaStreamKind::Audio);
        }
        add(Role::Format, "", Role::CodecResolver, "format", MediaPayloadKind::FormatContext,
            policies.metadata, ".format -> codec_resolver.format", MediaStreamKind::Metadata);
        add(Role::CodecResolver, "decoder", Role::Decode, "codec", MediaPayloadKind::CodecContext,
            policies.metadata, ".codec_resolver.decoder -> decode.codec", MediaStreamKind::Audio);
        if (shape.has(Role::Normalize))
            add(Role::Normalize, "packet", Role::Decode, "packet", MediaPayloadKind::Packet,
                policies.audioPacket, ".packet_normalize.packet -> decode.packet", MediaStreamKind::Audio);
        else add(Role::Packet, "", Role::Decode, "packet", MediaPayloadKind::Packet,
            packet, ".packet -> decode.packet", MediaStreamKind::Audio);
        add(Role::OutputCodec, "", Role::Resample, "codec", MediaPayloadKind::CodecContext,
            policies.metadata, ".codec_resolver.encoder -> resample.codec", MediaStreamKind::Audio);
        if (shape.has(Role::Trim)) {
            add(Role::Decode, "frame", Role::Trim, "frame", MediaPayloadKind::Frame,
                policies.audioFrame, ".decode.frame -> startup_trim.frame", MediaStreamKind::Audio);
            add(Role::Trim, "frame", Role::Drift, "audio", MediaPayloadKind::Frame,
                policies.audioFrame, ".startup_trim.frame -> drift_controller.audio", MediaStreamKind::Audio);
            add(Role::Drift, "audio", Role::Resample, "frame", MediaPayloadKind::Frame,
                policies.audioDriftTransaction, ".drift_controller.audio -> resample.frame", MediaStreamKind::Audio);
        } else add(Role::Decode, "frame", Role::Resample, "frame", MediaPayloadKind::Frame,
            policies.audioFrame, ".decode.frame -> resample.frame", MediaStreamKind::Audio);
        if (shape.externalCorrection)
            add(Role::Drift, "correction", Role::Resample, "correction", MediaPayloadKind::GraphEvent,
                policies.audioDriftTransaction, ".drift_controller.correction -> resample.correction", MediaStreamKind::Audio);
    }
    if (shape.has(Role::Encode)) {
        add(Role::OutputCodec, "", Role::Encode, "codec", MediaPayloadKind::CodecContext,
            policies.metadata, ".codec_resolver.encoder -> encode.codec", MediaStreamKind::Audio);
        if (shape.has(Role::Resample)) add(Role::Resample, "frame", Role::Encode, "frame", MediaPayloadKind::Frame,
            policies.audioFrame, ".resample.frame -> encode.frame", MediaStreamKind::Audio);
        if (shape.has(Role::Canonicalizer))
            add(Role::Encode, "packet", Role::Canonicalizer, "encoded", MediaPayloadKind::Packet,
                policies.audioPacket, ".encode.packet -> canonicalizer.encoded", MediaStreamKind::Audio);
    }
    return writer.finish();
}
} // namespace
::media::Result<std::vector<MediaAudioBranchConnection>> MediaAudioBranchConnectionsPlanner::plan(
    const MediaAudioBranchShape& shape, const MediaRealtimeEdgePolicySet& policies)
try {
    using Result = ::media::Result<std::vector<MediaAudioBranchConnection>>;
    std::vector<MediaAudioBranchConnection> result;
    MediaConnectionPlanWriter<MediaAudioBranchConnection, false> writer(result);
    if (auto status = compileConnections(shape, policies, writer); !status) return Result::failure(status.error());
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::vector<MediaAudioBranchConnection>>::failure(
        ::media::ErrorInfo::allocationFailed("audio branch connection plan"));
}
::media::Status MediaAudioBranchConnectionsPlanner::rebind(
    const MediaAudioBranchShape& shape, const MediaRealtimeEdgePolicySet& policies, std::span<MediaAudioBranchConnection> connections)
{
    MediaConnectionPlanWriter<MediaAudioBranchConnection, true> writer(connections);
    return compileConnections(shape, policies, writer);
}
} // namespace media::ffmpeg::graph
