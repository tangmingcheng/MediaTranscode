#include "internal/graph/runtime/threading/MediaWorkerThreadRegistration.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/threading/MediaWorkerStartupGate.h"
#include <optional>
#include <thread>
#include <system_error>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <avrt.h>
#endif
namespace media::ffmpeg::graph {
namespace {
void logRegistration(const char* stage, MediaNodeId node, std::uint32_t tid, long native) noexcept {
    try {
#ifdef _WIN32
        const auto pid = GetCurrentProcessId();
#else
        const std::uint32_t pid = 0; // This diagnostic is emitted only by AVRT operations.
#endif
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeLifecycle,
            std::string("distribution_registration scope=") + (node ? "worker" : "capability_probe") +
            " stage=" + stage + " node=" + (node ? std::to_string(node.value) : "none") +
            " pid=" + std::to_string(pid) + " tid=" + std::to_string(tid) + " native=" + std::to_string(native));
    } catch (...) {}
}
}
MediaWorkerThreadRegistration::~MediaWorkerThreadRegistration() {
    if (!m_handle) return;
    try { auto status = leave(); if (!status) logRegistration("release_retry_failed", m_node, m_ownerThread, status.error().nativeCode); }
    catch (...) { logRegistration("release_exception", m_node, m_ownerThread, 0); }
}
::media::Status MediaWorkerThreadRegistration::enter(const MediaWorkerExecutionPlan& plan) {
    if (m_handle || !plan.node) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Worker registration requires a valid inactive node identity"));
    m_node = plan.node;
#ifdef _WIN32
    if (plan.platform == MediaWorkerPlatformExecution::WindowsOrdinary && plan.role == MediaWorkerExecutionRole::Ordinary)
        return ::media::Status::success();
    if (plan.platform != MediaWorkerPlatformExecution::WindowsDistribution || plan.role != MediaWorkerExecutionRole::DatagramDistribution)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Worker Windows execution contract mismatch"));
    return enterDistribution();
#else
    if (plan.platform != MediaWorkerPlatformExecution::LinuxNative)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Worker native execution contract mismatch"));
#endif
    return ::media::Status::success();
}
::media::Status MediaWorkerThreadRegistration::enterDistribution() {
#ifdef _WIN32
    m_ownerThread = GetCurrentThreadId();
    DWORD taskIndex = 0;
    m_handle = AvSetMmThreadCharacteristicsW(L"Distribution", &taskIndex);
    const auto error = m_handle ? ERROR_SUCCESS : GetLastError();
    logRegistration("enter", m_node, m_ownerThread, error);
    if (!m_handle) return ::media::Status::failure(::media::ErrorInfo{::media::ErrorCode::HardwareUnavailable, static_cast<int>(error), {}});
#else
    return ::media::Status::failure(::media::ErrorInfo::unsupported("MMCSS is a Windows execution capability"));
#endif
    return ::media::Status::success();
}
::media::Status MediaWorkerThreadRegistration::leave() noexcept {
    if (!m_handle) return ::media::Status::success();
#ifdef _WIN32
    if (GetCurrentThreadId() != m_ownerThread) return ::media::Status::failure(
        ::media::ErrorInfo{::media::ErrorCode::InvalidArgument, 0, {}});
    if (!AvRevertMmThreadCharacteristics(m_handle)) {
        const auto error = GetLastError();
        logRegistration("leave_failed", m_node, m_ownerThread, error);
        return ::media::Status::failure(::media::ErrorInfo{::media::ErrorCode::HardwareUnavailable, static_cast<int>(error), {}});
    }
    m_handle = nullptr;
    logRegistration("leave", m_node, m_ownerThread, 0);
#endif
    return ::media::Status::success();
}
::media::Status probeMediaWindowsDistributionRegistration() {
#ifdef _WIN32
    std::optional<::media::Status> outcome;
    try {
        std::thread thread([&] {
            try {
                MediaWorkerThreadRegistration registration;
                auto entered = registration.enterDistribution();
                outcome = entered ? registration.leave() : entered;
            } catch (...) {
                outcome = ::media::Status::failure(MediaWorkerStartupGate::currentException());
            }
        });
        thread.join();
    } catch (...) {
        return ::media::Status::failure(MediaWorkerStartupGate::currentException());
    }
    if (!outcome) return ::media::Status::failure(::media::ErrorInfo::internalError("MMCSS probe returned no result"));
    return std::move(*outcome);
#else
    return ::media::Status::failure(::media::ErrorInfo::unsupported("MMCSS is a Windows execution capability"));
#endif
}
}
