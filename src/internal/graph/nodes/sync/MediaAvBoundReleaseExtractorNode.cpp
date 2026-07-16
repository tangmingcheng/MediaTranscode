#include "internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h"

#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"

namespace media::ffmpeg::graph {

MediaAvBoundReleaseExtractorNode::MediaAvBoundReleaseExtractorNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvBoundReleaseExtractorNode") {}

MediaNodeKind MediaAvBoundReleaseExtractorNode::staticKind() noexcept
{
    return MediaNodeKind::AvBoundReleaseExtractor;
}

::media::Status MediaAvBoundReleaseExtractorNode::stop(
    MediaGraphExecutionContext& context)
{
    m_pending.reset();
    m_stagedAudio.clear();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAvBoundReleaseExtractorNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_pending.reset();
    m_stagedAudio.clear();
    FFmpegNodeRuntime::abort(context);
}

::media::Result<bool> MediaAvBoundReleaseExtractorNode::preflight(
    MediaGraphExecutionContext& context,
    std::size_t requiredVideoCapacity,
    std::size_t requiredAudioCapacity) const
{
    MediaChannel* video = context.findOutputChannel(nodeId(), "video");
    MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
    if (!video || !audio) {
        return ::media::Result<bool>::failure(::media::ErrorInfo::notInitialized(
            "A/V bound release extractor requires explicit video and audio outputs"));
    }
    if (video->closed() || audio->closed() || video->aborted() || audio->aborted()) {
        return ::media::Result<bool>::failure(::media::ErrorInfo::cancelled(
            "A/V bound release extractor output is closed"));
    }
    if (video->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer ||
        audio->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer) {
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "Atomic A/V release requires blocking output queue policy"));
    }
    const bool videoReady = video->capacity() - video->size() >=
                            requiredVideoCapacity;
    const bool audioReady = audio->capacity() - audio->size() >=
                            requiredAudioCapacity;
    return ::media::Result<bool>::success(videoReady && audioReady);
}

::media::Status MediaAvBoundReleaseExtractorNode::stageAudio(
    const MediaAvStartupReleaseBuffer& release)
{
    m_stagedAudio.clear();
    m_stagedAudio.reserve(release.audio().size());
    for (const auto& unit : release.audio()) {
        auto staged = MediaAvReleasedAudioBuffer::create(
            unit.media, unit.trimLeadingSamples, release.audioOrigin());
        if (!staged) {
            m_stagedAudio.clear();
            return ::media::Status::failure(staged.error());
        }
        m_stagedAudio.push_back(std::move(staged.value()));
    }
    return ::media::Status::success();
}

::media::Status MediaAvBoundReleaseExtractorNode::commit(
    MediaGraphExecutionContext& context,
    const MediaAvStartupReleaseBuffer& release)
{
    MediaChannel* video = context.findOutputChannel(nodeId(), "video");
    MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
    const auto pushVideo = [](MediaChannel& channel,
                              const std::vector<MediaAvReleasedUnit>& units) {
        for (const auto& unit : units) {
            if (channel.pushOutcome(unit.media) != MediaQueuePushOutcome::Accepted) {
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    "A/V bound release extractor commit diverged after preflight"));
            }
        }
        return ::media::Status::success();
    };
    const auto pushAudio = [](MediaChannel& channel,
                              const std::vector<MediaBufferRef>& units) {
        for (const auto& unit : units) {
            if (channel.pushOutcome(unit) != MediaQueuePushOutcome::Accepted) {
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    "A/V bound release extractor commit diverged after preflight"));
            }
        }
        return ::media::Status::success();
    };
    if (auto status = pushVideo(*video, release.video()); !status) return status;
    return pushAudio(*audio, m_stagedAudio);
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
        auto ready = preflight(context, 1, 1);
        if (!ready) {
            return ::media::Result<MediaNodeProcessResult>::failure(ready.error());
        }
        if (!ready.value()) return processWaiting();
        MediaChannel* video = context.findOutputChannel(nodeId(), "video");
        MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
        if (video->pushOutcome(m_pending) != MediaQueuePushOutcome::Accepted ||
            audio->pushOutcome(m_pending) != MediaQueuePushOutcome::Accepted) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError(
                    "A/V control release commit diverged after preflight"));
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
    if (m_stagedAudio.empty() && !release->audio().empty()) {
        if (auto status = stageAudio(*release); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }
    auto ready = preflight(
        context, release->video().size(), m_stagedAudio.size());
    if (!ready) return ::media::Result<MediaNodeProcessResult>::failure(ready.error());
    if (!ready.value()) return processWaiting();
    auto committed = commit(context, *release);
    if (!committed) return ::media::Result<MediaNodeProcessResult>::failure(committed.error());
    m_pending.reset();
    m_stagedAudio.clear();
    return processProgress();
}

} // namespace media::ffmpeg::graph
