#include "internal/graph/nodes/audio/AudioEncodeNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"
#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

extern "C" {
#include <libavutil/error.h>
}

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

AudioEncodeLineageState::AudioEncodeLineageState(
    MediaAudioLineageExecutionMode mode,
    std::size_t capacity) noexcept
    : MediaAudioLineageState(
          mode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
          capacity)
    , frameQueue(mode, capacity)
{
}

void AudioEncodeLineageState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    if (m_codecApi && m_codecContext) {
        m_codecApi->flushBuffers(m_codecContext);
    }
    clearLineageStorage();
}

void AudioEncodeLineageState::clearLineageStorage() noexcept
{
    receivePending = false;
    frameQueue.reset();
    pendingFrame.reset();
    pendingFragments.clear();
    submittedFragments.clear();
    activeOrigin.reset();
    flushPending = false;
    flushIsEof = false;
    flushSent = false;
    flushBuffer.reset();
    terminals.reset();
    eofEmitted = false;
}

void AudioEncodeLineageState::resetForLifecycle() noexcept
{
    clearLineageStorage();
    resetLifecycleLineage();
    resetCodecBinding();
}

::media::Status AudioEncodeLineageState::preflightIncomingLineage(
    const std::vector<MediaAudioIntervalFragment>& incoming) const
{
    MediaAudioLineageCapacity leases(capacity());
    if (auto status = frameQueue.observeLineageCapacity(leases); !status) {
        return status;
    }
    if (!pendingFragments.empty()) {
        if (auto status = leases.observe(pendingFragments); !status) {
            return status;
        }
    }
    for (const auto& submitted : submittedFragments) {
        if (auto status = leases.observe(submitted); !status) {
            return status;
        }
    }
    return leases.observe(incoming);
}

void AudioEncodeLineageState::setCodecApi(
    std::shared_ptr<AudioEncoderCodecApi> codecApi) noexcept
{
    m_codecApi = std::move(codecApi);
}

void AudioEncodeLineageState::bindCodec(
    MediaBufferRef owner, AVCodecContext* context) noexcept
{
    m_codecOwner = std::move(owner);
    m_codecContext = context;
}

void AudioEncodeLineageState::resetCodecBinding() noexcept
{
    m_codecContext = nullptr;
    m_codecOwner.reset();
}

AudioEncodeNode::AudioEncodeNode(
    MediaNodeId nodeId,
    MediaAudioLineageExecutionMode lineageMode,
    std::shared_ptr<AudioEncodeLineageState> lineageState)
    : AudioEncodeNode(nodeId, lineageMode, std::move(lineageState),
                      makeFFmpegAudioEncoderCodecApi())
{
}

AudioEncodeNode::AudioEncodeNode(
    MediaNodeId nodeId,
    MediaAudioLineageExecutionMode lineageMode,
    std::shared_ptr<AudioEncodeLineageState> lineageState,
    std::shared_ptr<AudioEncoderCodecApi> codecApi)
    : FFmpegCodecNodeRuntime(nodeId, staticKind(), "AudioEncodeNode")
    , m_lineageState(std::move(lineageState))
    , m_receivePending(m_lineageState->receivePending)
    , m_flushPending(m_lineageState->flushPending)
    , m_flushIsEof(m_lineageState->flushIsEof)
    , m_flushSent(m_lineageState->flushSent)
    , m_flushBuffer(m_lineageState->flushBuffer)
    , m_codecApi(std::move(codecApi))
    , m_frameQueue(m_lineageState->frameQueue)
    , m_pendingFrame(m_lineageState->pendingFrame)
    , m_pendingFragments(m_lineageState->pendingFragments)
    , m_submittedFragments(m_lineageState->submittedFragments)
    , m_lineageMode(lineageMode)
    , m_activeOrigin(m_lineageState->activeOrigin)
{
    m_lineageState->setCodecApi(m_codecApi);
}

MediaNodeKind AudioEncodeNode::staticKind() noexcept
{
    return MediaNodeKind::AudioEncode;
}

std::string_view AudioEncodeNode::generationPurgeIdentity() noexcept
{
    return MediaAudioEncodeLineageIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
AudioEncodeNode::generationPurgeTarget() const noexcept
{
    return m_lineageState->synchronized() ? m_lineageState : nullptr;
}

bool AudioEncodeNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    if (dynamic_cast<const FFmpegCodecContextBuffer*>(buffer.get())) {
        return true;
    }
    const auto* encoded = dynamic_cast<const MediaEncodedAudioLineageBuffer*>(buffer.get());
    return m_lineageState && m_lineageState->pendingOutputIsCurrent(
        buffer, encoded ? std::optional<std::uint64_t>(
                              encoded->audioOrigin().generation)
                        : std::nullopt);
}

