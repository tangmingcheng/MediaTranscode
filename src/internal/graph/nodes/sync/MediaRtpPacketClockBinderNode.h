#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/runtime/session/MediaRtpPacketClockSession.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketTimestampAligner.h"
#include "internal/graph/sync/startup/MediaInitialClockAcquisitionDeadline.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
class MediaPreparedRtpSourceSession;
struct MediaPreparedRtpPrefix;

class MediaRtpPacketClockBinderNode final : public FFmpegNodeRuntime {
public:
    MediaRtpPacketClockBinderNode(MediaNodeId nodeId, MediaStreamKind stream);
    ~MediaRtpPacketClockBinderNode() override;
    ::media::Status bindPreparedSession(std::shared_ptr<MediaPreparedRtpSourceSession> source);
    ::media::Status start(MediaGraphExecutionContext& context) override;
    std::string_view generationPurgeIdentity() const noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<std::optional<MediaNodeProcessResult>> processControlBeforeOutput(
        MediaGraphExecutionContext& context, bool allowClock, bool& progressed) override;
    ::media::Result<MediaOutputCommitReservation> reserveOutputCommit(const MediaBufferRef& buffer) const override;
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    using PacketDisposition = MediaRtpPacketClockSession::PacketDisposition;
    ::media::Result<bool> retireExpiredCandidates();
    ::media::Result<bool> resumeCandidateBinding();
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status acceptClock(const MediaBufferRef& buffer);
    ::media::Status bufferAcquiring(MediaBufferRef buffer);
    ::media::Status bindPacket(MediaGraphExecutionContext& context,
                               MediaBufferRef buffer);
    ::media::Status finishPacketInput(MediaGraphExecutionContext& context,
                                      MediaBufferRef terminal);
    ::media::Status endGeneration(MediaGraphExecutionContext& context, MediaBufferRef terminal);
    void resetState() noexcept;
    ::media::Status releasePendingPublication(MediaGraphExecutionContext& context);
    ::media::Status emitPacket(MediaGraphExecutionContext& context, MediaBufferRef buffer);

    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_ownerPurge;
    std::optional<MediaAvSyncGroupKey> m_syncGroupKey;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_syncGroup;
    const MediaStreamKind m_streamKind;
    // Keeps preparation reservations until the handed-off binder is destroyed.
    std::shared_ptr<MediaPreparedRtpSourceSession> m_preparedSource;
    std::optional<MediaRtpPacketClockSession> m_clockSession;
    MediaLineageUniquePtr<MediaPreparedRtpPrefix> m_preparedPrefix{
        nullptr, MediaLineageObjectDeleter<MediaPreparedRtpPrefix>{MediaRuntimeLineageAllocation{}}};
    // One first-publication owner, shared by graph-credit and authority waits.
    MediaBufferRef m_pendingPublication;
    MediaBufferRef m_generationEnd;
    std::uint64_t m_initialGeneration = 0;
    bool m_configured = false;
};

} // namespace media::ffmpeg::graph
