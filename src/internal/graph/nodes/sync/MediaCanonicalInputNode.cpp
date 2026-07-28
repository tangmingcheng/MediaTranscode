#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

extern "C" {
#include <libavutil/mathematics.h>
}

#include <limits>
#include <string>

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
    m_keyTraceEmitted = false;
    m_sourceIdentity.clear();
    m_nextSequence = 1;
    m_audioSampleRate = 0;
    m_audioSampleCount = 0;
    m_audioTimeline.reset();
}

::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>
MediaCanonicalInputNode::canonicalize(
    MediaBufferRef encodedAccessUnit,
    const MediaPacketSourceTiming& protocolTiming,
    MediaRunningTime duration,
    MediaScheduledStream stream,
    MediaDecodeOrderMode decodeOrder,
    std::string sourceIdentity,
    MediaSourceAccessUnitSequence sourceSequence,
    std::optional<MediaCanonicalAudioSampleInterval> audioInterval)
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
        std::move(encodedAccessUnit), std::move(lineage).value(),
        std::move(audioInterval));
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
    MediaScheduledStream configuredStream;
    if (stream.value() == "video") configuredStream = MediaScheduledStream::Video;
    else if (stream.value() == "audio") configuredStream = MediaScheduledStream::Audio;
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Canonical input rejects unknown planned stream"));
    MediaDecodeOrderMode configuredOrder;
    if (order.value() == "reordered")
        configuredOrder = MediaDecodeOrderMode::ReorderedRequiresDecodeTime;
    else if (order.value() == "presentation")
        configuredOrder = MediaDecodeOrderMode::PresentationOrderNoReorder;
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Canonical input rejects unknown planned decode order"));
    std::uint32_t configuredSampleCount = 0;
    if (configuredStream == MediaScheduledStream::Video) {
        if (durationSource.value() != "packet") {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical video input requires planned packet duration"));
        }
    } else {
        if (durationSource.value() != "audio_samples") {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical audio input requires planner-provided sample duration"));
        }
        auto sampleCount = requiredPositiveIntNodeOption(
            options, "MediaCanonicalInputNode",
            "canonical_input.audio_sample_count");
        if (!sampleCount) return ::media::Status::failure(sampleCount.error());
        configuredSampleCount =
            static_cast<std::uint32_t>(sampleCount.value());
    }
    int configuredSampleRate = 0;
    std::optional<MediaCanonicalAudioSourceTimeline> configuredAudioTimeline;
    if (configuredStream == MediaScheduledStream::Audio) {
        auto sampleRate = requiredPositiveIntNodeOption(
            options, "MediaCanonicalInputNode", "canonical_input.audio_sample_rate");
        if (!sampleRate) return ::media::Status::failure(sampleRate.error());
        configuredSampleRate = sampleRate.value();
        auto timeline =
            MediaCanonicalAudioSourceTimeline::create(configuredSampleRate);
        if (!timeline) return ::media::Status::failure(timeline.error());
        configuredAudioTimeline.emplace(std::move(timeline).value());
    }
    m_stream.emplace(configuredStream);
    m_decodeOrder.emplace(configuredOrder);
    m_sourceIdentity = std::move(identity).value();
    m_audioSampleRate = configuredSampleRate;
    m_audioSampleCount = configuredSampleCount;
    m_audioTimeline = std::move(configuredAudioTimeline);
    return ::media::Status::success();
}

