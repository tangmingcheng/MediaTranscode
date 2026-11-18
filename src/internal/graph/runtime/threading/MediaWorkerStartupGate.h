#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <memory>
#include <span>
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "media_transcode/Result.h"
namespace media::ffmpeg::graph {
class MediaGraphWorker;
class MediaGraphExecutionContext;
class MediaGraphScheduler;
class MediaGraphWorkerFailureRecorder;
class MediaGraphWorkerFailureSupervisor;
// One gate per execution scope; no callback or join executes while its lock is held.
class MediaWorkerStartupGate final {
public:
    MediaWorkerStartupGate() = default;
    // The borrowed cleanup context outlives this guard. Startup rollback cannot
    // depend on allocating a diagnostic, including during stack unwinding.
    class Transaction final {
    public:
        Transaction(void* context, void (*rollback)(void*) noexcept) noexcept
            : m_context(context), m_rollback(rollback) {}
        ~Transaction() { rollback(); }
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        void commit() noexcept { m_rollback = nullptr; }
        void rollback() noexcept {
            if (!m_rollback) return;
            auto* action = m_rollback;
            m_rollback = nullptr;
            action(m_context);
        }
    private:
        void* m_context;
        void (*m_rollback)(void*) noexcept;
    };
    // Call only within a catch handler. Preserves native codes without requiring
    // error text allocation in the failure path.
    static ::media::ErrorInfo currentException() noexcept;
    void reset(std::size_t expected) noexcept { std::lock_guard lock(m_mutex); m_expected = expected; m_ready = 0; m_cancelled = false; m_committed = false; }
    ::media::Status prepare(MediaGraphExecutionContext& context, MediaGraphScheduler& scheduler,
        MediaFixedCapacityStorage<std::unique_ptr<MediaGraphWorker>>& workers,
        MediaGraphWorkerFailureRecorder& failures, MediaGraphWorkerFailureSupervisor& supervisor);
    ::media::Status launch(std::span<const std::unique_ptr<MediaGraphWorker>> workers);
    void ready() noexcept { std::lock_guard lock(m_mutex); ++m_ready; m_changed.notify_all(); }
    bool awaitReady() { std::unique_lock lock(m_mutex); m_changed.wait(lock, [&] { return m_cancelled || m_ready == m_expected; }); return !m_cancelled; }
    bool awaitCommit() { std::unique_lock lock(m_mutex); m_changed.wait(lock, [&] { return m_cancelled || m_committed; }); return m_committed && !m_cancelled; }
    bool commit() noexcept { std::lock_guard lock(m_mutex); if (m_cancelled || m_ready != m_expected) return false; m_committed = true; m_changed.notify_all(); return true; }
    void cancel() noexcept { std::lock_guard lock(m_mutex); m_cancelled = true; m_changed.notify_all(); }
private:
    std::mutex m_mutex;
    std::condition_variable m_changed;
    std::size_t m_expected = 0, m_ready = 0;
    bool m_committed = false, m_cancelled = false;
};
}
