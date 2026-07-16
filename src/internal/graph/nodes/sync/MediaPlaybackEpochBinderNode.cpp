#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <string>

namespace media::ffmpeg::graph {

// The binder is the sole runtime owner of activation authority.

MediaPlaybackEpochBinderNode::MediaPlaybackEpochBinderNode(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey groupKey,
    MediaPlaybackEpochActivationCapability capability)
    : m_nodeId(nodeId)
    , m_groupKey(std::move(groupKey))
    , m_capability(std::move(capability))
{
}

MediaNodeKind MediaPlaybackEpochBinderNode::staticKind() noexcept
{
    return MediaNodeKind::PlaybackEpochBinder;
}

MediaNodeId MediaPlaybackEpochBinderNode::nodeId() const noexcept
{
    return m_nodeId;
}

::media::Result<MediaNodeProcessResult>
MediaPlaybackEpochBinderNode::process(MediaGraphExecutionContext& context)
{
    if (!m_pendingRelease) {
        MediaChannel* input = context.findInputChannel(nodeId(), "release");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Playback epoch binder requires a release input"));
        }
        if (!input->tryPop(m_pendingRelease)) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waiting());
        }
    }
    const auto* release = dynamic_cast<const MediaAvStartupReleaseBuffer*>(
        m_pendingRelease.get());
    if (!release || release->groupKey() != m_groupKey) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch binder rejects mismatched startup release"));
    }
    if (auto status = MediaAvStartupReleaseBuffer::validateReleaseKind(
            release->releaseKind()); !status) {
        return ::media::Result<MediaNodeProcessResult>::failure(status.error());
    }
    switch (release->releaseKind()) {
    case MediaAvStartupReleaseKind::InitialAtomicRelease: {
        if (!m_pendingActivatedEvent) {
            auto event = MediaPlaybackEpochActivatedBuffer::create(
                m_groupKey, release->epoch(), release->audioOrigin());
            if (!event) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    event.error());
            }
            m_pendingActivatedEvent = std::move(event).value();
        }
        if (!m_activationCommitted) {
            auto status = m_capability.activateInitial(
                release->epoch(), release->audioOrigin());
            if (!status) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    status.error());
            }
            m_activationCommitted = true;
        }
        break;
    }
    case MediaAvStartupReleaseKind::ActiveEpochPassThrough:
        m_activationCommitted = true;
        m_activatedEventCommitted = true;
        break;
    }

    const auto push = [&](const char* port, const MediaBufferRef& value)
        -> ::media::Result<bool> {
        MediaChannel* output = context.findOutputChannel(nodeId(), port);
        if (!output) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::notInitialized(
                    std::string("Playback epoch binder requires output: ") + port));
        }
        if (output->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Playback epoch binder requires blocking outputs"));
        }
        const auto outcome = output->pushOutcome(value);
        if (outcome == MediaQueuePushOutcome::WouldBlock) {
            return ::media::Result<bool>::success(false);
        }
        if (outcome != MediaQueuePushOutcome::Accepted) {
            return ::media::Result<bool>::failure(
                outcome == MediaQueuePushOutcome::Closed
                    ? ::media::ErrorInfo::cancelled(
                          "Playback epoch binder output is closed")
                    : ::media::ErrorInfo::internalError(
                          "Playback epoch binder output rejected transfer"));
        }
        return ::media::Result<bool>::success(true);
    };
    if (!m_activatedEventCommitted) {
        auto committed = push("activated", m_pendingActivatedEvent);
        if (!committed) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                committed.error());
        }
        if (!committed.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waiting());
        }
        m_activatedEventCommitted = true;
    }
    auto forwarded = push("bound_release", m_pendingRelease);
    if (!forwarded) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            forwarded.error());
    }
    if (!forwarded.value()) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    m_pendingRelease.reset();
    m_pendingActivatedEvent.reset();
    m_activationCommitted = false;
    m_activatedEventCommitted = false;
    return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::progress());
}

::media::Status MediaPlaybackEpochBinderNode::stop(
    MediaGraphExecutionContext& context)
{
    m_pendingRelease.reset();
    m_pendingActivatedEvent.reset();
    m_activationCommitted = false;
    m_activatedEventCommitted = false;
    return MediaRuntimeNode::stop(context);
}

void MediaPlaybackEpochBinderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_pendingRelease.reset();
    m_pendingActivatedEvent.reset();
    m_activationCommitted = false;
    m_activatedEventCommitted = false;
    MediaRuntimeNode::abort(context);
}

const MediaAvSyncGroupKey& MediaPlaybackEpochBinderNode::groupKey() const noexcept
{
    return m_groupKey;
}

::media::Status MediaPlaybackEpochBinderNode::publishInitial(
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin)
{
    return m_capability.activateInitial(epoch, audioOrigin);
}

::media::Status MediaPlaybackEpochBinderNode::publishNext(
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin,
    std::uint64_t completedTransitionSequence)
{
    return m_capability.activateNext(epoch, audioOrigin,
                                     completedTransitionSequence);
}

} // namespace media::ffmpeg::graph
