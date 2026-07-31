#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaDemuxTimestampClockMapper.h"

#include <memory>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvGenerationPurgeTarget;
class MediaAvSyncGroupRuntime;
class MediaDemuxPacketClockBinderState;

class MediaDemuxPacketClockBinderNode final : public FFmpegNodeRuntime {
public:
    MediaDemuxPacketClockBinderNode(
        MediaNodeId nodeId,
        MediaScheduledStream stream,
        MediaRational plannedTimeBase,
        std::shared_ptr<MediaDemuxTimestampClockMapper> mapper,
        std::shared_ptr<MediaAvSyncGroupRuntime> syncGroup);

    static MediaNodeKind staticKind() noexcept;
    std::string_view generationPurgeIdentity() const noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;

    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    bool pendingOutputIsCurrent(
        const MediaBufferRef& buffer) const noexcept override;
    ::media::Result<MediaOutputCommitReservation>
    reserveOutputCommit(const MediaBufferRef& buffer) const override;
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Result<MediaDemuxTimestampOutputCommitEvidence>
    outputCommitEvidence(const MediaBufferRef& buffer) const;
    ::media::Result<MediaBufferRef> timedPacket(
        MediaBufferRef buffer,
        std::uint64_t generation);
    ::media::Result<MediaNodeProcessResult> processPacket(
        MediaGraphExecutionContext& context,
        MediaBufferRef buffer);
    ::media::Result<MediaNodeProcessResult> publishClockState(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> processTerminal(
        MediaGraphExecutionContext& context,
        MediaBufferRef terminal);
    ::media::Status resetLifecycle();

    const MediaScheduledStream m_stream;
    const MediaStreamKind m_streamKind;
    const MediaRational m_plannedTimeBase;
    std::shared_ptr<MediaDemuxTimestampClockMapper> m_mapper;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    std::shared_ptr<MediaDemuxPacketClockBinderState> m_state;
};

} // namespace media::ffmpeg::graph
