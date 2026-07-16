#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/startup/MediaInitialClockAcquisitionDeadline.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;

class MediaInitialLockedPacketGateNode final : public FFmpegNodeRuntime {
public:
    explicit MediaInitialLockedPacketGateNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status acceptClock(const MediaBufferRef& buffer);
    ::media::Status bufferPacket(MediaBufferRef buffer);
    ::media::Status emitValidatedPacket(MediaGraphExecutionContext& context,
                                        MediaBufferRef buffer);
    void resetState() noexcept;

    std::deque<MediaBufferRef> m_acquiringPackets;
    std::optional<std::uint64_t> m_lockedGeneration;
    std::optional<MediaAvSyncGroupKey> m_syncGroupKey;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    std::optional<MediaInitialClockAcquisitionDeadline> m_acquisitionDeadline;
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    std::size_t m_acquiringCapacity = 0;
    bool m_configured = false;
};

} // namespace media::ffmpeg::graph
