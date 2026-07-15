#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

namespace media::ffmpeg::graph {

MediaCanonicalInputNode::MediaCanonicalInputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaCanonicalInputNode") {}

MediaNodeKind MediaCanonicalInputNode::staticKind() noexcept
{
    return MediaNodeKind::CanonicalInput;
}

::media::Status MediaCanonicalInputNode::stop(MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaCanonicalInputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaCanonicalInputNode::resetState() noexcept
{
    m_mapper.reset();
    m_stream.reset();
    m_decodeOrder.reset();
    m_duration.reset();
    m_sourceIdentity.clear();
    m_generation = 0;
    m_nextSequence = 1;
    m_audioSampleRate = 0;
    m_audioSampleCount = 0;
}

::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>
MediaCanonicalInputNode::canonicalize(
    MediaBufferRef encodedAccessUnit,
    const MediaCanonicalSourceTimestamp& protocolTimestamp,
    const MediaCanonicalTimeMapper& mapper,
    MediaScheduledStream stream,
    MediaDecodeOrderMode decodeOrder,
    MediaSourceAccessUnitSequence sourceSequence)
{
    const auto expected = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video : MediaStreamKind::Audio;
    if (!encodedAccessUnit || encodedAccessUnit->streamKind() != expected ||
        !protocolTimestamp.presentationTime()) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires validated protocol presentation time"));
    }
    auto mapped = mapper.map(protocolTimestamp);
    if (!mapped) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            mapped.error().toErrorInfo());
    }
    auto lineage = createMediaCanonicalLineage(
        mapped.value(), decodeOrder, sourceSequence);
    if (!lineage) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            lineage.error());
    }
    auto created = MediaCanonicalAccessUnitBuffer::create(
        std::move(encodedAccessUnit), std::move(lineage).value());
    if (!created) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            created.error());
    }
    return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::success(
        std::dynamic_pointer_cast<MediaCanonicalAccessUnitBuffer>(
            std::move(created).value()));
}

