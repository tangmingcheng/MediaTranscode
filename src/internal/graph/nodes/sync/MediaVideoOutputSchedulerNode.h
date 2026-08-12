#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <chrono>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaVideoOutputSchedulerNode final : public FFmpegNodeRuntime {
public:
    MediaVideoOutputSchedulerNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaVideoProtocolOutputRuntimeAuthority> authority);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    enum class PacketTimingMode {
        PacketDuration,
        PlannedCadence
    };

    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status validateStartupDeadline() const;
    ::media::Result<MediaBufferRef> schedule(MediaBufferRef media);
    ::media::Result<MediaNodeProcessResult> emitPending(
        MediaGraphExecutionContext& context);
    void resetState() noexcept;

    std::shared_ptr<MediaVideoProtocolOutputRuntimeAuthority> m_authority;
    bool m_configured = false;
    bool m_requireKeyFrame = false;
    bool m_startedMedia = false;
    MediaRunningTime m_maximumStartupWait =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_transportLead =
        MediaRunningTime::fromNanoseconds(0);
    std::size_t m_packetCapacity = 0;
    std::uint64_t m_maximumUnitBytes = 0;
    std::uint64_t m_byteCapacity = 0;
    MediaRational m_sourceTimeBase;
    MediaRational m_outputFrameRate;
    MediaRational m_packetTimeBase;
    std::optional<PacketTimingMode> m_packetTimingMode;
    std::uint64_t m_initialGeneration = 0;
    std::chrono::steady_clock::time_point m_startedAt{};
    std::optional<MediaRunningTime> m_pendingDeadline;
    MediaBufferRef m_pendingActivation;
    MediaBufferRef m_pendingScheduled;
    std::optional<MediaRunningTime> m_sourceStart;
    std::optional<MediaRunningTime> m_masterRelease;
    std::optional<MediaRunningTime> m_lastDispatch;
    std::uint64_t m_nextSequence = 1;
};

} // namespace media::ffmpeg::graph
