#include "internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h"

#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"

#include <array>
#include <span>

namespace media::ffmpeg::graph {

MediaAvBoundReleaseExtractorNode::MediaAvBoundReleaseExtractorNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvBoundReleaseExtractorNode") {}

MediaNodeKind MediaAvBoundReleaseExtractorNode::staticKind() noexcept
{
    return MediaNodeKind::AvBoundReleaseExtractor;
}

::media::Status MediaAvBoundReleaseExtractorNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "video"), {}},
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "audio"), {}}};
    auto transaction = MediaAtomicOutputTransaction::acquire(
        "A/V bound release extractor", batches);
    if (!transaction) return ::media::Status::failure(transaction.error());
    if (!transaction.value()) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "A/V bound release extractor output validation would block"));
    }
    transaction.value().reset();
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaAvBoundReleaseExtractorNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAvBoundReleaseExtractorNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaAvBoundReleaseExtractorNode::resetState() noexcept
{
    m_pending.reset();
    m_stagedVideo.clear();
    m_stagedAudio.clear();
    m_releaseStaged = false;
}

::media::Status MediaAvBoundReleaseExtractorNode::stageRelease(
    const MediaAvStartupReleaseBuffer& release)
{
    m_stagedVideo.clear();
    m_stagedVideo.reserve(release.video().size());
    for (const auto& unit : release.video()) {
        if (!unit.media) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor rejects null video units"));
        }
        m_stagedVideo.push_back(unit.media);
    }

    m_stagedAudio.clear();
    m_stagedAudio.reserve(release.audio().size());
    for (const auto& unit : release.audio()) {
        auto staged = MediaAvReleasedAudioBuffer::create(
            unit.media, unit.trimLeadingSamples, release.audioOrigin());
        if (!staged) {
            m_stagedVideo.clear();
            m_stagedAudio.clear();
            return ::media::Status::failure(staged.error());
        }
        m_stagedAudio.push_back(std::move(staged.value()));
    }
    m_releaseStaged = true;
    return ::media::Status::success();
}

::media::Status MediaAvBoundReleaseExtractorNode::commit(
    MediaGraphExecutionContext& context)
{
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "video"), m_stagedVideo},
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "audio"), m_stagedAudio}};
    auto transaction = MediaAtomicOutputTransaction::acquire(
        "A/V bound release extractor", batches);
    if (!transaction) return ::media::Status::failure(transaction.error());
    if (!transaction.value()) {
        return ::media::Status::failure(::media::ErrorInfo::wouldBlock(
            "A/V bound release extractor outputs are full"));
    }
    return transaction.value()->commit();
}

::media::Result<MediaNodeProcessResult>
MediaAvBoundReleaseExtractorNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_pending) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "in"),
            "A/V bound release extractor", "in");
        if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
        if (!input.value()) return processWaiting();
        m_pending = std::move(*input.value());
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            m_pending.get())) {
        if (control->controlKind() == MediaControlBufferKind::Unknown) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V bound release extractor rejects unknown control"));
        }
        const std::array<MediaAtomicOutputBatch, 2> batches{
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "video"),
                std::span(&m_pending, 1)},
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "audio"),
                std::span(&m_pending, 1)}};
        auto transaction = MediaAtomicOutputTransaction::acquire(
            "A/V bound release extractor", batches);
        if (!transaction) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                transaction.error());
        }
        if (!transaction.value()) return processWaiting();
        if (auto committed = transaction.value()->commit(); !committed) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                committed.error());
        }
        const bool finished = control->controlKind() == MediaControlBufferKind::Eof ||
                              control->controlKind() == MediaControlBufferKind::Abort;
        m_pending.reset();
        return finished ? processFinished() : processProgress();
    }
    const auto* release = dynamic_cast<const MediaAvStartupReleaseBuffer*>(m_pending.get());
    if (!release) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor requires a startup release"));
    }
    if (!m_releaseStaged) {
        if (auto status = stageRelease(*release); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }
    auto committed = commit(context);
    if (!committed) {
        if (committed.error().code == ::media::ErrorCode::WouldBlock) {
            return processWaiting();
        }
        return ::media::Result<MediaNodeProcessResult>::failure(committed.error());
    }
    m_pending.reset();
    m_stagedVideo.clear();
    m_stagedAudio.clear();
    m_releaseStaged = false;
    return processProgress();
}

} // namespace media::ffmpeg::graph