::media::Status AudioEncodeNode::start(MediaGraphExecutionContext& context) { resetRuntimeState(); return FFmpegCodecNodeRuntime::start(context); }
::media::Status AudioEncodeNode::stop(MediaGraphExecutionContext& context) { auto status = FFmpegCodecNodeRuntime::stop(context); resetRuntimeState(); return status; }
void AudioEncodeNode::abort(MediaGraphExecutionContext& context) noexcept { FFmpegCodecNodeRuntime::abort(context); resetRuntimeState(); }
void AudioEncodeNode::resetRuntimeState() noexcept
{
    auto lineageLock = m_lineageState->lock();
    m_encoderConfigEmitted = false;
    m_lineageState->resetForLifecycle();
}

::media::Result<MediaNodeProcessResult> AudioEncodeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto lineageLock = m_lineageState->lock();
    if (m_flushPending) return continueFlush(context);
    if (m_receivePending) {
        auto receiveResult = receivePackets(context);
        if (!receiveResult) return processProgress(::media::Status::failure(receiveResult.error()));
        m_receivePending = false;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_pendingFrame || m_frameQueue.hasFullFrame()) {
        return encodeQueuedFrame(context, false);
    }
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
                ::media::ErrorInfo::invalidArgument("AudioEncodeNode expected codec context on codec input"));
        }
        m_lineageState->bindCodec(*codecInput.value(), codecContext());
        if (codecContext()->frame_size > 0) {
            auto queueStatus = m_frameQueue.configure(*codecContext());
            if (!queueStatus) {
                return ::media::Result<MediaNodeProcessResult>::failure(queueStatus.error());
            }
        }
        auto emitStatus = emitEncoderConfig(context, *codecInput.value());
        if (!emitStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(emitStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* frameInput = context.findInputChannel(nodeId(), "frame");
        if (frameInput && frameInput->closed()) {
            m_lineageState->terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }

    const MediaBufferRef& buffer = *input.value();
    if (tryBindCodecContext(buffer)) {
        m_lineageState->bindCodec(buffer, codecContext());
        auto emitStatus = emitEncoderConfig(context, buffer);
        if (!emitStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(emitStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }

    if (buffer->isEof() || buffer->isFlush()) {
        const bool eof = buffer->isEof();
        if (eof && m_lineageState->eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        m_flushPending = true;
        m_flushIsEof = eof;
        m_flushSent = false;
        m_flushBuffer = buffer;
        return continueFlush(context);
    }

    MediaBufferRef media = buffer;
    std::vector<MediaAudioIntervalFragment> fragments;
    std::optional<MediaAudioPlaybackOrigin> incomingOrigin;
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(buffer.get());
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        if (!bound) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Synchronized AudioEncodeNode requires bound canonical audio"));
        }
        if (!m_lineageState) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized AudioEncodeNode requires planned lineage state"));
        }
        if (auto status = m_lineageState->validateObservation(
                bound->audioOrigin().generation); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        if (m_activeOrigin && *m_activeOrigin != bound->audioOrigin()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioEncodeNode rejects changed active playback origin"));
        }
        incomingOrigin = bound->audioOrigin();
        media = bound->media()->media();
        fragments = bound->media()->fragments();
        if (auto status = m_lineageState->preflightIncomingLineage(
                fragments); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    } else if (bound) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Legacy AudioEncodeNode rejects bound canonical audio"));
    }
    AVFrame* frame = FFmpegFrameView::writableFrame(media);
    if (!frame) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("AudioEncodeNode expected frame buffer"));
    }

    ::media::ffmpeg::FramePtr pendingFrame;
    if (!m_frameQueue.configured()) {
        pendingFrame.reset(av_frame_clone(frame));
        if (!pendingFrame) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "AudioEncodeNode failed to retain an exact pending frame"));
        }
    }
    if (incomingOrigin) {
        if (auto status = m_lineageState->observe(incomingOrigin->generation); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        if (!m_activeOrigin) {
            m_activeOrigin = *incomingOrigin;
        }
    }

    if (m_frameQueue.configured()) {
        auto queueStatus = m_frameQueue.push(*frame, std::move(fragments));
        if (!queueStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(queueStatus.error());
        }
        if (!m_frameQueue.hasFullFrame()) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
        }
        return encodeQueuedFrame(context, false);
    }

    if (!m_codecApi) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("AudioEncodeNode requires an encoder codec API"));
    }
    m_pendingFrame = std::move(pendingFrame);
    m_pendingFragments = std::move(fragments);
    return encodeQueuedFrame(context, false);
}

