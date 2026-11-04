#pragma once

#include "application/realtime/MediaRealtimeOutputPublisher.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaVideoOutputReadyCollector.h"

namespace media::ffmpeg::graph {

// Control-thread state only. The runtime owns all media and protocol resources.
class MediaRealtimeFixedOutputLifecycle final {
public:
    MediaRealtimeFixedOutputLifecycle(std::uint64_t id, std::string path,
        MediaRealtimeVideoRunControl& control, const MediaRealtimeVideoRunObserver& observer)
        : m_snapshot{id, MediaRealtimeOutputState::Preparing,
            MediaRealtimeOutputFailureStage::Preparation, {}, std::move(path), std::nullopt},
          m_control(control), m_observer(observer) {}

    void bindIdentity(std::uint64_t id) noexcept { m_snapshot.outputId = id; }
    bool running() const noexcept { return m_snapshot.state == MediaRealtimeOutputState::Running; }

    ::media::Status start(MediaGraphRuntime& runtime, std::chrono::milliseconds timeout)
    {
        auto authority = runtime.protocolOutputAuthority();
        if (!authority) return missingAuthority();
        auto now = authority->now();
        if (!now) return ::media::Status::failure(now.error());
        auto duration = MediaRunningTime::checkedFromTicks(timeout.count(), 1, 1000);
        if (!duration) return ::media::Status::failure(duration.error());
        auto deadline = now.value().checkedAdd(duration.value());
        if (!deadline) return ::media::Status::failure(deadline.error());
        m_startedAt = now.value();
        m_deadline = deadline.value();
        m_snapshot.state = MediaRealtimeOutputState::WaitingForRandomAccess;
        m_snapshot.stage = MediaRealtimeOutputFailureStage::Publication;
        return publish();
    }

    ::media::Status poll(MediaGraphRuntime& runtime)
    {
        if (m_snapshot.state == MediaRealtimeOutputState::Running) return ::media::Status::success();
        if (!m_startedAt || !m_deadline) return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("fixed output lifecycle has not started"));
        auto authority = runtime.protocolOutputAuthority();
        if (!authority) return missingAuthority();
        const auto evidence = collectVideoOutputReadyEvidence(runtime.scheduler(), runtime.context());
        const auto activation = authority->currentActivation();
        if (evidence && activation && evidence->generation == activation.value().generation &&
            evidence->submittedAtNanoseconds >= m_startedAt->nanoseconds() &&
            evidence->submittedAtNanoseconds < m_deadline->nanoseconds()) {
            m_snapshot.state = MediaRealtimeOutputState::Running;
            m_snapshot.stage = MediaRealtimeOutputFailureStage::Runtime;
            return publish();
        }
        const auto now = authority->now();
        if (!now) return ::media::Status::failure(now.error());
        if (now.value() >= *m_deadline) return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("fixed output readiness was not observed within the operational startup deadline"));
        return ::media::Status::success();
    }

    // Called only after the entire runtime scope has been destroyed.
    ::media::Status finish(const MediaRealtimeVideoRunOutcome& outcome)
    {
        if (!m_snapshot.outputId) return ::media::Status::success();
        auto notification = ::media::Status::success();
        std::exception_ptr preparationFailure;
        try {
        if (!outcome.status) {
            m_snapshot.state = MediaRealtimeOutputState::Failed;
            m_snapshot.error = outcome.status.error();
            m_snapshot.detail = outcome.status.error().message;
            switch (outcome.stage) {
            case MediaRealtimeVideoRunStage::RuntimeProgress:
                m_snapshot.stage = MediaRealtimeOutputFailureStage::Runtime; break;
            case MediaRealtimeVideoRunStage::RuntimeCompletion:
            case MediaRealtimeVideoRunStage::Completed:
                m_snapshot.stage = MediaRealtimeOutputFailureStage::Release; break;
            default: m_snapshot.stage = MediaRealtimeOutputFailureStage::Publication; break;
            }
            notification = publish();
        }
        } catch (...) { preparationFailure = std::current_exception(); }
        m_snapshot.state = MediaRealtimeOutputState::Retired;
        auto retired = publish();
        if (preparationFailure) std::rethrow_exception(preparationFailure);
        return !notification ? notification : retired;
    }

private:
    static ::media::Status missingAuthority()
    {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "fixed output has no protocol clock authority"));
    }
    ::media::Status publish()
    {
        try {
            MediaRealtimeOutputPublisher::publish(m_control, m_observer, m_snapshot);
            return ::media::Status::success();
        } catch (const std::bad_alloc&) {
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
                "fixed output state publication allocation failed"));
        } catch (...) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "fixed output state observer failed"));
        }
    }
    MediaRealtimeOutputSnapshot m_snapshot;
    MediaRealtimeVideoRunControl& m_control;
    const MediaRealtimeVideoRunObserver& m_observer;
    std::optional<MediaRunningTime> m_startedAt;
    std::optional<MediaRunningTime> m_deadline;
};

} // namespace media::ffmpeg::graph
