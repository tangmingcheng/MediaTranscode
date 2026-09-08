#include "media_transcode_beta/MediaRealtimeBetaTemporaryDescription.h"

#include <array>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
#endif

namespace media::beta {
namespace {

#ifdef _WIN32

int checkedWindowsError(DWORD error) noexcept
{
    return error <= static_cast<DWORD>(std::numeric_limits<int>::max())
        ? static_cast<int>(error)
        : 0;
}

class WindowsTemporaryFileGuard final {
public:
    explicit WindowsTemporaryFileGuard(const wchar_t* path) noexcept
        : m_path(path)
    {
    }

    ~WindowsTemporaryFileGuard() noexcept
    {
        if (m_path != nullptr) {
            DeleteFileW(m_path);
        }
    }

    void release() noexcept { m_path = nullptr; }

private:
    const wchar_t* m_path;
};

class WindowsHandleGuard final {
public:
    explicit WindowsHandleGuard(HANDLE handle) noexcept
        : m_handle(handle)
    {
    }

    ~WindowsHandleGuard() noexcept
    {
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
        }
    }

private:
    HANDLE m_handle;
};

::media::Result<std::string> checkedUtf8Path(const wchar_t* nativePath)
{
    const int nativeLength = static_cast<int>(wcslen(nativePath));
    const int utf8Length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, nativePath, nativeLength, nullptr, 0,
        nullptr, nullptr);
    if (utf8Length <= 0) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to convert the temporary Beta description path to UTF-8",
                checkedWindowsError(GetLastError())));
    }

    std::string utf8Path(static_cast<std::size_t>(utf8Length), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, nativePath, nativeLength,
        utf8Path.data(), utf8Length, nullptr, nullptr);
    if (converted != utf8Length) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to convert the temporary Beta description path to UTF-8",
                checkedWindowsError(GetLastError())));
    }
    return ::media::Result<std::string>::success(std::move(utf8Path));
}

} // namespace

::media::Result<MediaRealtimeBetaTemporaryDescription>
MediaRealtimeBetaTemporaryDescription::createAtomicTemporaryFile()
{
    std::array<wchar_t, MAX_PATH + 1U> temporaryDirectory{};
    const DWORD directoryLength = GetTempPathW(
        static_cast<DWORD>(temporaryDirectory.size()),
        temporaryDirectory.data());
    if (directoryLength == 0U ||
        directoryLength >= temporaryDirectory.size()) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to resolve the Windows temporary directory",
                checkedWindowsError(GetLastError())));
    }

    std::array<wchar_t, MAX_PATH + 1U> nativePath{};
    if (GetTempFileNameW(
            temporaryDirectory.data(), L"mtb", 0U,
            nativePath.data()) == 0U) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to atomically create a temporary Beta description",
                checkedWindowsError(GetLastError())));
    }
    WindowsTemporaryFileGuard cleanup(nativePath.data());

    auto plannerPath = checkedUtf8Path(nativePath.data());
    if (!plannerPath) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
                plannerPath.error());
    }
    MediaRealtimeBetaTemporaryDescription owner(
        std::wstring(nativePath.data()), std::move(plannerPath).value());
    cleanup.release();
    return ::media::Result<MediaRealtimeBetaTemporaryDescription>::success(
        std::move(owner));
}

#else

class PosixTemporaryFileGuard final {
public:
    PosixTemporaryFileGuard(const char* path, int descriptor) noexcept
        : m_path(path)
        , m_descriptor(descriptor)
    {
    }

    ~PosixTemporaryFileGuard() noexcept
    {
        if (m_descriptor >= 0) {
            ::close(m_descriptor);
        }
        if (m_path != nullptr) {
            ::unlink(m_path);
        }
    }

    int closeFile() noexcept
    {
        const int descriptor = m_descriptor;
        m_descriptor = -1;
        return ::close(descriptor);
    }

    void release() noexcept { m_path = nullptr; }

private:
    const char* m_path;
    int m_descriptor;
};

} // namespace

::media::Result<MediaRealtimeBetaTemporaryDescription>
MediaRealtimeBetaTemporaryDescription::createAtomicTemporaryFile()
{
    std::error_code error;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path(error);
    if (error) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to resolve the platform temporary directory",
                error.value()));
    }

    const std::string pattern =
        (directory / "media-transcode-beta-XXXXXX").string();
    std::vector<char> writablePattern(pattern.begin(), pattern.end());
    writablePattern.push_back('\0');
    const int descriptor = ::mkstemp(writablePattern.data());
    if (descriptor < 0) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to atomically create a temporary Beta description",
                errno));
    }
    PosixTemporaryFileGuard cleanup(writablePattern.data(), descriptor);
    if (cleanup.closeFile() != 0) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
                ::media::ErrorInfo::ioFailure(
                    "failed to close the temporary Beta description",
                    errno));
    }

    std::string nativePath(writablePattern.data());
    std::string plannerPath(nativePath);
    MediaRealtimeBetaTemporaryDescription owner(
        std::move(nativePath), std::move(plannerPath));
    cleanup.release();
    return ::media::Result<MediaRealtimeBetaTemporaryDescription>::success(
        std::move(owner));
}

