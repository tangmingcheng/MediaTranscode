#include "internal/graph/nodes/audio/AudioDecodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaDecodedAudioTrimInputBuffer.h"
#include "internal/graph/nodes/audio/MediaAudioDecodeInputView.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"
#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/intreadwrite.h>
}

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::optional<std::uint32_t>> packetDiscardPadding(
    const AVPacket& packet)
{
    std::size_t sideDataSize = 0;
    const std::uint8_t* sideData = av_packet_get_side_data(
        &packet, AV_PKT_DATA_SKIP_SAMPLES, &sideDataSize);
    if (!sideData) {
        return ::media::Result<std::optional<std::uint32_t>>::success(
            std::nullopt);
    }
    if (sideDataSize < 8) {
        return ::media::Result<std::optional<std::uint32_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio decoder skip-samples evidence is truncated"));
    }
    const std::uint32_t discardPadding = AV_RL32(sideData + 4);
    return ::media::Result<std::optional<std::uint32_t>>::success(
        discardPadding == 0
            ? std::nullopt
            : std::optional<std::uint32_t>(discardPadding));
}

} // namespace

AudioDecodeLineageState::AudioDecodeLineageState(
    MediaAudioLineageExecutionMode mode,
    std::size_t capacity) noexcept
    : MediaAudioLineageState(
          mode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
          capacity)
{
}

void AudioDecodeLineageState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    if (m_codecApi && m_codecContext) {
        m_codecApi->flushBuffers(m_codecContext);
    }
    clearLineageStorage();
}

void AudioDecodeLineageState::clearLineageStorage() noexcept
{
    receivePending = false;
    pendingPacket.reset();
    intervals.reset();
    discardPaddingProof.reset();
    activeOrigin.reset();
    startupTrimDirective = 0;
    startupTrimDirectiveEmitted = false;
    flushPending = false;
    flushIsEof = false;
    flushSent = false;
    flushBuffer.reset();
    terminals.reset();
    eofEmitted = false;
}

void AudioDecodeLineageState::resetForLifecycle() noexcept
{
    clearLineageStorage();
    resetLifecycleLineage();
    resetCodecBinding();
}

void AudioDecodeLineageState::setCodecApi(
    std::shared_ptr<AudioDecoderCodecApi> codecApi) noexcept
{
    m_codecApi = std::move(codecApi);
}

void AudioDecodeLineageState::bindCodec(
    MediaBufferRef owner, AVCodecContext* context) noexcept
{
    m_codecOwner = std::move(owner);
    m_codecContext = context;
}

void AudioDecodeLineageState::resetCodecBinding() noexcept
{
    m_codecContext = nullptr;
    m_codecOwner.reset();
}

AudioDecodeNode::AudioDecodeNode(
    MediaNodeId nodeId,
    MediaAudioLineageExecutionMode lineageMode,
    std::shared_ptr<AudioDecodeLineageState> lineageState)
    : AudioDecodeNode(nodeId, lineageMode, std::move(lineageState),
                      makeFFmpegAudioDecoderCodecApi())
{
}

AudioDecodeNode::AudioDecodeNode(
    MediaNodeId nodeId,
    MediaAudioLineageExecutionMode lineageMode,
    std::shared_ptr<AudioDecodeLineageState> lineageState,
    std::shared_ptr<AudioDecoderCodecApi> codecApi)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "AudioDecodeNode")
    , m_lineageMode(lineageMode)
    , m_lineageState(std::move(lineageState))
    , m_codecApi(std::move(codecApi))
    , m_flushPending(m_lineageState->flushPending)
    , m_flushIsEof(m_lineageState->flushIsEof)
    , m_flushSent(m_lineageState->flushSent)
    , m_flushBuffer(m_lineageState->flushBuffer)
{
    m_lineageState->setCodecApi(m_codecApi);
}

MediaNodeKind AudioDecodeNode::staticKind() noexcept
{
    return MediaNodeKind::AudioDecode;
}

::media::Status AudioDecodeNode::start(MediaGraphExecutionContext& context) { resetRuntimeState(); return FFmpegCodecNodeRuntime::start(context); }
::media::Status AudioDecodeNode::stop(MediaGraphExecutionContext& context) { auto status = FFmpegCodecNodeRuntime::stop(context); resetRuntimeState(); return status; }
void AudioDecodeNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegCodecNodeRuntime::abort(context); resetRuntimeState(); }
void AudioDecodeNode::resetRuntimeState() noexcept
{
    auto lineageLock = m_lineageState->lock();
    m_lineageState->resetForLifecycle();
}