::media::Status MediaCanonicalInputNode::configure(
    MediaGraphExecutionContext& context)
{
    if (m_mapper) return ::media::Status::success();
    const auto* options = nodeOptions(context);
    auto stream = requiredNodeOption(options, "MediaCanonicalInputNode",
                                     "canonical_input.stream");
    auto topology = requiredNodeOption(options, "MediaCanonicalInputNode",
                                       "canonical_input.topology");
    auto identity = requiredNodeOption(options, "MediaCanonicalInputNode",
                                       "canonical_input.source_identity");
    auto sourceEpoch = requiredPositiveInt64NodeOption(options, "MediaCanonicalInputNode",
                                                       "canonical_input.source_epoch_ns");
    auto runningEpoch = requiredPositiveInt64NodeOption(options, "MediaCanonicalInputNode",
                                                        "canonical_input.running_epoch_ns");
    auto generation = requiredPositiveInt64NodeOption(options, "MediaCanonicalInputNode",
                                                      "canonical_input.generation");
    auto duration = requiredPositiveInt64NodeOption(options, "MediaCanonicalInputNode",
                                                    "canonical_input.duration_ns");
    auto order = requiredNodeOption(options, "MediaCanonicalInputNode",
                                    "canonical_input.decode_order");
    if (!stream || !topology || !identity || !sourceEpoch || !runningEpoch ||
        !generation || !duration || !order) {
        if (!stream) return ::media::Status::failure(stream.error());
        if (!topology) return ::media::Status::failure(topology.error());
        if (!identity) return ::media::Status::failure(identity.error());
        if (!sourceEpoch) return ::media::Status::failure(sourceEpoch.error());
        if (!runningEpoch) return ::media::Status::failure(runningEpoch.error());
        if (!generation) return ::media::Status::failure(generation.error());
        if (!duration) return ::media::Status::failure(duration.error());
        return ::media::Status::failure(order.error());
    }
    if (stream.value() == "video") m_stream.emplace(MediaScheduledStream::Video);
    else if (stream.value() == "audio") m_stream.emplace(MediaScheduledStream::Audio);
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Canonical input rejects unknown planned stream"));
    MediaAvSyncTopology topologyValue;
    if (topology.value() == "separate_rtp")
        topologyValue = MediaAvSyncTopology::SeparateRtpToSeparateRtp;
    else if (topology.value() == "mpegts")
        topologyValue = MediaAvSyncTopology::MpegTsToMpegTs;
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Canonical input rejects unknown planned topology"));
    if (order.value() == "reordered")
        m_decodeOrder.emplace(MediaDecodeOrderMode::ReorderedRequiresDecodeTime);
    else if (order.value() == "presentation")
        m_decodeOrder.emplace(MediaDecodeOrderMode::PresentationOrderNoReorder);
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Canonical input rejects unknown planned decode order"));
    m_sourceIdentity = std::move(identity).value();
    m_generation = static_cast<std::uint64_t>(generation.value());
    m_duration.emplace(MediaRunningTime::fromNanoseconds(duration.value()));
    if (*m_stream == MediaScheduledStream::Audio) {
        auto sampleRate = requiredPositiveIntNodeOption(
            options, "MediaCanonicalInputNode", "canonical_input.audio_sample_rate");
        auto sampleCount = requiredPositiveIntNodeOption(
            options, "MediaCanonicalInputNode", "canonical_input.audio_sample_count");
        if (!sampleRate || !sampleCount)
            return ::media::Status::failure(!sampleRate ? sampleRate.error() : sampleCount.error());
        m_audioSampleRate = sampleRate.value();
        m_audioSampleCount = static_cast<std::uint32_t>(sampleCount.value());
    }
    auto mapper = MediaCanonicalTimeMapper::create({
        MediaRunningTime::fromNanoseconds(sourceEpoch.value()),
        MediaRunningTime::fromNanoseconds(runningEpoch.value()),
        topologyValue, m_sourceIdentity, m_generation});
    if (!mapper) return ::media::Status::failure(mapper.error().toErrorInfo());
    m_mapper = std::make_unique<MediaCanonicalTimeMapper>(std::move(mapper).value());
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MediaCanonicalInputNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (auto configured = configure(context); !configured)
        return processProgress(configured);
    auto input = tryPopInputOptional(context, "in");
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(input.value()->get());
    if (!packet || !packet->sourceTiming())
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires validated packet source timing"));
    const auto& timing = *packet->sourceTiming();
    if (timing.readiness != MediaSourceClockReadiness::Locked ||
        timing.generation != m_generation)
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires locked matching-generation clock evidence"));
    MediaCanonicalSourceTimestamp source(
        timing.presentationNs ? std::optional<MediaRunningTime>(
            MediaRunningTime::fromNanoseconds(*timing.presentationNs)) : std::nullopt,
        timing.decodeNs ? std::optional<MediaRunningTime>(
            MediaRunningTime::fromNanoseconds(*timing.decodeNs)) : std::nullopt,
        *m_duration, timing.generation, m_sourceIdentity,
        MediaTimeMappingConfidence::Locked);
    const auto sequence = MediaSourceAccessUnitSequence(m_nextSequence);
    auto canonical = canonicalize(*input.value(), source, *m_mapper, *m_stream,
                                  *m_decodeOrder, sequence);
    if (!canonical)
        return ::media::Result<MediaNodeProcessResult>::failure(canonical.error());
    MediaAvStartupAccessUnit unit{
        *m_stream == MediaScheduledStream::Video ? MediaAvStartupStream::Video
                                                 : MediaAvStartupStream::Audio,
        m_sourceIdentity, m_nextSequence, 0, canonical.value()->canonicalPresentation(),
        canonical.value()->canonicalDuration(), MediaSourceClockReadiness::Locked,
        m_generation, canonical.value()->media()->isKeyFrame(),
        *m_stream == MediaScheduledStream::Audio
            ? std::optional<MediaAvAudioSampleSpan>(
                  MediaAvAudioSampleSpan{static_cast<std::uint32_t>(m_audioSampleRate),
                                         m_audioSampleCount})
            : std::nullopt};
    auto envelope = MediaAvStartupEnvelopeBuffer::create(
        canonical.value(), std::move(unit), canonical.value()->canonicalPresentation());
    if (!envelope)
        return ::media::Result<MediaNodeProcessResult>::failure(envelope.error());
    ++m_nextSequence;
    auto emitted = emitOutput(context, "out", envelope.value());
    return processProgress(emitted);
}

} // namespace media::ffmpeg::graph
