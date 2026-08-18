#pragma once

#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaRealtimeVideoRunPolicy final {
public:
    static ::media::Result<MediaRealtimeVideoRunPolicy> create(
        std::chrono::milliseconds progressTimeout,
        std::chrono::milliseconds firstOutputTimeout,
        std::chrono::milliseconds pollInterval,
        std::optional<std::chrono::milliseconds> maximumDuration);

    ::media::Status validate() const;
    std::chrono::milliseconds progressTimeout() const noexcept;
    std::chrono::milliseconds firstOutputTimeout() const noexcept;
    std::chrono::milliseconds pollInterval() const noexcept;
    std::optional<std::chrono::milliseconds> maximumDuration() const noexcept;

private:
    MediaRealtimeVideoRunPolicy(
        std::chrono::milliseconds progressTimeout,
        std::chrono::milliseconds firstOutputTimeout,
        std::chrono::milliseconds pollInterval,
        std::optional<std::chrono::milliseconds> maximumDuration) noexcept;

    std::chrono::milliseconds m_progressTimeout;
    std::chrono::milliseconds m_firstOutputTimeout;
    std::chrono::milliseconds m_pollInterval;
    std::optional<std::chrono::milliseconds> m_maximumDuration;
};

enum class MediaRealtimeVideoRunStage {
    PolicyValidation,
    StopRequested,
    Preflight,
    ExecutableGraphBuild,
    PreparedNotification,
    RuntimeCompile,
    RuntimeNodeRegistration,
    RuntimeStart,
    RuntimeProgress,
    RuntimeCompletion,
    Completed
};

enum class MediaRealtimeVideoRunEndReason {
    NotStarted,
    CallerStop,
    SourceCompleted,
    MaximumDuration,
    FirstOutputTimeout,
    ProgressTimeout,
    WorkerFailure,
    RuntimeStopped,
    Failure
};

struct MediaRealtimeVideoRunFailureSignal final {
    ::media::ErrorCode errorCode = ::media::ErrorCode::None;
    int nativeCode = 0;
    MediaRealtimeVideoRunStage stage =
        MediaRealtimeVideoRunStage::PolicyValidation;
    MediaRealtimeVideoRunEndReason endReason =
        MediaRealtimeVideoRunEndReason::NotStarted;
};

class MediaRealtimeVideoRunControl final {
public:
    MediaRealtimeVideoRunControl() = default;
    MediaRealtimeVideoRunControl(const MediaRealtimeVideoRunControl&) = delete;
    MediaRealtimeVideoRunControl& operator=(
        const MediaRealtimeVideoRunControl&) = delete;

    void requestStop() noexcept;
    bool stopRequested() const noexcept;
    bool waitForStop(std::chrono::milliseconds timeout);
    MediaRealtimeVideoRunStage activeStage() const noexcept;
    std::optional<MediaRealtimeVideoRunFailureSignal>
        firstFailureSignal() const noexcept;

private:
    friend class MediaRealtimeVideoRunController;

    enum class State : std::uint8_t {
        Ready,
        RuntimeStartClaimed,
        StopRequested
    };

    bool tryClaimRuntimeStart() noexcept;
    void beginRunTracking() noexcept;
    void setActiveStage(MediaRealtimeVideoRunStage stage) noexcept;
    void recordFirstFailureSignal(
        const ::media::ErrorInfo& error,
        MediaRealtimeVideoRunStage stage,
        MediaRealtimeVideoRunEndReason endReason) noexcept;

    std::atomic<State> m_state{ State::Ready };
    std::atomic<MediaRealtimeVideoRunStage> m_activeStage{
        MediaRealtimeVideoRunStage::PolicyValidation };
    MediaRealtimeVideoRunFailureSignal m_firstFailureSignal;
    std::atomic_bool m_hasFirstFailureSignal{ false };
    std::mutex m_waitMutex;
    std::condition_variable m_waitCondition;
};

enum class MediaRealtimeVideoOutputDescriptionKind {
    None,
    SessionDescriptionProtocol
};

struct MediaRealtimeVideoOutputDescription final {
    MediaRealtimeVideoOutputDescriptionKind kind =
        MediaRealtimeVideoOutputDescriptionKind::None;
    std::string path;
};

struct MediaRealtimeVideoPreparedAudioOutput final {
    std::string codecName;
    int sampleRate = 0;
    int channels = 0;
    int accessUnitSamples = 0;
    std::string encoderName;
    std::optional<int> bitrateKbps;
};

struct MediaRealtimeVideoPreparedAudioReport final {
    MediaBranchMode branchMode = MediaBranchMode::Drop;
    std::string reason;
    std::optional<MediaRealtimeVideoPreparedAudioOutput> resolvedOutput;
};

struct MediaRealtimeVideoPreparedReport final {
    RealtimeInputType inputType = RealtimeInputType::Url;
    RealtimeInputStreamLayout inputLayout =
        RealtimeInputStreamLayout::SessionDescribed;
    RealtimeOutputStreamLayout outputLayout =
        RealtimeOutputStreamLayout::SeparateStreams;
    MediaOutputTransportKind outputTransport =
        MediaOutputTransportKind::UdpDatagrams;
    MediaTranscodeStreamSet streamSet = MediaTranscodeStreamSet::AudioVideo;
    std::string selectedChain;
    int selectedScore = 0;
    std::string decoderName;
    bool filterActive = false;
    std::string filterName;
    std::string encoderName;
    std::string outputCodecName;
    MediaHardwareDeviceKind hardwareDeviceKind =
        MediaHardwareDeviceKind::Unknown;
    bool zeroCopyPlanned = false;
    std::optional<MediaRealtimeVideoPreparedAudioReport> audio;
    MediaRealtimeVideoOutputDescription outputDescription;
};

struct MediaRealtimeVideoRunObserver final {
    std::function<void(const MediaRealtimeVideoPreparedReport&)> prepared;
    std::function<void(const MediaGraphRuntimeReport&)> progress;
};

struct MediaRealtimeVideoRunOutcome final {
    ::media::Status status;
    MediaRealtimeVideoRunStage stage;
    MediaRealtimeVideoRunEndReason endReason;
    std::optional<MediaGraphRuntimeReport> failureReport;
    std::optional<MediaGraphRuntimeReport> finalReport;
};

class MediaRealtimeVideoRunController final {
public:
    static MediaRealtimeVideoRunOutcome run(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeVideoRunPolicy& policy,
        MediaRealtimeVideoRunControl& control,
        const MediaRealtimeVideoRunObserver& observer);

private:
    MediaRealtimeVideoRunController() = default;
};

} // namespace media::ffmpeg::graph