::media::Result<MediaNodeProcessResult> AudioDecodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    if (m_flushPending) return continueFlush(context);
    if (m_lineageState->receivePending) {
        auto receiveResult = receiveFrames(context);
        if (!receiveResult) return processProgress(::media::Status::failure(receiveResult.error()));
        m_lineageState->receivePending = false;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_lineageState->pendingPacket) return submitPendingPacket(context);
    if (m_lineageState->terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    if (!hasCodecContext()) {
        auto codecInput = tryPopInputOptional(context, "codec");
        if (!codecInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(codecInput.error());
        }
        if (!codecInput.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        }
        if (!tryBindCodecContext(*codecInput.value())) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument("AudioDecodeNode expected codec context on codec input"));
        }
        m_lineageState->bindCodec(*codecInput.value(), codecContext());
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* packetInput = context.findInputChannel(nodeId(), "packet");
        if (packetInput && packetInput->closed()) {
            m_lineageState->terminals.markClosed("packet");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const MediaBufferRef& buffer = *input.value();
    if (buffer->isEof() || buffer->isFlush()) {
        const bool eof = buffer->isEof();
        if (eof && m_lineageState->eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        m_flushPending = true; m_flushIsEof = eof; m_flushSent = false; m_flushBuffer = buffer;
        return continueFlush(context);
    }

    auto resolved = resolveMediaAudioDecodeInput(buffer, m_lineageMode);
    if (!resolved) {
        return ::media::Result<MediaNodeProcessResult>::failure(resolved.error());
    }
    const AVPacket* packet = FFmpegPacketView::packet(resolved.value().packet);
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "AudioDecodeNode requires an FFmpeg packet"));
    }
    ::media::ffmpeg::PacketPtr pendingPacket(av_packet_clone(packet));
    if (!pendingPacket) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::allocationFailed(
                "AudioDecodeNode failed to retain packet ownership"));
    }
    std::optional<MediaAudioIntervalAccumulator> candidateIntervals;
    std::optional<MediaAudioPlaybackOrigin> incomingOrigin;
    std::optional<AudioDecoderDiscardPaddingProof> incomingDiscardPadding;
    std::uint32_t incomingTrim = 0;
    if (resolved.value().synchronized) {
        const auto& synchronized = *resolved.value().synchronized;
        if (!m_lineageState) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized AudioDecodeNode requires planned lineage state"));
        }
        if (auto status = m_lineageState->validateObservation(
                synchronized.origin.generation); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        if (m_lineageState->discardPaddingProof) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioDecodeNode received media after terminal discard-padding evidence"));
        }
        if (m_lineageState->activeOrigin &&
            (synchronized.origin != *m_lineageState->activeOrigin ||
             synchronized.trimLeadingSamples != 0)) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioDecodeNode accepts startup trim exactly once per origin"));
        }
        incomingOrigin = synchronized.origin;
        incomingTrim = synchronized.trimLeadingSamples;
        if (!codecContext() || codecContext()->sample_rate <= 0) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "AudioDecodeNode requires the source codec sample rate"));
        }
        const auto& sourceInterval = synchronized.sourceInterval;
        if (sourceInterval.sampleRate != codecContext()->sample_rate) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioDecodeNode source interval sample rate conflicts with codec"));
        }
        const MediaAudioIntervalFragment incomingFragment{
            synchronized.lineage,
            sourceInterval};
        MediaAudioLineageCapacity leases(m_lineageState->capacity());
        if (auto status =
                m_lineageState->intervals.observeLineageCapacity(leases);
            !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        if (auto status = leases.observe(incomingFragment.lineage); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        candidateIntervals = m_lineageState->intervals;
        if (auto status = candidateIntervals->push(incomingFragment); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        auto discardPadding = packetDiscardPadding(*packet);
        if (!discardPadding) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                discardPadding.error());
        }
        if (discardPadding.value()) {
            incomingDiscardPadding =
                AudioDecoderDiscardPaddingProof{
                    *discardPadding.value(), synchronized.origin.generation};
        }
    }
    if (incomingOrigin) {
        if (auto status = m_lineageState->observe(incomingOrigin->generation); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        if (!m_lineageState->activeOrigin) {
            m_lineageState->activeOrigin = *incomingOrigin;
            m_lineageState->startupTrimDirective = incomingTrim;
            m_lineageState->startupTrimDirectiveEmitted = false;
        }
        m_lineageState->intervals = std::move(*candidateIntervals);
        m_lineageState->discardPaddingProof = incomingDiscardPadding;
    }
    m_lineageState->pendingPacket = std::move(pendingPacket);
    return submitPendingPacket(context);
}

std::string_view AudioDecodeNode::generationPurgeIdentity() noexcept
{
    return MediaAudioDecodeLineageIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
AudioDecodeNode::generationPurgeTarget() const noexcept
{
    return m_lineageState->synchronized() ? m_lineageState : nullptr;
}

bool AudioDecodeNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto* decoded = dynamic_cast<const MediaDecodedAudioTrimInputBuffer*>(buffer.get());
    return m_lineageState && m_lineageState->pendingOutputIsCurrent(
        buffer, decoded ? std::optional<std::uint64_t>(
                              decoded->audioOrigin().generation)
                        : std::nullopt);
}

