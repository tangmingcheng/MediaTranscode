#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvSyncError.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/startup/MediaInitialClockAcquisitionDeadline.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;

class MediaLockedPacketGateNode final : public FFmpegNodeRuntime {
public:
    explicit MediaLockedPacketGateNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Result<MediaLockedPacketGateDisposition> acceptClock(
        const MediaBufferRef& buffer);
    ::media::Result<MediaLockedPacketGateDisposition>
    classifyLockedGeneration(std::uint64_t generation,
                             bool mayRequestReacquisition);
    ::media::Result<MediaLockedPacketGateDisposition> classifyPacket(
        const MediaBufferRef& buffer);
    ::media::Result<std::uint64_t> packetGeneration(
        const MediaBufferRef& buffer) const;
    ::media::Status processPacket(MediaGraphExecutionContext& context,
                                  MediaBufferRef buffer);
    ::media::Status retainPendingPacket(MediaBufferRef buffer);
    void resetState() noexcept;

    std::optional<std::uint64_t> m_lockedGeneration;
    std::optional<MediaAvSyncGroupKey> m_syncGroupKey;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    std::optional<MediaInitialClockAcquisitionDeadline> m_acquisitionDeadline;
    MediaBufferRef m_pendingPacket;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    bool m_configured = false;
};

} // namespace media::ffmpeg::graph
