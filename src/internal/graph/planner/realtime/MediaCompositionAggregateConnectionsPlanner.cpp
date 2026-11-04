#include "internal/graph/planner/realtime/MediaCompositionAggregateConnectionsPlanner.h"
#include <new>
#include <unordered_set>

namespace media::ffmpeg::graph {
::media::Result<MediaCompositionAggregateConnections> MediaCompositionAggregateConnectionsPlanner::plan(
    std::span<const MediaAvAggregateSourcePlan> sources, std::size_t audioSource,
    std::string_view audioPort, std::span<const MediaRealtimeEdgePolicySet> sourcePolicies,
    const MediaRealtimeEdgePolicySet& outputPolicies)
try {
    using Result = ::media::Result<MediaCompositionAggregateConnections>;
    using Role = MediaCompositionAggregateRole;
    if (sources.empty() || sourcePolicies.size() != sources.size() ||
        audioSource >= sources.size() || audioPort.empty() || !sources[audioSource].audioIdentity)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Aggregate connections require source policies and a selected audio source"));
    MediaCompositionAggregateConnections result;
    // Port names are unique within a node and direction, not across nodes.
    std::unordered_set<std::string> videoInputs{"video_codec"};
    std::unordered_set<std::string> audioInputs{"audio_codec", "activated"};
    if (!audioInputs.insert(std::string(audioPort)).second)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Aggregate selected audio port conflicts with an audio aggregate input"));
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const auto& source = sources[i];
        const auto& policies = sourcePolicies[i];
        const auto epoch = "source_epoch_" + std::to_string(i);
        const bool audio = source.audioIdentity.has_value();
        if (source.videoPort.empty() || !videoInputs.insert(source.videoPort).second ||
            !videoInputs.insert(epoch).second || source.discardedAudioPort.has_value() != (audio && i != audioSource) ||
            (source.discardedAudioPort && (source.discardedAudioPort->empty() ||
                !videoInputs.insert(*source.discardedAudioPort).second)))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Aggregate connections require distinct source ports and exact audio membership"));
        std::vector<MediaCompositionAggregateConnection> connections{
            {Role::SourceVideoFrame, "", Role::VideoAggregate, source.videoPort,
                MediaStreamKind::Video, MediaPayloadKind::Frame, policies.preparedVideoFrame},
            {Role::SourceActivated, "", Role::VideoAggregate, epoch,
                MediaStreamKind::Metadata, MediaPayloadKind::GraphEvent, policies.atomicMetadata}};
        if (audio) connections.push_back({Role::SourceAudioFrame, "",
            i == audioSource ? Role::AudioAggregate : Role::VideoAggregate,
            i == audioSource ? std::string(audioPort) : *source.discardedAudioPort,
            MediaStreamKind::Audio, MediaPayloadKind::Frame, policies.audioFrame});
        result.sources.push_back(std::move(connections));
    }
    result.output = {
        {Role::OutputVideoCodec, "", Role::VideoAggregate, "video_codec",
            MediaStreamKind::Video, MediaPayloadKind::CodecContext, outputPolicies.metadata},
        {Role::OutputAudioCodec, "", Role::AudioAggregate, "audio_codec",
            MediaStreamKind::Audio, MediaPayloadKind::CodecContext, outputPolicies.metadata},
        {Role::VideoAggregate, "video", Role::OutputVideoFrame, "",
            MediaStreamKind::Video, MediaPayloadKind::Frame, outputPolicies.preparedVideoFrame},
        {Role::AudioAggregate, "audio", Role::OutputAudioFrame, "",
            MediaStreamKind::Audio, MediaPayloadKind::Frame, outputPolicies.audioFrame},
        {Role::VideoAggregate, "activated", Role::AudioAggregate, "activated",
            MediaStreamKind::Metadata, MediaPayloadKind::GraphEvent, outputPolicies.atomicMetadata}};
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaCompositionAggregateConnections>::failure(
        ::media::ErrorInfo::allocationFailed("composition aggregate connection plan"));
}
} // namespace media::ffmpeg::graph