::media::Result<MediaNodeProcessResult> AudioEncodeNode::encodeQueuedFrame(
    MediaGraphExecutionContext& context,
    bool allowPartial)
{
    if (!m_pendingFrame) {
        auto frame = m_frameQueue.hasFullFrame()
            ? m_frameQueue.popFullFrame()
            : (allowPartial ? m_frameQueue.popRemainingFrame()
                            : ::media::Result<AudioEncoderFrameQueue::QueuedFrame>::failure(
                                  ::media::ErrorInfo::wouldBlock("AudioEncodeNode has no complete queued frame")));
        if (!frame) {
            return ::media::Result<MediaNodeProcessResult>::failure(frame.error());
        }
        auto queued = std::move(frame).value();
        m_pendingFrame = std::move(queued.media);
        m_pendingFragments = std::move(queued.fragments);
    }

    const int sendRet = m_codecApi->sendFrame(codecContext(), m_pendingFrame.get());
    if (sendRet == AVERROR(EAGAIN)) {
        auto receiveStatus = receivePackets(context);
        if (!receiveStatus) {
            return processProgress(::media::Status::failure(receiveStatus.error()));
        }
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (sendRet < 0) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            FFmpegGraphError::fromCode(sendRet, "avcodec_send_frame(audio queued)"));
    }
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        m_submittedFragments.push_back(std::move(m_pendingFragments));
    }
    m_pendingFrame.reset();

    auto receiveStatus = receivePackets(context);
    if (!receiveStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(receiveStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Status AudioEncodeNode::emitEncoderConfig(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    if (m_encoderConfigEmitted || !buffer) {
        return ::media::Status::success();
    }
    if (!context.findOutputChannel(nodeId(), "codec")) {
        m_encoderConfigEmitted = true;
        return ::media::Status::success();
    }
    auto status = emitOutput(context, "codec", buffer);
    if (!status) {
        return status;
    }
    m_encoderConfigEmitted = true;
    return ::media::Status::success();
}

::media::Result<bool> AudioEncodeNode::receivePackets(MediaGraphExecutionContext& context)
{
    while (true) {
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::allocationFailed("AudioEncodeNode failed: av_packet_alloc returned null"));
        }

        const int ret = m_codecApi->receivePacket(codecContext(), packet.get());
        if (ret == AVERROR(EAGAIN)) return ::media::Result<bool>::success(false);
        if (ret == AVERROR_EOF) return ::media::Result<bool>::success(true);

        if (ret < 0) {
            return ::media::Result<bool>::failure(FFmpegGraphError::fromCode(ret, "avcodec_receive_packet(audio)"));
        }

        auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), MediaStreamKind::Audio, std::nullopt);
        if (!buffer) {
            return ::media::Result<bool>::failure(buffer.error());
        }

        MediaTimeDescriptor timeDescriptor;
        timeDescriptor.timeBase = MediaRational{ codecContext()->time_base.num, codecContext()->time_base.den };
        buffer.value()->setTimeDescriptor(timeDescriptor);

        MediaBufferRef output = buffer.value();
        if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
            if (!m_activeOrigin || m_submittedFragments.empty()) {
                return ::media::Result<bool>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "AudioEncodeNode packet has no exact submitted lineage"));
            }
            auto encoded = MediaEncodedAudioLineageBuffer::create(
                output, std::move(m_submittedFragments.front()), *m_activeOrigin);
            m_submittedFragments.pop_front();
            if (!encoded) return ::media::Result<bool>::failure(encoded.error());
            output = std::move(encoded).value();
        }
        auto pushStatus = emitOutput(context, "packet", output);
        if (!pushStatus) {
            if (pushStatus.error().code == ::media::ErrorCode::WouldBlock && !m_flushPending) m_receivePending = true;
            return ::media::Result<bool>::failure(pushStatus.error());
        }
    }
}

::media::Result<MediaNodeProcessResult> AudioEncodeNode::continueFlush(MediaGraphExecutionContext& context)
{
    if (m_frameQueue.configured() &&
        (m_pendingFrame || m_frameQueue.queuedSamples() > 0)) {
        return encodeQueuedFrame(context, true);
    }
    if (!m_flushSent) {
        const int sendRet = m_codecApi->sendFrame(codecContext(), nullptr);
        if (sendRet == 0 || sendRet == AVERROR_EOF) m_flushSent = true;
        else if (sendRet != AVERROR(EAGAIN))
            return ::media::Result<MediaNodeProcessResult>::failure(
                FFmpegGraphError::fromCode(sendRet, "avcodec_send_frame(audio flush)"));
    }
    auto drain = receivePackets(context);
    if (!drain) return processProgress(::media::Status::failure(drain.error()));
    if (!drain.value()) return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    if (m_lineageMode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        if (!m_submittedFragments.empty()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "AudioEncodeNode finished with delayed lineage residue"));
        }
        if (auto status = m_frameQueue.finishLineage(); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }
    const bool eof = m_flushIsEof;
    MediaBufferRef terminal = std::move(m_flushBuffer);
    m_flushPending = false;
    m_flushIsEof = false;
    m_flushSent = false;
    if (eof) { m_lineageState->terminals.markEof("frame"); m_lineageState->eofEmitted = true; }
    if (auto freshness = m_lineageState->authorizeRetainedControl(terminal);
        !freshness) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            freshness.error());
    }
    auto status = emitOutput(context, "packet", terminal);
    return eof ? processFinished(std::move(status)) : processProgress(std::move(status));
}

} // namespace media::ffmpeg::graph
