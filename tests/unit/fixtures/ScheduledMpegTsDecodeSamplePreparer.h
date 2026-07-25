#pragma once

#include "media_transcode/Result.h"

#include <filesystem>

namespace media_transcode::test {

class ScheduledMpegTsDecodeSamplePreparer final {
public:
    static ::media::Result<ScheduledMpegTsDecodeSamplePreparer> prepare(
        const std::filesystem::path& ffmpeg,
        const std::filesystem::path& source);

    ScheduledMpegTsDecodeSamplePreparer(
        ScheduledMpegTsDecodeSamplePreparer&& other) noexcept;
    ScheduledMpegTsDecodeSamplePreparer& operator=(
        ScheduledMpegTsDecodeSamplePreparer&& other) noexcept;
    ~ScheduledMpegTsDecodeSamplePreparer();

    ScheduledMpegTsDecodeSamplePreparer(
        const ScheduledMpegTsDecodeSamplePreparer&) = delete;
    ScheduledMpegTsDecodeSamplePreparer& operator=(
        const ScheduledMpegTsDecodeSamplePreparer&) = delete;

    const std::filesystem::path& path() const noexcept { return m_path; }

private:
    explicit ScheduledMpegTsDecodeSamplePreparer(
        std::filesystem::path path) noexcept;

    void remove() noexcept;

    std::filesystem::path m_path;
};

} // namespace media_transcode::test
