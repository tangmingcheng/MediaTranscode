#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <chrono>
#include <limits>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaScheduledAccessUnit;

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
    void recordEncodedReady(
        const MediaScheduledAccessUnit& unit,
        MediaRunningTime ready) noexcept;
    void emitDiagnostics(const char* stage) noexcept;
    void resetState() noexcept;

    std::shared_ptr<MediaVideoProtocolOutputRuntimeAuthority> m_authority;
    bool m_configured = false;
    bool m_requireKeyFrame = false;
    bool m_startedMedia = false;
    MediaRunningTime m_maximumStartupWait =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_transportLead =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_protocolPreparationLead =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_activationLead =
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
    std::int64_t m_maximumEncodedReadyAfterEmitNanoseconds =
        (std::numeric_limits<std::int64_t>::min)();
    std::int64_t m_worstEncodedReadyNanoseconds = 0;
    std::int64_t m_worstEncodedEmitNanoseconds = 0;
    std::int64_t m_worstEncodedDispatchNanoseconds = 0;
    std::int64_t m_worstEncodedReadyAfterMasterReleaseNanoseconds = 0;
    std::int64_t m_worstEncodedDtsDeltaNanoseconds = 0;
    std::int64_t m_worstEncodedDts = 0;
    std::uint64_t m_worstEncodedSequence = 0;
    bool m_diagnosticsEmitted = false;
};

} // namespace media::ffmpeg::graph
