#include "internal/graph/planner/realtime/MediaCompositionAggregateConnectionsPlanner.h"
#include <algorithm>
#include <charconv>
#include <limits>
#include <new>

namespace media::ffmpeg::graph {
namespace {
template<bool Updating> ::media::Status compileConnections(
    std::span<const MediaAvAggregateSourcePlan> sources, std::size_t audioSource,
    std::string_view audioPort, std::span<const MediaRealtimeEdgePolicySet> sourcePolicies,
    const MediaRealtimeEdgePolicySet& outputPolicies, MediaCompositionAggregateConnections& result)
{
    using Role = MediaCompositionAggregateRole;
    const auto invalid = [](const char* message) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
    };
    if (sources.empty() || sourcePolicies.size() != sources.size() ||
        audioSource >= sources.size() || audioPort.empty() || !sources[audioSource].audioIdentity)
        return invalid("Aggregate connections require source policies and a selected audio source");
    if (audioPort == "audio_codec" || audioPort == "activated")
        return invalid("Aggregate selected audio port conflicts with an audio aggregate input");
    if constexpr (Updating) {
        if (result.sources.size() != sources.size()) return invalid("Aggregate source catalog changed");
    } else result.sources.resize(sources.size());
    bool identityValid = true;
    const auto add = [&](std::vector<MediaCompositionAggregateConnection>& edges, std::size_t& cursor,
        Role from, std::string_view output, Role to, std::string_view input,
        MediaStreamKind stream, MediaPayloadKind payload, const MediaEdgePolicy& policy) {
        if (!identityValid) return;
        if constexpr (Updating) {
            if (cursor >= edges.size()) { identityValid = false; return; }
            auto& old = edges[cursor];
            if (old.from != from || old.to != to || old.output != output || old.input != input ||
                old.stream != stream || old.payload != payload) { identityValid = false; return; }
            old.policy = policy;
        } else edges.push_back({from, std::string(output), to, std::string(input), stream, payload, policy});
        ++cursor;
    };
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const auto& source = sources[i];
        const auto& policies = sourcePolicies[i];
        const bool audio = source.audioIdentity.has_value();
        if (source.videoPort.empty() || source.discardedAudioPort.has_value() != (audio && i != audioSource) ||
            (source.discardedAudioPort && source.discardedAudioPort->empty()))
            return invalid("Aggregate connections require distinct source ports and exact audio membership");
        constexpr std::string_view prefix = "source_epoch_";
        std::array<char, prefix.size() + std::numeric_limits<std::size_t>::digits10 + 1> epoch{};
        std::copy(prefix.begin(), prefix.end(), epoch.begin());
        const auto converted = std::to_chars(epoch.data() + prefix.size(), epoch.data() + epoch.size(), i);
        if (converted.ec != std::errc{}) return invalid("Aggregate source index cannot form its epoch port");
        auto& edges = result.sources[i];
        std::size_t cursor = 0;
        add(edges, cursor, Role::SourceVideoFrame, "", Role::VideoAggregate, source.videoPort,
            MediaStreamKind::Video, MediaPayloadKind::Frame, policies.preparedVideoFrame);
        add(edges, cursor, Role::SourceActivated, "", Role::VideoAggregate,
            std::string_view(epoch.data(), static_cast<std::size_t>(converted.ptr - epoch.data())),
            MediaStreamKind::Metadata, MediaPayloadKind::GraphEvent, policies.atomicMetadata);
        if (audio) add(edges, cursor, Role::SourceAudioFrame, "",
            i == audioSource ? Role::AudioAggregate : Role::VideoAggregate,
            i == audioSource ? audioPort : std::string_view(*source.discardedAudioPort),
            MediaStreamKind::Audio, MediaPayloadKind::Frame, policies.audioFrame);
        if (!identityValid || cursor != edges.size()) return invalid("Aggregate source connection identity changed");
    }
    auto& edges = result.output;
    std::size_t cursor = 0;
    add(edges, cursor, Role::OutputVideoCodec, "", Role::VideoAggregate, "video_codec",
        MediaStreamKind::Video, MediaPayloadKind::CodecContext, outputPolicies.metadata);
    add(edges, cursor, Role::OutputAudioCodec, "", Role::AudioAggregate, "audio_codec",
        MediaStreamKind::Audio, MediaPayloadKind::CodecContext, outputPolicies.metadata);
    add(edges, cursor, Role::VideoAggregate, "video", Role::OutputVideoFrame, "",
        MediaStreamKind::Video, MediaPayloadKind::Frame, outputPolicies.preparedVideoFrame);
    add(edges, cursor, Role::AudioAggregate, "audio", Role::OutputAudioFrame, "",
        MediaStreamKind::Audio, MediaPayloadKind::Frame, outputPolicies.audioFrame);
    add(edges, cursor, Role::VideoAggregate, "activated", Role::AudioAggregate, "activated",
        MediaStreamKind::Metadata, MediaPayloadKind::GraphEvent, outputPolicies.atomicMetadata);
    if (!identityValid || cursor != edges.size()) return invalid("Aggregate output connection identity changed");
    // The fixed connection catalog is also the port catalog. Compare only
    // aggregate input endpoints: names may repeat across different nodes.
    const auto visit = [&](const auto& callback) {
        for (const auto& source : result.sources) for (const auto& edge : source) callback(edge);
        for (const auto& edge : result.output) callback(edge);
    };
    bool duplicate = false;
    visit([&](const auto& edge) {
        if (edge.to != Role::VideoAggregate && edge.to != Role::AudioAggregate) return;
        visit([&](const auto& other) {
            if (&edge != &other && edge.to == other.to && edge.input == other.input) duplicate = true;
        });
    });
    if (duplicate) return invalid("Aggregate connections require distinct source ports and exact audio membership");
    return ::media::Status::success();
}
} // namespace
::media::Result<MediaCompositionAggregateConnections> MediaCompositionAggregateConnectionsPlanner::plan(
    std::span<const MediaAvAggregateSourcePlan> sources, std::size_t audioSource,
    std::string_view audioPort, std::span<const MediaRealtimeEdgePolicySet> sourcePolicies,
    const MediaRealtimeEdgePolicySet& outputPolicies)
try {
    using Result = ::media::Result<MediaCompositionAggregateConnections>;
    MediaCompositionAggregateConnections result;
    if (auto status = compileConnections<false>(sources, audioSource, audioPort, sourcePolicies, outputPolicies, result); !status)
        return Result::failure(status.error());
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionAggregateConnections>::failure(
        ::media::ErrorInfo::allocationFailed("composition aggregate connection plan"));
}
::media::Status MediaCompositionAggregateConnectionsPlanner::rebind(
    std::span<const MediaAvAggregateSourcePlan> sources, std::size_t audioSource,
    std::string_view audioPort, std::span<const MediaRealtimeEdgePolicySet> sourcePolicies,
    const MediaRealtimeEdgePolicySet& outputPolicies, MediaCompositionAggregateConnections& connections)
{
    return compileConnections<true>(sources, audioSource, audioPort, sourcePolicies, outputPolicies, connections);
}
} // namespace media::ffmpeg::graph
