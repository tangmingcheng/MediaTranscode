#pragma once

#include "internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketTimestampAligner.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/model/MediaRtpVideoTimingPlan.h"
#include "internal/graph/sync/startup/MediaInitialClockAcquisitionDeadline.h"
#include "internal/graph/runtime/storage/MediaLineageUniquePtr.h"
#include <deque>
#include <optional>
#include <memory_resource>

namespace media::ffmpeg::graph {

struct MediaRtpPacketClockSessionConfig final {
    MediaStreamKind stream;
    MediaTranscodeStreamSet members;
    std::size_t acquiringCapacity;
    MediaRunningTime acquisitionTimeout;
    std::optional<int> videoClockRate;
    std::optional<MediaRtpVideoTimingPlan> videoTiming;
};

class MediaRtpPacketClockBinderNode;
class MediaPreparedRtpSourceSession;

// State and packet transformation shared by preparation and runtime owners.
// Scheduling, publication and generation arbitration remain with the caller.
class MediaRtpPacketClockSession final {
public:
    static ::media::Result<MediaRtpPacketClockSession> create(MediaRtpPacketClockSessionConfig config,
        MediaLineageAllocationScope allocation);
    MediaRtpPacketClockSession(const MediaRtpPacketClockSession&) = delete;
    MediaRtpPacketClockSession& operator=(const MediaRtpPacketClockSession&) = delete;
    MediaRtpPacketClockSession(MediaRtpPacketClockSession&&) = default;
    MediaRtpPacketClockSession& operator=(MediaRtpPacketClockSession&&) = delete;
private:
    friend class MediaRtpPacketClockBinderNode;
    friend class MediaPreparedRtpSourceSession;
    MediaRtpPacketClockSession(MediaRtpPacketClockSessionConfig config,
        MediaInitialClockAcquisitionDeadline deadline, MediaLineageAllocationScope allocation);
    enum class PacketDisposition { Retired, AwaitClock, Bind };
    ::media::Result<PacketDisposition> classifyPacket(const MediaBufferRef& buffer) const;
    ::media::Result<MediaBufferRef> timedPacket(MediaBufferRef buffer, std::uint64_t& extendedTimestamp);
    ::media::Result<MediaBufferRef> bindPacket(MediaBufferRef buffer);
    ::media::Result<MediaBufferRef> finishPacketInput(MediaBufferRef terminal);
    ::media::Status acceptClock(const MediaRtpClockGroupSnapshot& snapshot, MediaTranscodeStreamSet members);
    ::media::Status bufferAcquiring(MediaBufferRef buffer, std::optional<MediaRunningTime> observedAt);
    void invalidateClockProjection() noexcept;
    std::optional<std::uint64_t> m_ingressCutoff;
    MediaRtpPacketClockProjector m_projector;
    MediaRtpPacketTimestampAligner m_timestampAligner;
    std::optional<MediaRtpClockGroupSnapshot> m_lockedSnapshot;
    MediaLineageAllocator<std::byte> m_allocation;
    MediaLineageUniquePtr<std::pmr::deque<MediaBufferRef>> m_acquiringPackets;
    std::optional<MediaInitialClockAcquisitionDeadline> m_acquisitionDeadline;
    MediaBufferRef m_videoLookahead;
    std::optional<std::uint64_t> m_videoLookaheadTimestamp;
    std::optional<std::int64_t> m_lastPositiveVideoDelta;
    MediaBufferRef m_pendingTerminal;
    std::size_t m_acquiringCapacity = 0;
    std::size_t m_acquiringHighWater = 0;
    std::optional<int> m_videoClockRate;
    std::optional<MediaRtpVideoTimingPlan> m_videoTiming;
    bool m_discardingUnboundCandidates = false;
    bool m_keyTraceEmitted = false;
    MediaStreamKind m_streamKind;
    MediaScheduledStream m_scheduledStream;
};

} // namespace media::ffmpeg::graph
