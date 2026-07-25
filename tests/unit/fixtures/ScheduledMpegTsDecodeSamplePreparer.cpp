#include "unit/fixtures/ScheduledMpegTsDecodeSamplePreparer.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace media_transcode::test {
namespace {

::media::ErrorInfo preparationFailure(std::string message, int code)
{
    return ::media::ErrorInfo::ioFailure(std::move(message), code);
}

std::filesystem::path temporarySamplePath()
{
    return std::filesystem::temp_directory_path() /
        ("scheduled_mpeg_ts_decode_48k_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()) +
         ".mp4");
}

#if defined(_WIN32)

struct HandleCloser final {
    void operator()(void* handle) const noexcept
    {
        if (handle) CloseHandle(static_cast<HANDLE>(handle));
    }
};

using UniqueHandle = std::unique_ptr<void, HandleCloser>;

::media::Status runFfmpeg(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& source,
    const std::filesystem::path& output)
{
    const std::wstring command =
        L"\"" + ffmpeg.wstring() +
        L"\" -hide_banner -loglevel error -y -i \"" + source.wstring() +
        L"\" -t 3 -map 0:v:0 -map 0:a:0 -c:v copy -c:a aac "
        L"-profile:a aac_low -b:a 128k -ar 48000 -ac 2 -movflags +faststart \"" +
        output.wstring() + L"\"";
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(
            ffmpeg.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        return ::media::Status::failure(preparationFailure(
            "could not start FFmpeg 48 kHz decode sample preparation",
            static_cast<int>(GetLastError())));
    }
    UniqueHandle thread(process.hThread);
    UniqueHandle child(process.hProcess);
    const DWORD waited = WaitForSingleObject(process.hProcess, 60'000);
    if (waited != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5'000);
        return ::media::Status::failure(preparationFailure(
            "FFmpeg 48 kHz decode sample preparation timed out",
            waited == WAIT_FAILED ? static_cast<int>(GetLastError()) : 1));
    }
    DWORD exitCode = 1;
    if (!GetExitCodeProcess(process.hProcess, &exitCode) || exitCode != 0) {
        return ::media::Status::failure(preparationFailure(
            "FFmpeg 48 kHz decode sample preparation failed",
            static_cast<int>(exitCode)));
    }
    return ::media::Status::success();
}

#else

::media::Status runFfmpeg(
    const std::filesystem::path&,
    const std::filesystem::path&,
    const std::filesystem::path&)
{
    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "48 kHz decode sample preparation is unavailable on this platform"));
}

#endif

} // namespace

::media::Result<ScheduledMpegTsDecodeSamplePreparer>
ScheduledMpegTsDecodeSamplePreparer::prepare(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& source)
{
    const auto output = temporarySamplePath();
    if (auto prepared = runFfmpeg(ffmpeg, source, output); !prepared) {
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
        return ::media::Result<ScheduledMpegTsDecodeSamplePreparer>::failure(
            prepared.error());
    }
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(output, filesystemError) ||
        std::filesystem::file_size(output, filesystemError) == 0) {
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
        return ::media::Result<ScheduledMpegTsDecodeSamplePreparer>::failure(
            preparationFailure(
                "FFmpeg 48 kHz decode sample preparation produced no artifact",
                filesystemError.value()));
    }
    return ::media::Result<ScheduledMpegTsDecodeSamplePreparer>::success(
        ScheduledMpegTsDecodeSamplePreparer(output));
}

ScheduledMpegTsDecodeSamplePreparer::ScheduledMpegTsDecodeSamplePreparer(
    std::filesystem::path path) noexcept
    : m_path(std::move(path))
{
}

ScheduledMpegTsDecodeSamplePreparer::ScheduledMpegTsDecodeSamplePreparer(
    ScheduledMpegTsDecodeSamplePreparer&& other) noexcept
    : m_path(std::move(other.m_path))
{
    other.m_path.clear();
}

ScheduledMpegTsDecodeSamplePreparer&
ScheduledMpegTsDecodeSamplePreparer::operator=(
    ScheduledMpegTsDecodeSamplePreparer&& other) noexcept
{
    if (this == &other) return *this;
    remove();
    m_path = std::move(other.m_path);
    other.m_path.clear();
    return *this;
}

ScheduledMpegTsDecodeSamplePreparer::~ScheduledMpegTsDecodeSamplePreparer()
{
    remove();
}

void ScheduledMpegTsDecodeSamplePreparer::remove() noexcept
{
    if (m_path.empty()) return;
    std::error_code ignored;
    std::filesystem::remove(m_path, ignored);
    m_path.clear();
}

} // namespace media_transcode::test
