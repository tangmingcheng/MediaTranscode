#include "internal/graph/runtime/filesystem/MediaWin32AtomicFileReplacePort.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr unsigned int kMaximumTemporaryNameAttempts = 16;
std::atomic<std::uint64_t> g_temporarySequence{0};

::media::Result<std::wstring> strictUtf8ToWide(std::string_view value)
{
    if (value.empty() || value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return ::media::Result<std::wstring>::failure(
            ::media::ErrorInfo::invalidArgument("Atomic publisher path is empty or too long"));
    }
    if (value.find('\0') != std::string_view::npos) {
        return ::media::Result<std::wstring>::failure(
            ::media::ErrorInfo::invalidArgument("Atomic publisher path contains NUL"));
    }

    const int inputLength = static_cast<int>(value.size());
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), inputLength, nullptr, 0);
    if (required <= 0) {
        return ::media::Result<std::wstring>::failure(::media::ErrorInfo::invalidArgument(
            "Atomic publisher path is not valid UTF-8"));
    }

    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), inputLength,
        converted.data(), required);
    if (written != required) {
        return ::media::Result<std::wstring>::failure(::media::ErrorInfo::ioFailure(
            "Atomic publisher failed to convert UTF-8 path", GetLastError()));
    }
    return ::media::Result<std::wstring>::success(std::move(converted));
}

::media::ErrorInfo win32Failure(std::string operation)
{
    return ::media::ErrorInfo::ioFailure(std::move(operation),
                                         static_cast<int>(GetLastError()));
}

class Win32AtomicFileReplaceTransaction final : public MediaAtomicFileReplaceTransaction {
public:
    Win32AtomicFileReplaceTransaction(std::wstring targetPath,
                                      std::wstring temporaryPath,
                                      HANDLE handle) noexcept
        : m_targetPath(std::move(targetPath)),
          m_temporaryPath(std::move(temporaryPath)),
          m_handle(handle)
    {
    }

    ~Win32AtomicFileReplaceTransaction() override
    {
        closeNoThrow();
        if (!m_temporaryPath.empty()) {
            DeleteFileW(m_temporaryPath.c_str());
        }
    }

    ::media::Status writeAll(std::span<const std::uint8_t> bytes) override
    {
        if (m_phase != Phase::Open || m_handle == INVALID_HANDLE_VALUE) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Atomic file transaction is not writable"));
        }
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto remaining = bytes.size() - offset;
            const DWORD requested = static_cast<DWORD>((std::min)(
                remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
            DWORD written = 0;
            if (!WriteFile(m_handle, bytes.data() + offset, requested, &written, nullptr)) {
                return ::media::Status::failure(win32Failure("Atomic publisher write failed"));
            }
            if (written == 0) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::ioFailure("Atomic publisher write made no progress"));
            }
            offset += written;
        }
        return ::media::Status::success();
    }

    ::media::Status flushAndClose() override
    {
        if (m_phase != Phase::Open || m_handle == INVALID_HANDLE_VALUE) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Atomic file transaction is not open"));
        }
        if (!FlushFileBuffers(m_handle)) {
            return ::media::Status::failure(win32Failure("Atomic publisher flush failed"));
        }
        if (!CloseHandle(m_handle)) {
            m_phase = Phase::Failed;
            return ::media::Status::failure(win32Failure("Atomic publisher close failed"));
        }
        m_handle = INVALID_HANDLE_VALUE;
        m_phase = Phase::Flushed;
        return ::media::Status::success();
    }

    ::media::Status replaceTarget() override
    {
        if (m_phase != Phase::Flushed) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Atomic file transaction is not flushed"));
        }
        if (!MoveFileExW(m_temporaryPath.c_str(), m_targetPath.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            m_phase = Phase::Failed;
            return ::media::Status::failure(win32Failure("Atomic publisher replace failed"));
        }
        m_temporaryPath.clear();
        m_phase = Phase::Committed;
        return ::media::Status::success();
    }

private:
    enum class Phase { Open, Flushed, Committed, Failed };

    void closeNoThrow() noexcept
    {
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    std::wstring m_targetPath;
    std::wstring m_temporaryPath;
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    Phase m_phase = Phase::Open;
};

} // namespace

::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>
MediaWin32AtomicFileReplacePort::begin(std::string_view targetPathUtf8)
{
    auto targetPath = strictUtf8ToWide(targetPathUtf8);
    if (!targetPath) {
        return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::failure(
            targetPath.error());
    }

    const DWORD processId = GetCurrentProcessId();
    for (unsigned int attempt = 0; attempt < kMaximumTemporaryNameAttempts; ++attempt) {
        const std::uint64_t sequence =
            g_temporarySequence.fetch_add(1, std::memory_order_relaxed);
        std::wstring temporaryPath = targetPath.value();
        temporaryPath += L".tmp." + std::to_wstring(processId) + L"." +
                         std::to_wstring(sequence);
        HANDLE handle = CreateFileW(
            temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            try {
                auto transaction = std::make_unique<Win32AtomicFileReplaceTransaction>(
                    std::move(targetPath.value()), temporaryPath, handle);
                return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::success(
                    std::move(transaction));
            } catch (...) {
                CloseHandle(handle);
                DeleteFileW(temporaryPath.c_str());
                throw;
            }
        }
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::failure(
                ::media::ErrorInfo::ioFailure(
                    "Atomic publisher could not create temporary file",
                    static_cast<int>(error)));
        }
    }
    return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::failure(
        ::media::ErrorInfo::ioFailure("Atomic publisher exhausted temporary names"));
}

} // namespace media::ffmpeg::graph

#else

namespace media::ffmpeg::graph {

::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>
MediaWin32AtomicFileReplacePort::begin(std::string_view)
{
    return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::failure(
        ::media::ErrorInfo::unsupported("Win32 atomic publisher is unavailable"));
}

} // namespace media::ffmpeg::graph

#endif
