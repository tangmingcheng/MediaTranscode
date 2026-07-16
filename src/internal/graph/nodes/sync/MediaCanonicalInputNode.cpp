#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

extern "C" {
#include <libavutil/mathematics.h>
}

#include <limits>
#include <numeric>

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
    m_stream.reset();
    m_decodeOrder.reset();
    m_durationSource.reset();
    m_sourceIdentity.clear();
    m_nextSequence = 1;
    m_audioSampleRate = 0;
    m_audioSampleCount = 0;
}

::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>
MediaCanonicalInputNode::canonicalize(
    MediaBufferRef encodedAccessUnit,
    const MediaPacketSourceTiming& protocolTiming,
    MediaRunningTime duration,
    MediaScheduledStream stream,
    MediaDecodeOrderMode decodeOrder,
    std::string sourceIdentity,
    MediaSourceAccessUnitSequence sourceSequence)
{
    const auto expected = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video : MediaStreamKind::Audio;
    if (!encodedAccessUnit || encodedAccessUnit->streamKind() != expected ||
        protocolTiming.readiness != MediaSourceClockReadiness::Locked ||
        protocolTiming.generation == 0 || !protocolTiming.presentationNs ||
        sourceIdentity.empty() || duration.nanoseconds() <= 0) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires validated protocol presentation time"));
    }
    auto lineage = createMediaCanonicalLineage(
        MediaRunningTime::fromNanoseconds(*protocolTiming.presentationNs),
        protocolTiming.decodeNs
            ? std::optional<MediaRunningTime>(MediaRunningTime::fromNanoseconds(
                  *protocolTiming.decodeNs))
            : std::nullopt,
        duration, decodeOrder, std::move(sourceIdentity), sourceSequence,
        MediaTimeMappingConfidence::Locked, protocolTiming.generation);
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
    if (m_stream) return ::media::Status::success();
    const auto* options = nodeOptions(context);
    auto stream = requiredNodeOption(options, "MediaCanonicalInputNode",
                                     "canonical_input.stream");
    auto identity = requiredNodeOption(options, "MediaCanonicalInputNode",
                                       "canonical_input.source_identity");
    auto durationSource = requiredNodeOption(options, "MediaCanonicalInputNode",
                                             "canonical_input.duration_source");
    auto order = requiredNodeOption(options, "MediaCanonicalInputNode",
                                    "canonical_input.decode_order");
    if (!stream || !identity || !durationSource || !order) {
        if (!stream) return ::media::Status::failure(stream.error());
        if (!identity) return ::media::Status::failure(identity.error());
        if (!durationSource) return ::media::Status::failure(durationSource.error());
        return ::media::Status::failure(order.error());
    }
    if (stream.value() == "video") m_stream.emplace(MediaScheduledStream::Video);
    else if (stream.value() == "audio") m_stream.emplace(MediaScheduledStream::Audio);
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Canonical input rejects unknown planned stream"));
    if (order.value() == "reordered")
        m_decodeOrder.emplace(MediaDecodeOrderMode::ReorderedRequiresDecodeTime);
    else if (order.value() == "presentation")
        m_decodeOrder.emplace(MediaDecodeOrderMode::PresentationOrderNoReorder);
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Canonical input rejects unknown planned decode order"));
    m_sourceIdentity = std::move(identity).value();
    if (durationSource.value() == "packet") {
        m_durationSource = DurationSource::Packet;
    } else if (durationSource.value() == "audio_samples" &&
               *m_stream == MediaScheduledStream::Audio) {
        m_durationSource = DurationSource::AudioSamples;
        auto sampleCount = requiredPositiveIntNodeOption(
            options, "MediaCanonicalInputNode", "canonical_input.audio_sample_count");
        if (!sampleCount) return ::media::Status::failure(sampleCount.error());
        m_audioSampleCount = static_cast<std::uint32_t>(sampleCount.value());
    } else {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Canonical input rejects unknown planned duration source"));
    }
    if (*m_stream == MediaScheduledStream::Audio) {
        auto sampleRate = requiredPositiveIntNodeOption(
            options, "MediaCanonicalInputNode", "canonical_input.audio_sample_rate");
        if (!sampleRate) return ::media::Status::failure(sampleRate.error());
        m_audioSampleRate = sampleRate.value();
    }
    return ::media::Status::success();
}