::media::Result<MediaNodeProcessResult> AudioDecodeNode::submitPendingPacket(
    MediaGraphExecutionContext& context)
{
    const int sendRet = m_codecApi->sendPacket(
        codecContext(), m_lineageState->pendingPacket.get());
    if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(audio)"));
    }
    if (sendRet == 0) {
        m_lineageState->pendingPacket.reset();
    }

    auto receiveStatus = receiveFrames(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<bool> AudioDecodeNode::receiveFrames(MediaGraphExecutionContext& context)
{
    while (true) {
        auto frame = ::media::ffmpeg::makeFrame();
        if (!frame) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::allocationFailed("AudioDecodeNode failed: av_frame_alloc returned null"));
        }

        const int ret = m_codecApi->receiveFrame(codecContext(), frame.get());
        if (ret == AVERROR(EAGAIN)) return ::media::Result<bool>::success(false);
        if (ret == AVERROR_EOF) return ::media::Result<bool>::success(true);

        if (ret < 0) {
            return ::media::Result<bool>::failure(FFmpegGraphError::fromCode(ret, "avcodec_receive_frame(audio)"));
        }

        const int decodedSamples = frame->nb_samples;
        const int decodedRate = frame->sample_rate;
        if (!codecContext() || decodedRate <= 0 ||
            decodedRate != codecContext()->sample_rate) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioDecodeNode decoded sample rate changed from its source codec"));
        }
        auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
        if (!buffer) {
            return ::media::Result<bool>::failure(buffer.error());
        }

        if (codecContext()->pkt_timebase.num > 0 && codecContext()->pkt_timebase.den > 0) {
            MediaTimeDescriptor timeDescriptor;
            timeDescriptor.timeBase = MediaRational{ codecContext()->pkt_timebase.num, codecContext()->pkt_timebase.den };
            buffer.value()->setTimeDescriptor(timeDescriptor);
        }

        MediaBufferRef output = buffer.value();
        if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
            if (!m_lineageState->activeOrigin || decodedSamples <= 0 || decodedRate <= 0) {
                return ::media::Result<bool>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Synchronized AudioDecodeNode requires an active canonical origin"));
            }
            auto fragments = m_lineageState->intervals.take(decodedSamples);
            if (!fragments) {
                return ::media::Result<bool>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "AudioDecodeNode decoded samples exceed exact submitted intervals"));
            }
            auto canonical = MediaCanonicalAudioSamplesBuffer::create(
                output, std::move(fragments).value());
            if (!canonical) return ::media::Result<bool>::failure(canonical.error());
            const auto trim = m_lineageState->startupTrimDirectiveEmitted
                ? 0U
                : m_lineageState->startupTrimDirective;
            auto decoded = MediaDecodedAudioTrimInputBuffer::create(
                std::move(canonical).value(), *m_lineageState->activeOrigin,
                trim);
            if (!decoded) return ::media::Result<bool>::failure(decoded.error());
            m_lineageState->startupTrimDirectiveEmitted = true;
            output = std::move(decoded).value();
        }
        auto pushStatus = pushToMatchingOutputs(context, output, MediaStreamKind::Audio);
        if (!pushStatus) {
            if (pushStatus.error().code == ::media::ErrorCode::WouldBlock && !m_flushPending) {
                m_lineageState->receivePending = true;
            }
            return ::media::Result<bool>::failure(pushStatus.error());
        }
    }
}

::media::Result<MediaNodeProcessResult> AudioDecodeNode::continueFlush(MediaGraphExecutionContext& context)
{
    if (!m_flushSent) {
        const int sendRet = m_codecApi->sendPacket(codecContext(), nullptr);
        if (sendRet == 0 || sendRet == AVERROR_EOF) m_flushSent = true;
        else if (sendRet != AVERROR(EAGAIN)) return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_packet(audio flush)"));
    }
    auto drain = receiveFrames(context);
    if (!drain) return processProgress(::media::Status::failure(drain.error()));
    if (!drain.value()) return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        if (m_lineageState->pendingPacket) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioDecodeNode codec EOF retains an unsubmitted packet"));
        }
        auto candidateIntervals = m_lineageState->intervals;
        const auto queuedSamples = candidateIntervals.queuedSamples();
        const auto& proof = m_lineageState->discardPaddingProof;
        if (queuedSamples < 0 ||
            (queuedSamples != 0 &&
             (!proof || !m_lineageState->activeOrigin ||
              proof->generation != m_lineageState->activeOrigin->generation ||
              proof->samples != static_cast<std::uint64_t>(queuedSamples))) ||
            (queuedSamples == 0 && proof)) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioDecodeNode codec EOF residue lacks exact discard-padding evidence"));
        }
        if (auto settled = candidateIntervals.settleDroppedSamples(
                proof ? proof->samples : 0); !settled) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                settled.error());
        }
        m_lineageState->intervals = std::move(candidateIntervals);
        m_lineageState->discardPaddingProof.reset();
    }
    const bool eof = m_flushIsEof; MediaBufferRef terminal = std::move(m_flushBuffer);
    m_flushPending = false; m_flushIsEof = false; m_flushSent = false;
    if (eof) { m_lineageState->terminals.markEof("packet"); m_lineageState->eofEmitted = true; }
    if (auto freshness = m_lineageState->authorizeRetainedControl(terminal);
        !freshness) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            freshness.error());
    }
    auto status = broadcastControlToAllOutputs(context, terminal);
    return eof ? processFinished(std::move(status)) : processProgress(std::move(status));
}

} // namespace media::ffmpeg::graph
