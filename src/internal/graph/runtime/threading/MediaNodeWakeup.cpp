#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <algorithm>
#include <limits>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace media::ffmpeg::graph {
namespace {

#if defined(_WIN32)
::media::ErrorInfo waitFailure(const char* operation, unsigned long code)
{
    return ::media::ErrorInfo::ioFailure(
        std::string("MediaNodeWakeup ") + operation +
        " failed with Windows error " + std::to_string(code));
}
#endif

} // namespace

MediaNodeWakeup::MediaNodeWakeup() noexcept
{
#if defined(_WIN32)
    m_notificationEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_notificationEvent) {
        m_platformError = GetLastError();
        return;
    }
    m_deadlineTimer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);
    if (!m_deadlineTimer) m_platformError = GetLastError();
#endif
}

MediaNodeWakeup::~MediaNodeWakeup()
{
#if defined(_WIN32)
    if (m_deadlineTimer) CloseHandle(m_deadlineTimer);
    if (m_notificationEvent) CloseHandle(m_notificationEvent);
#endif
}

MediaNodeWakeup::Sequence MediaNodeWakeup::sequence() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sequence;
}

void MediaNodeWakeup::notify() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_sequence;
    if (m_activeWaitPolicy ==
        MediaNodeDeadlineWakePolicy::DeadlineOrCancellation) {
        return;
    }
#if defined(_WIN32)
    if (m_platformError == 0 &&
        !SetEvent(static_cast<HANDLE>(m_notificationEvent))) {
        m_platformError = GetLastError();
    }
#else
    m_condition.notify_one();
#endif
}

::media::Result<MediaNodeWakeup::WaitOutcome> MediaNodeWakeup::wait(
    Sequence observedSequence,
    MediaNodeDeadlineWakePolicy wakePolicy,
    std::optional<std::chrono::nanoseconds> timeout)
{
#if defined(_WIN32)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_platformError != 0) {
            return ::media::Result<WaitOutcome>::failure(
                waitFailure("initialization", m_platformError));
        }
        if (m_interrupted) {
            return ::media::Result<WaitOutcome>::success(
                WaitOutcome::Interrupted);
        }
        if (wakePolicy == MediaNodeDeadlineWakePolicy::InputOrDeadline &&
            m_sequence != observedSequence) {
            return ::media::Result<WaitOutcome>::success(
                WaitOutcome::Notified);
        }
        m_activeWaitPolicy = wakePolicy;
    }
    if (timeout && timeout->count() <= 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeWaitPolicy.reset();
        return ::media::Result<WaitOutcome>::success(
            WaitOutcome::Deadline);
    }

    HANDLE handles[2]{static_cast<HANDLE>(m_notificationEvent), nullptr};
    DWORD handleCount = 1;
    if (timeout) {
        const std::int64_t nanoseconds = timeout->count();
        const std::int64_t hundredNanoseconds =
            nanoseconds / 100 + (nanoseconds % 100 == 0 ? 0 : 1);
        LARGE_INTEGER dueTime{};
        dueTime.QuadPart = -(std::max)(std::int64_t{1}, hundredNanoseconds);
        if (!SetWaitableTimerEx(
                static_cast<HANDLE>(m_deadlineTimer), &dueTime, 0,
                nullptr, nullptr, nullptr, 0)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_activeWaitPolicy.reset();
            return ::media::Result<WaitOutcome>::failure(
                waitFailure("deadline arm", GetLastError()));
        }
        handles[1] = static_cast<HANDLE>(m_deadlineTimer);
        handleCount = 2;
    }

    const DWORD waited = WaitForMultipleObjects(
        handleCount, handles, FALSE, INFINITE);
    if (timeout && waited == WAIT_OBJECT_0) {
        (void)CancelWaitableTimer(static_cast<HANDLE>(m_deadlineTimer));
    }
    if (waited == WAIT_FAILED) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeWaitPolicy.reset();
        return ::media::Result<WaitOutcome>::failure(
            waitFailure("wait", GetLastError()));
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeWaitPolicy.reset();
    if (m_platformError != 0) {
        return ::media::Result<WaitOutcome>::failure(
            waitFailure("notification", m_platformError));
    }
    if (m_interrupted) {
        return ::media::Result<WaitOutcome>::success(
            WaitOutcome::Interrupted);
    }
    if (m_sequence != observedSequence || waited == WAIT_OBJECT_0) {
        return ::media::Result<WaitOutcome>::success(
            WaitOutcome::Notified);
    }
    return ::media::Result<WaitOutcome>::success(
        WaitOutcome::Deadline);
#else
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto changedPredicate = [&] {
        return m_interrupted ||
            (wakePolicy == MediaNodeDeadlineWakePolicy::InputOrDeadline &&
             m_sequence != observedSequence);
    };
    m_activeWaitPolicy = wakePolicy;
    bool changed = true;
    if (timeout) {
        changed = m_condition.wait_for(lock, *timeout, changedPredicate);
    } else {
        m_condition.wait(lock, changedPredicate);
    }
    m_activeWaitPolicy.reset();
    if (m_interrupted) {
        return ::media::Result<WaitOutcome>::success(
            WaitOutcome::Interrupted);
    }
    return ::media::Result<WaitOutcome>::success(
        changed ? WaitOutcome::Notified : WaitOutcome::Deadline);
#endif
}

void MediaNodeWakeup::interrupt() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interrupted = true;
    ++m_sequence;
#if defined(_WIN32)
    if (m_platformError == 0 &&
        !SetEvent(static_cast<HANDLE>(m_notificationEvent))) {
        m_platformError = GetLastError();
    }
#else
    m_condition.notify_all();
#endif
}

void MediaNodeWakeup::reset() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interrupted = false;
#if defined(_WIN32)
    if (m_notificationEvent && !ResetEvent(
            static_cast<HANDLE>(m_notificationEvent))) {
        m_platformError = GetLastError();
    }
    if (m_deadlineTimer) {
        (void)CancelWaitableTimer(static_cast<HANDLE>(m_deadlineTimer));
    }
#endif
}

} // namespace media::ffmpeg::graph