::media::Result<MediaRunningTime> MediaCanonicalInputNode::durationFor(
    const MediaBufferRef& buffer) const
{
    if (*m_durationSource == DurationSource::AudioSamples) {
        const auto whole = m_audioSampleCount / static_cast<std::uint32_t>(m_audioSampleRate);
        const auto remainder = m_audioSampleCount % static_cast<std::uint32_t>(m_audioSampleRate);
        if (whole > static_cast<std::uint64_t>(
                        std::numeric_limits<std::int64_t>::max() / 1'000'000'000LL)) {
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical audio sample duration overflows nanoseconds"));
        }
        const std::int64_t duration =
            static_cast<std::int64_t>(whole) * 1'000'000'000LL +
            (static_cast<std::int64_t>(remainder) * 1'000'000'000LL +
             m_audioSampleRate / 2) / m_audioSampleRate;
        if (duration <= 0) {
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical audio sample duration is not positive"));
        }
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(duration));
    }
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    if (!packet || !packet->packet() || packet->packet()->duration <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical packet duration requires positive runtime evidence"));
    }
    AVRational timeBase = packet->packet()->time_base;
    if (timeBase.num <= 0 || timeBase.den <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical packet duration requires runtime time base"));
    }
    const auto nanoseconds = av_rescale_q_rnd(
        packet->packet()->duration, timeBase, AVRational{1, 1'000'000'000},
        static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    if (nanoseconds <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical packet duration is not representable"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(nanoseconds));
}

::media::Result<std::uint32_t> MediaCanonicalInputNode::audioSampleCountFor(
    const MediaBufferRef& buffer) const
{
    if (*m_durationSource == DurationSource::AudioSamples) {
        return ::media::Result<std::uint32_t>::success(m_audioSampleCount);
    }
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    if (!packet || !packet->packet() || packet->packet()->duration <= 0 ||
        packet->packet()->time_base.num <= 0 ||
        packet->packet()->time_base.den <= 0 || m_audioSampleRate <= 0) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio packet sample span requires runtime evidence"));
    }
    const auto duration = packet->packet()->duration;
    auto denominator =
        static_cast<std::int64_t>(packet->packet()->time_base.den);
    auto numerator =
        static_cast<std::int64_t>(packet->packet()->time_base.num) *
        static_cast<std::int64_t>(m_audioSampleRate);
    if (numerator <= 0) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio packet sample span overflows"));
    }
    const auto divisor = std::gcd(numerator, denominator);
    numerator /= divisor;
    denominator /= divisor;
    if (duration % denominator != 0) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio packet sample span must be exact"));
    }
    const auto units = duration / denominator;
    if (units > std::numeric_limits<std::int64_t>::max() / numerator) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio packet sample span overflows"));
    }
    const auto samples = units * numerator;
    if (samples <= 0 ||
        samples > std::numeric_limits<std::uint32_t>::max()) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio packet sample span is not representable"));
    }
    return ::media::Result<std::uint32_t>::success(
        static_cast<std::uint32_t>(samples));
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
        timing.generation == 0)
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires locked nonzero-generation clock evidence"));
    auto duration = durationFor(*input.value());
    if (!duration) {
        return ::media::Result<MediaNodeProcessResult>::failure(duration.error());
    }
    std::optional<std::uint32_t> audioSampleCount;
    if (*m_stream == MediaScheduledStream::Audio) {
        auto samples = audioSampleCountFor(*input.value());
        if (!samples) {
            return ::media::Result<MediaNodeProcessResult>::failure(samples.error());
        }
        audioSampleCount = samples.value();
    }
    const auto sequence = MediaSourceAccessUnitSequence(m_nextSequence);
    auto canonical = canonicalize(*input.value(), timing, duration.value(),
                                  *m_stream, *m_decodeOrder, m_sourceIdentity,
                                  sequence);
    if (!canonical)
        return ::media::Result<MediaNodeProcessResult>::failure(canonical.error());
    MediaAvStartupAccessUnit unit{
        *m_stream == MediaScheduledStream::Video ? MediaAvStartupStream::Video
                                                 : MediaAvStartupStream::Audio,
        m_sourceIdentity, m_nextSequence, 0, canonical.value()->canonicalPresentation(),
        canonical.value()->canonicalDuration(), MediaSourceClockReadiness::Locked,
        timing.generation, canonical.value()->media()->isKeyFrame(),
        *m_stream == MediaScheduledStream::Audio
            ? std::optional<MediaAvAudioSampleSpan>(
                  MediaAvAudioSampleSpan{static_cast<std::uint32_t>(m_audioSampleRate),
                                         *audioSampleCount})
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
