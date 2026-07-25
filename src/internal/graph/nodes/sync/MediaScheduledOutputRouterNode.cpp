#include "internal/graph/nodes/sync/MediaScheduledOutputRouterNode.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"

#include <array>
#include <span>

namespace media::ffmpeg::graph {

MediaScheduledOutputRouterNode::MediaScheduledOutputRouterNode(
    MediaNodeId nodeId)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaScheduledOutputRouterNode")
{
}

MediaNodeKind MediaScheduledOutputRouterNode::staticKind() noexcept
{
    return MediaNodeKind::ScheduledOutputRouter;
}

::media::Status MediaScheduledOutputRouterNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto mode = requiredNodeOption(
        nodeOptions(context), "MediaScheduledOutputRouterNode",
        "scheduled_output_router.mode");
    if (!mode) return ::media::Status::failure(mode.error());
    if (mode.value() == "split_av") m_mode = Mode::SplitAv;
    else if (mode.value() == "serialized_av") m_mode = Mode::SerializedAv;
    else return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Scheduled output router mode must be split_av or serialized_av"));
    auto configured = configureTopology(context);
    return configured ? FFmpegNodeRuntime::start(context) : configured;
}

::media::Status MediaScheduledOutputRouterNode::configureTopology(
    MediaGraphExecutionContext& context) const
{
    if (context.inputChannels(nodeId()).size() != 1 ||
        !context.findInputChannel(nodeId(), "scheduled")) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled output router requires exactly one scheduled input"));
    }
    auto outputs = context.outputChannels(nodeId());
    if (m_mode == Mode::SerializedAv) {
        MediaChannel* serialized = context.findOutputChannel(
            nodeId(), "serialized");
        if (outputs.size() != 1 || !serialized) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Serialized scheduled output router requires one serialized output"));
        }
        return ::media::Status::success();
    }
    MediaChannel* video = context.findOutputChannel(nodeId(), "video");
    MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
    if (outputs.size() != 2 || !video || !audio || video == audio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled output router requires one video and one audio output"));
    }
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{video, {}},
        MediaAtomicOutputBatch{audio, {}}};
    auto transaction = MediaAtomicOutputTransaction::acquire(
        "Scheduled output router", batches);
    return transaction ? ::media::Status::success()
                       : ::media::Status::failure(transaction.error());
}

::media::Result<MediaNodeProcessResult>
MediaScheduledOutputRouterNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (!m_pending) {
        auto input = tryPopInputOptional(context, "scheduled");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (!input.value()) {
            MediaChannel* scheduled = context.findInputChannel(
                nodeId(), "scheduled");
            if (scheduled && scheduled->closed()) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Scheduled output router input closed without typed terminal"));
            }
            return processWaiting();
        }
        m_pending = std::move(*input.value());
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            m_pending.get())) {
        return routeControl(context, control->controlKind());
    }
    return routeScheduledUnit(context);
}

::media::Result<MediaNodeProcessResult>
MediaScheduledOutputRouterNode::routeScheduledUnit(
    MediaGraphExecutionContext& context)
{
    const auto* unit = dynamic_cast<const MediaScheduledAccessUnit*>(
        m_pending.get());
    if (!unit) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled output router requires a typed scheduled unit"));
    }
    const char* port = m_mode == Mode::SerializedAv ? "serialized" : nullptr;
    MediaStreamKind expectedStream = MediaStreamKind::Unknown;
    switch (unit->stream()) {
    case MediaScheduledStream::Video:
        if (m_mode == Mode::SplitAv) port = "video";
        expectedStream = MediaStreamKind::Video;
        break;
    case MediaScheduledStream::Audio:
        if (m_mode == Mode::SplitAv) port = "audio";
        expectedStream = MediaStreamKind::Audio;
        break;
    default:
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled output router rejects an invalid stream discriminator"));
    }
    if (m_pending->streamKind() != expectedStream || !unit->media() ||
        unit->media()->streamKind() != expectedStream) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled output router discriminator does not match its payload"));
    }
    MediaChannel* output = context.findOutputChannel(nodeId(), port);
    if (!output) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled output router output is missing"));
    }
    switch (output->pushOutcome(m_pending)) {
    case MediaQueuePushOutcome::Accepted:
        m_pending.reset();
        return processProgress();
    case MediaQueuePushOutcome::WouldBlock:
        return processWaiting();
    case MediaQueuePushOutcome::Closed:
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "Scheduled output router output is closed"));
    case MediaQueuePushOutcome::Aborted:
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError(
                "Scheduled output router output is aborted"));
    case MediaQueuePushOutcome::Dropped:
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError(
                "Scheduled output router output violated its blocking policy"));
    }
    return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::internalError(
            "Scheduled output router observed an unknown push outcome"));
}

::media::Result<MediaNodeProcessResult>
MediaScheduledOutputRouterNode::routeControl(
    MediaGraphExecutionContext& context,
    MediaControlBufferKind kind)
{
    if (kind != MediaControlBufferKind::Eof &&
        kind != MediaControlBufferKind::Flush &&
        kind != MediaControlBufferKind::Abort) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled output router rejects an unknown control kind"));
    }
    if (m_mode == Mode::SerializedAv) {
        MediaChannel* output = context.findOutputChannel(nodeId(), "serialized");
        switch (output->pushOutcome(m_pending)) {
        case MediaQueuePushOutcome::Accepted:
            m_pending.reset();
            return processFinished();
        case MediaQueuePushOutcome::WouldBlock:
            return processWaiting();
        case MediaQueuePushOutcome::Closed:
        case MediaQueuePushOutcome::Aborted:
        case MediaQueuePushOutcome::Dropped:
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::cancelled(
                    "Serialized scheduled output router terminal was rejected"));
        }
    }
    MediaChannel* video = context.findOutputChannel(nodeId(), "video");
    MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{video, std::span(&m_pending, 1)},
        MediaAtomicOutputBatch{audio, std::span(&m_pending, 1)}};
    auto transaction = MediaAtomicOutputTransaction::acquire(
        "Scheduled output router", batches);
    if (!transaction) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            transaction.error());
    }
    if (!transaction.value()) return processWaiting();
    if (auto committed = transaction.value()->commit(); !committed) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            committed.error());
    }
    m_pending.reset();
    return processFinished();
}

::media::Status MediaScheduledOutputRouterNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaScheduledOutputRouterNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaScheduledOutputRouterNode::resetState() noexcept
{
    m_pending.reset();
}

} // namespace media::ffmpeg::graph
