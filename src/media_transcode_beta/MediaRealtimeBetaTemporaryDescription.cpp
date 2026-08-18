#include "media_transcode_beta/MediaRealtimeBetaTemporaryDescription.h"

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

::media::Result<std::string> createAtomicTemporaryFile()
{
    std::vector<char> temporaryDirectory(MAX_PATH + 1U, '\0');
    const DWORD directoryLength = GetTempPathA(
        static_cast<DWORD>(temporaryDirectory.size()),
        temporaryDirectory.data());
    if (directoryLength == 0U ||
        directoryLength >= temporaryDirectory.size()) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to resolve the Windows temporary directory",
                checkedWindowsError(GetLastError())));
    }

    std::vector<char> path(MAX_PATH + 1U, '\0');
    if (GetTempFileNameA(
            temporaryDirectory.data(), "mtb", 0U, path.data()) == 0U) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to atomically create a temporary Beta description",
                checkedWindowsError(GetLastError())));
    }
    return ::media::Result<std::string>::success(path.data());
}

#else

::media::Result<std::string> createAtomicTemporaryFile()
{
    std::error_code error;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path(error);
    if (error) {
        return ::media::Result<std::string>::failure(
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
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::ioFailure(
                "failed to atomically create a temporary Beta description",
                errno));
    }
    ::close(descriptor);
    return ::media::Result<std::string>::success(writablePattern.data());
}

#endif

} // namespace

MediaRealtimeBetaTemporaryDescription::MediaRealtimeBetaTemporaryDescription(
    std::string path) noexcept
    : m_path(std::move(path))
{
}

::media::Result<MediaRealtimeBetaTemporaryDescription>
MediaRealtimeBetaTemporaryDescription::create()
{
    try {
        auto path = createAtomicTemporaryFile();
        if (!path) {
            return ::media::Result<
                MediaRealtimeBetaTemporaryDescription>::failure(path.error());
        }
        return ::media::Result<MediaRealtimeBetaTemporaryDescription>::success(
            MediaRealtimeBetaTemporaryDescription(
                std::move(path).value()));
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
    : m_path(std::move(other.m_path))
{
    other.m_path.clear();
}

MediaRealtimeBetaTemporaryDescription&
MediaRealtimeBetaTemporaryDescription::operator=(
    MediaRealtimeBetaTemporaryDescription&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    removeOwnedFile();
    m_path = std::move(other.m_path);
    other.m_path.clear();
    return *this;
}

const std::string& MediaRealtimeBetaTemporaryDescription::path() const noexcept
{
    return m_path;
}

::media::Result<std::string>
MediaRealtimeBetaTemporaryDescription::readCompletedText() const
{
    if (m_path.empty()) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::notInitialized(
                "temporary Beta description has no owned path"));
    }

    try {
        std::ifstream input(m_path, std::ios::binary);
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
    if (m_path.empty()) {
        return;
    }
#ifdef _WIN32
    DeleteFileA(m_path.c_str());
#else
    ::unlink(m_path.c_str());
#endif
    m_path.clear();
}

} // namespace media::beta
