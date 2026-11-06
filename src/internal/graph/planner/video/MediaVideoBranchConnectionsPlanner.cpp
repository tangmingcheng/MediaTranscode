#include "internal/graph/planner/video/MediaVideoBranchConnectionsPlanner.h"
#include "internal/graph/planner/MediaConnectionPlanWriter.h"
#include <new>

namespace media::ffmpeg::graph {
MediaVideoBranchShape MediaVideoBranchConnectionsPlanner::outputEncoder() noexcept
{
    using Role=MediaVideoBranchRole;
    MediaVideoBranchShape shape{};
    for(const auto role:{Role::CodecResolver,Role::OutputCodec,Role::Encode})
        shape.present[static_cast<std::size_t>(role)]=true;
    return shape;
}
std::optional<MediaNodeKind> MediaVideoBranchShape::ownedKind(MediaVideoBranchRole role) noexcept
{
    switch (role) {
    case MediaVideoBranchRole::CodecResolver: return MediaNodeKind::CodecResolver;
    case MediaVideoBranchRole::StartGate: return MediaNodeKind::PacketStartGate;
    case MediaVideoBranchRole::Decode: return MediaNodeKind::VideoDecode;
    case MediaVideoBranchRole::SourceCopy: return MediaNodeKind::VideoFilter;
    case MediaVideoBranchRole::Fanout: return MediaNodeKind::VideoOutputFanout;
    case MediaVideoBranchRole::Transfer: return MediaNodeKind::HardwareTransfer;
    case MediaVideoBranchRole::Timestamp: return MediaNodeKind::VideoTimestamp;
    case MediaVideoBranchRole::FrameRate: return MediaNodeKind::VideoFrameRate;
    case MediaVideoBranchRole::Filter: return MediaNodeKind::VideoFilter;
    case MediaVideoBranchRole::Preparation: return MediaNodeKind::VideoPreparation;
    case MediaVideoBranchRole::Encode: return MediaNodeKind::VideoEncode;
    default: return std::nullopt;
    }
}
MediaVideoBranchShape MediaVideoBranchConnectionsPlanner::sourceFrames(bool canonicalCadence, bool filterActive) noexcept
{
    using Role = MediaVideoBranchRole;
    MediaVideoBranchShape shape{};
    for (const auto role : {Role::Frame, Role::OutputCodec, Role::Transfer, Role::Preparation})
        shape.present[static_cast<std::size_t>(role)] = true;
    shape.present[static_cast<std::size_t>(Role::FrameRate)] = canonicalCadence;
    shape.present[static_cast<std::size_t>(Role::Filter)] = filterActive;
    return shape;
}
MediaVideoBranchShape MediaVideoBranchConnectionsPlanner::sourceDecoder() noexcept
{
    using Role = MediaVideoBranchRole;
    MediaVideoBranchShape shape{};
    for (const auto role : {Role::Format, Role::Packet, Role::CodecResolver, Role::Decode})
        shape.present[static_cast<std::size_t>(role)] = true;
    return shape;
}
namespace {
template<bool Updating> ::media::Status compileConnections(
    const MediaVideoBranchShape& shape, const MediaVideoBranchConnectionPolicies& policies,
    MediaConnectionPlanWriter<MediaVideoBranchConnection, Updating>& writer)
{
    using Role = MediaVideoBranchRole;
    const auto add = [&](Role from, std::string_view output, Role to, std::string_view input,
        MediaPayloadKind payload, const MediaEdgePolicy& policy, std::string_view label, MediaStreamKind stream) {
        if (!shape.has(from) || !shape.has(to)) {
            writer.reject(::media::ErrorInfo::invalidArgument("Branch connection references a missing role"));
            return;
        }
        writer.append({from, output, to, input, payload, stream, policy, label});
    };
    const auto codec = [&](Role from, std::string_view output, Role to, std::string_view input, std::string_view label) {
        add(from, output, to, input, MediaPayloadKind::CodecContext, policies.metadata, label, MediaStreamKind::Video);
    };
    if (shape.has(Role::CodecResolver) && (shape.has(Role::Transfer) || shape.has(Role::Decode)))
        add(Role::Format, "", Role::CodecResolver, "format", MediaPayloadKind::FormatContext,
            policies.metadata, ".format -> codec_resolver.format", MediaStreamKind::Metadata);
    if (shape.has(Role::Decode))
        codec(Role::CodecResolver, "decoder", Role::Decode, "codec", ".codec_resolver.decoder -> decode.codec");
    const Role target = shape.has(Role::Filter) ? Role::Filter : Role::Encode;
    if (shape.has(Role::Timestamp)) {
        codec(shape.has(Role::SharedCodec) ? Role::SharedCodec : Role::CodecResolver,
            shape.has(Role::SharedCodec) ? "" : "timestamp_source", Role::Timestamp, "source_codec", ".source_codec -> timestamp.source_codec");
        codec(Role::OutputCodec, "", Role::Timestamp, "target_codec", ".codec_resolver.encoder -> timestamp.target_codec");
        codec(Role::Timestamp, "target_codec", target, "codec", shape.has(Role::Filter)
            ? ".timestamp.target_codec -> filter.codec" : ".timestamp.target_codec -> encode.codec");
    } else if (shape.has(target)) {
        codec(Role::OutputCodec, "", target, "codec", shape.has(Role::Filter)
            ? ".codec_resolver.encoder -> filter.codec" : ".codec_resolver.encoder -> encode.codec");
    }
    if (shape.has(Role::Filter) && shape.has(Role::Encode))
        codec(Role::Filter, "codec", Role::Encode, "codec", ".filter.codec -> encode.codec");
    if (shape.has(Role::Decode)) {
        if (shape.has(Role::StartGate)) {
            add(Role::Packet, "", Role::StartGate, "packet", MediaPayloadKind::Packet,
                policies.packet, ".packet -> packet_start_gate.packet", MediaStreamKind::Video);
            add(Role::StartGate, "packet", Role::Decode, "packet", MediaPayloadKind::Packet,
                policies.packet, ".packet_start_gate.packet -> decode.packet", MediaStreamKind::Video);
        } else add(Role::Packet, "", Role::Decode, "packet", MediaPayloadKind::Packet,
            policies.packet, ".packet -> decode.packet", MediaStreamKind::Video);
    }
    if (shape.has(Role::Transfer)) {
        Role frame = Role::Frame;
        const auto frameEdge = [&](Role targetRole, const MediaEdgePolicy& policy, std::string_view label) {
            add(frame, frame == Role::Frame ? "" : "frame", targetRole, "frame", MediaPayloadKind::Frame, policy, label, MediaStreamKind::Video);
            frame = targetRole;
        };
        if (shape.has(Role::SourceCopy)) frameEdge(Role::SourceCopy, policies.frame, ".decode.frame -> source_copy.frame");
        if (shape.has(Role::Fanout)) frameEdge(Role::Fanout, policies.frame, ".decode.frame -> output_fanout.frame");
        frameEdge(Role::Transfer, policies.inputFrame, ".decode.frame -> hwtransfer.frame");
        if (shape.has(Role::Timestamp)) frameEdge(Role::Timestamp, policies.frame, ".transfer.frame -> timestamp.frame");
        if (shape.has(Role::FrameRate)) frameEdge(Role::FrameRate, policies.frame, ".source.frame -> framerate.frame");
        if (shape.has(Role::Filter)) frameEdge(Role::Filter, policies.frame, ".processed.frame -> filter.frame");
        if (shape.has(Role::Preparation)) frameEdge(Role::Preparation, policies.frame, ".processed.frame -> preparation.frame");
        if (shape.has(Role::Encode)) frameEdge(Role::Encode, policies.encoderFrame, ".processed.frame -> encode.frame");
    }
    return writer.finish();
}
} // namespace
::media::Result<MediaConnectionCatalog<MediaVideoBranchConnection>> MediaVideoBranchConnectionsPlanner::plan(
    const MediaVideoBranchShape& shape, const MediaVideoBranchConnectionPolicies& policies, std::pmr::memory_resource& resource)
try {
    using Result = ::media::Result<MediaConnectionCatalog<MediaVideoBranchConnection>>;
    MediaConnectionCatalog<MediaVideoBranchConnection> result(resource);
    MediaConnectionPlanWriter<MediaVideoBranchConnection, false> writer(result);
    if (auto status = compileConnections(shape, policies, writer); !status) return Result::failure(status.error());
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaConnectionCatalog<MediaVideoBranchConnection>>::failure(
        ::media::ErrorInfo::allocationFailed("video branch connection plan"));
}
::media::Status MediaVideoBranchConnectionsPlanner::rebind(
    const MediaVideoBranchShape& shape, const MediaVideoBranchConnectionPolicies& policies, std::span<MediaVideoBranchConnection> connections)
{
    MediaConnectionPlanWriter<MediaVideoBranchConnection, true> writer(connections);
    return compileConnections(shape, policies, writer);
}
} // namespace media::ffmpeg::graph