::media::Result<MediaRunningTime> MediaCanonicalInputNode::durationFor(
    const MediaBufferRef& buffer) const
{
    if (*m_stream == MediaScheduledStream::Audio) {
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
    const MediaTimeDescriptor& time = buffer->timeDescriptor();
    if (!time.hasKnownTimeBase()) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical packet duration requires runtime time base"));
    }
    const AVRational timeBase{time.timeBase.num, time.timeBase.den};
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

::media::Result<MediaNodeProcessResult> MediaCanonicalInputNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (auto configured = configure(context); !configured)
        return processProgress(configured);
    auto input = tryReadRequiredInput(
        context.findInputChannel(nodeId(), "in"),
        "Canonical input", "in");
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            input.value()->get())) {
        if (control->controlKind() == MediaControlBufferKind::Unknown) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical input rejects unknown control"));
        }
        auto terminal = broadcastControlToAllOutputs(context, *input.value());
        return control->controlKind() == MediaControlBufferKind::Eof ||
                       control->controlKind() == MediaControlBufferKind::Abort
            ? processFinished(std::move(terminal))
            : processProgress(std::move(terminal));
    }
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(input.value()->get());
    if (!packet || !packet->sourceTiming())
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires validated packet source timing"));
    const auto& timing = *packet->sourceTiming();
    if (timing.readiness != MediaSourceClockReadiness::Locked ||
        timing.generation == 0 || !timing.presentationNs)
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical input requires locked nonzero-generation clock evidence with presentation time"));
    auto duration = durationFor(*input.value());
    if (!duration) {
        return ::media::Result<MediaNodeProcessResult>::failure(duration.error());
    }
    const auto sequence = MediaSourceAccessUnitSequence(m_nextSequence);
    std::optional<MediaCanonicalAudioSampleInterval> audioInterval;
    if (*m_stream == MediaScheduledStream::Audio) {
        if (!m_audioTimeline) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError(
                    "Canonical audio input timeline is not configured"));
        }
        auto appended = m_audioTimeline->append(
            MediaRunningTime::fromNanoseconds(*timing.presentationNs),
            m_audioSampleCount, timing.generation, m_nextSequence);
        if (!appended) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                appended.error());
        }
        audioInterval = std::move(appended).value();
    }
    const std::optional<std::int64_t> audioFirstSample =
        audioInterval
        ? std::optional<std::int64_t>(audioInterval->begin)
        : std::nullopt;
    auto canonical = canonicalize(*input.value(), timing, duration.value(),
                                  *m_stream, *m_decodeOrder, m_sourceIdentity,
                                  sequence, std::move(audioInterval));
    if (!canonical)
        return ::media::Result<MediaNodeProcessResult>::failure(canonical.error());
    MediaAvStartupAccessUnit unit{
        *m_stream == MediaScheduledStream::Video ? MediaAvStartupStream::Video
                                                 : MediaAvStartupStream::Audio,
        m_sourceIdentity, m_nextSequence, 0, canonical.value()->canonicalPresentation(),
        canonical.value()->canonicalDuration(), MediaSourceClockReadiness::Locked,
        timing.generation,
        *m_stream == MediaScheduledStream::Video &&
            canonical.value()->media()->isKeyFrame(),
        *m_stream == MediaScheduledStream::Audio
            ? std::optional<MediaAvAudioSampleSpan>(
                  MediaAvAudioSampleSpan{*audioFirstSample,
                                         static_cast<std::uint32_t>(m_audioSampleRate),
                                         m_audioSampleCount})
            : std::nullopt};
    if (*m_stream == MediaScheduledStream::Video && unit.keyFrame &&
        !m_keyTraceEmitted) {
        m_keyTraceEmitted = true;
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            std::string("rtp_key_trace stage=canonical_input media=") +
                (canonical.value()->media()->isKeyFrame() ? "1" : "0") +
                " startup_unit=" + (unit.keyFrame ? "1" : "0"));
    }
    auto envelope = MediaAvStartupEnvelopeBuffer::create(
        canonical.value(), std::move(unit), canonical.value()->canonicalPresentation());
    if (!envelope)
        return ::media::Result<MediaNodeProcessResult>::failure(envelope.error());
    ++m_nextSequence;
    auto emitted = emitOutput(context, "out", envelope.value());
    return processProgress(emitted);
}

} // namespace media::ffmpeg::graph