#endif

MediaRealtimeBetaTemporaryDescription::MediaRealtimeBetaTemporaryDescription(
#ifdef _WIN32
    std::wstring nativePath,
#else
    std::string nativePath,
#endif
    std::string plannerPath) noexcept
    : m_nativePath(std::move(nativePath))
    , m_plannerPath(std::move(plannerPath))
{
}

::media::Result<MediaRealtimeBetaTemporaryDescription>
MediaRealtimeBetaTemporaryDescription::create()
{
    try {
        return createAtomicTemporaryFile();
    } catch (const std::bad_alloc&) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "temporary Beta description allocation failed"));
    } catch (const std::exception& error) {
        return ::media::Result<
            MediaRealtimeBetaTemporaryDescription>::failure(
                ::media::ErrorInfo::ioFailure(
                    std::string("temporary Beta description creation failed: ") +
                    error.what()));
    }
}

MediaRealtimeBetaTemporaryDescription::~MediaRealtimeBetaTemporaryDescription()
    noexcept
{
    removeOwnedFile();
}

MediaRealtimeBetaTemporaryDescription::MediaRealtimeBetaTemporaryDescription(
    MediaRealtimeBetaTemporaryDescription&& other) noexcept
    : m_nativePath(std::move(other.m_nativePath))
    , m_plannerPath(std::move(other.m_plannerPath))
{
    other.m_nativePath.clear();
    other.m_plannerPath.clear();
}

MediaRealtimeBetaTemporaryDescription&
MediaRealtimeBetaTemporaryDescription::operator=(
    MediaRealtimeBetaTemporaryDescription&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    removeOwnedFile();
    m_nativePath = std::move(other.m_nativePath);
    m_plannerPath = std::move(other.m_plannerPath);
    other.m_nativePath.clear();
    other.m_plannerPath.clear();
    return *this;
}

const std::string& MediaRealtimeBetaTemporaryDescription::path() const noexcept
{
    return m_plannerPath;
}

::media::Result<std::string>
MediaRealtimeBetaTemporaryDescription::readCompletedText() const
{
    if (m_nativePath.empty()) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::notInitialized(
                "temporary Beta description has no owned path"));
    }

    try {
#ifdef _WIN32
        const HANDLE file = CreateFileW(
            m_nativePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::ioFailure(
                    "failed to open the completed Beta output description",
                    checkedWindowsError(GetLastError())));
        }
        WindowsHandleGuard closeFile(file);

        LARGE_INTEGER nativeSize{};
        if (GetFileSizeEx(file, &nativeSize) == FALSE ||
            nativeSize.QuadPart < 0 ||
            static_cast<unsigned long long>(nativeSize.QuadPart) >
                static_cast<unsigned long long>(
                    std::numeric_limits<std::size_t>::max())) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::ioFailure(
                    "failed to size the completed Beta output description",
                    checkedWindowsError(GetLastError())));
        }
        if (nativeSize.QuadPart == 0) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::wouldBlock(
                    "Beta output description is not complete"));
        }

        std::string text(
            static_cast<std::size_t>(nativeSize.QuadPart), '\0');
        std::size_t totalRead = 0U;
        while (totalRead < text.size()) {
            const std::size_t remaining = text.size() - totalRead;
            const DWORD requested = remaining > MAXDWORD
                ? MAXDWORD
                : static_cast<DWORD>(remaining);
            DWORD bytesRead = 0U;
            if (ReadFile(
                    file, text.data() + totalRead, requested, &bytesRead,
                    nullptr) == FALSE ||
                bytesRead == 0U) {
                return ::media::Result<std::string>::failure(
                    ::media::ErrorInfo::ioFailure(
                        "failed to read the completed Beta output description",
                        checkedWindowsError(GetLastError())));
            }
            totalRead += bytesRead;
        }
#else
        std::ifstream input(m_nativePath, std::ios::binary);
        if (!input) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::ioFailure(
                    "failed to open the completed Beta output description"));
        }
        const std::istreambuf_iterator<char> end;
        std::string text(std::istreambuf_iterator<char>(input), end);
        if (input.bad()) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::ioFailure(
                    "failed to read the completed Beta output description"));
        }
        if (text.empty()) {
            return ::media::Result<std::string>::failure(
                ::media::ErrorInfo::wouldBlock(
                    "Beta output description is not complete"));
        }
#endif
        return ::media::Result<std::string>::success(std::move(text));
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::allocationFailed(
                "completed Beta output description allocation failed"));
    } catch (const std::exception& error) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ioFailure(
                std::string("completed Beta output description read failed: ") +
                error.what()));
    }
}

void MediaRealtimeBetaTemporaryDescription::removeOwnedFile() noexcept
{
    if (m_nativePath.empty()) {
        return;
    }
#ifdef _WIN32
    DeleteFileW(m_nativePath.c_str());
#else
    ::unlink(m_nativePath.c_str());
#endif
    m_nativePath.clear();
    m_plannerPath.clear();
}

} // namespace media::beta
