#include "internal/graph/runtime/threading/MediaLinuxTimerSlackScope.h"
#if defined(__linux__)
#include "internal/graph/diagnostics/MediaNodeWaitObservation.h"
#include <cerrno>
#include <cstdio>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace media::ffmpeg::graph {
MediaLinuxTimerSlackScope::MediaLinuxTimerSlackScope(MediaNodeWaitObservation* observation) noexcept
    : m_observation(observation) {}

std::optional<MediaLinuxTimerSlackScope::Failure> MediaLinuxTimerSlackScope::enter() noexcept
{
    const long previous = syscall(SYS_prctl, PR_GET_TIMERSLACK, 0UL, 0UL, 0UL, 0UL);
    const int readError = previous < 0 ? errno : 0;
    if (m_observation) {
        if (previous >= 0) m_observation->linuxTimerSlackNs = previous;
        else m_observation->linuxTimerSlackError = readError;
    }
    if (previous < 0) return Failure{Operation::Read, readError};
    // Zero would restore the inherited default, not the observed value.
    // Reject before mutation if an exact nonzero restoration is unavailable.
    if (previous == 0) return Failure{Operation::Read, EINVAL};
    m_previous = static_cast<unsigned long>(previous);
    if (m_observation) m_observation->linuxSlackSetBefore = mediaSteadyClockNowNs();
    // API minimum nonzero slack reduces timer coalescing; it is not a deadline guarantee.
    const long changed = syscall(SYS_prctl, PR_SET_TIMERSLACK, 1UL, 0UL, 0UL, 0UL);
    const int setError = changed < 0 ? errno : 0;
    if (m_observation) {
        m_observation->linuxSlackSetAfter = mediaSteadyClockNowNs();
        if (changed < 0) m_observation->linuxSlackSetError = setError;
    }
    if (changed < 0) return Failure{Operation::Set, setError};
    m_active = true;
    return std::nullopt;
}

std::optional<MediaLinuxTimerSlackScope::Failure> MediaLinuxTimerSlackScope::restore() noexcept
{
    if (!m_active) return std::nullopt;
    if (m_observation) m_observation->linuxSlackRestoreBefore = mediaSteadyClockNowNs();
    const long restored = syscall(SYS_prctl, PR_SET_TIMERSLACK, m_previous, 0UL, 0UL, 0UL);
    const int restoreError = restored < 0 ? errno : 0;
    if (m_observation) {
        m_observation->linuxSlackRestoreAfter = mediaSteadyClockNowNs();
        if (restored < 0) m_observation->linuxSlackRestoreError = restoreError;
    }
    if (restored < 0) return Failure{Operation::Restore, restoreError};
    m_active = false;
    return std::nullopt;
}

MediaLinuxTimerSlackScope::~MediaLinuxTimerSlackScope() noexcept
{
    if (!m_active) return;
    const long restored = syscall(SYS_prctl, PR_SET_TIMERSLACK, m_previous, 0UL, 0UL, 0UL);
    if (restored < 0) {
        const int error = errno;
        // No allocation or throwing logger in the last-resort unwind path.
        std::fprintf(stderr, "MediaLinuxTimerSlackScope emergency restore failed: errno=%d previous_ns=%lu\n",
            error, m_previous);
    }
}
} // namespace media::ffmpeg::graph
#endif
