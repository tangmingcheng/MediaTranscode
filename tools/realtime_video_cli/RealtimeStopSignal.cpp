#include "RealtimeStopSignal.h"
#include "application/realtime/MediaRealtimeVideoRunController.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <pthread.h>
#endif

namespace media::ffmpeg::graph::cli {
namespace {

// atomic_flag operations are guaranteed lock-free, including in signal context.
// Static lifetime prevents an in-flight OS callback from touching a dead object.
std::atomic_flag signalRequested = ATOMIC_FLAG_INIT;
std::atomic_flag signalOwner = ATOMIC_FLAG_INIT;

#ifdef _WIN32
BOOL WINAPI handleStop(DWORD event) noexcept
{
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
    signalRequested.test_and_set(std::memory_order_relaxed);
    return TRUE;
}
#else
extern "C" void handleStop(int) noexcept
{
    signalRequested.test_and_set(std::memory_order_relaxed);
}
#endif

class SignalRegistration final {
public:
    SignalRegistration()
    {
        if (signalOwner.test_and_set(std::memory_order_acquire))
            throw std::runtime_error("realtime CLI stop signal already has an owner");
        signalRequested.clear(std::memory_order_relaxed);
        try {
#ifdef _WIN32
            if (!SetConsoleCtrlHandler(handleStop, TRUE))
                throw std::runtime_error("cannot register realtime CLI console stop handler");
            m_registered = true;
#else
            if (sigemptyset(&m_stopSignals) != 0 ||
                sigaddset(&m_stopSignals, SIGINT) != 0 ||
                sigaddset(&m_stopSignals, SIGTERM) != 0)
                throw std::runtime_error("cannot initialize realtime CLI stop signals");
            if (pthread_sigmask(SIG_BLOCK, &m_stopSignals, &m_previousMask) != 0)
                throw std::runtime_error("cannot block realtime CLI stop signals");
            m_maskRegistered = true;
            struct sigaction action {};
            action.sa_handler = handleStop;
            if (sigemptyset(&action.sa_mask) != 0)
                throw std::runtime_error("cannot initialize realtime CLI signal mask");
            if (sigaction(SIGINT, &action, &m_previousInterrupt) != 0)
                throw std::runtime_error("cannot register realtime CLI interrupt handler");
            m_interruptRegistered = true;
            if (sigaction(SIGTERM, &action, &m_previousTerminate) != 0)
                throw std::runtime_error("cannot register realtime CLI termination handler");
            m_terminateRegistered = true;
#endif
        } catch (...) {
            restore();
            signalOwner.clear(std::memory_order_release);
            throw;
        }
    }

    ~SignalRegistration()
    {
        restore();
        signalOwner.clear(std::memory_order_release);
    }

    SignalRegistration(const SignalRegistration&) = delete;
    SignalRegistration& operator=(const SignalRegistration&) = delete;

    // Called only by the bridge. Media workers inherit the creator's blocked mask.
    void enableBridgeSignals()
    {
#ifndef _WIN32
        if (pthread_sigmask(SIG_UNBLOCK, &m_stopSignals, nullptr) != 0)
            throw std::runtime_error("cannot enable realtime CLI bridge signals");
#endif
    }

private:
    void restore() noexcept
    {
#ifdef _WIN32
        if (m_registered && !SetConsoleCtrlHandler(handleStop, FALSE))
            std::fputs("[CLI] cannot unregister console stop handler\n", stderr);
        m_registered = false;
#else
        if (m_terminateRegistered && sigaction(SIGTERM, &m_previousTerminate, nullptr) != 0)
            std::fputs("[CLI] cannot restore termination handler\n", stderr);
        if (m_interruptRegistered && sigaction(SIGINT, &m_previousInterrupt, nullptr) != 0)
            std::fputs("[CLI] cannot restore interrupt handler\n", stderr);
        m_terminateRegistered = false;
        m_interruptRegistered = false;
        if (m_maskRegistered && pthread_sigmask(SIG_SETMASK, &m_previousMask, nullptr) != 0)
            std::fputs("[CLI] cannot restore stop signal mask\n", stderr);
        m_maskRegistered = false;
#endif
    }

#ifdef _WIN32
    bool m_registered = false;
#else
    sigset_t m_stopSignals {};
    sigset_t m_previousMask {};
    bool m_maskRegistered = false;
    struct sigaction m_previousInterrupt {};
    struct sigaction m_previousTerminate {};
    bool m_interruptRegistered = false;
    bool m_terminateRegistered = false;
#endif
};

} // namespace

struct RealtimeStopSignal::State final {
    State(MediaRealtimeVideoRunControl& control,
          std::chrono::milliseconds pollInterval)
        : worker([this, &control, pollInterval](std::stop_token stop) {
            try {
                registration.enableBridgeSignals();
                started.set_value();
            } catch (...) {
                started.set_exception(std::current_exception());
                return;
            }
            std::unique_lock lock(waitMutex);
            while (!stop.stop_requested()) {
                if (signalRequested.test(std::memory_order_relaxed)) {
                    lock.unlock();
                    control.requestStop();
                    return;
                }
                changed.wait_for(lock, stop, pollInterval, [] {
                    return signalRequested.test(std::memory_order_relaxed);
                });
            }
        })
    {
        started.get_future().get();
    }

    SignalRegistration registration;
    std::promise<void> started;
    std::mutex waitMutex;
    std::condition_variable_any changed;
    // Destroyed first: request stop, wake the interruptible wait, then join.
    std::jthread worker;
};

RealtimeStopSignal::RealtimeStopSignal(
    MediaRealtimeVideoRunControl& control, std::chrono::milliseconds pollInterval)
{
    if (pollInterval.count() <= 0)
        throw std::invalid_argument("realtime CLI signal bridge requires a positive poll interval");
    m_state = std::make_unique<State>(control, pollInterval);
}

RealtimeStopSignal::~RealtimeStopSignal() = default;

} // namespace media::ffmpeg::graph::cli
