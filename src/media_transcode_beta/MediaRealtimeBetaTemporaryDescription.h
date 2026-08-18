#pragma once

#include "media_transcode/Result.h"

#include <string>

namespace media::beta {

class MediaRealtimeBetaTemporaryDescription final {
public:
    static ::media::Result<MediaRealtimeBetaTemporaryDescription> create();

    ~MediaRealtimeBetaTemporaryDescription() noexcept;

    MediaRealtimeBetaTemporaryDescription(
        const MediaRealtimeBetaTemporaryDescription&) = delete;
    MediaRealtimeBetaTemporaryDescription& operator=(
        const MediaRealtimeBetaTemporaryDescription&) = delete;
    MediaRealtimeBetaTemporaryDescription(
        MediaRealtimeBetaTemporaryDescription&& other) noexcept;
    MediaRealtimeBetaTemporaryDescription& operator=(
        MediaRealtimeBetaTemporaryDescription&& other) noexcept;

    const std::string& path() const noexcept;
    ::media::Result<std::string> readCompletedText() const;

private:
    explicit MediaRealtimeBetaTemporaryDescription(std::string path) noexcept;

    void removeOwnedFile() noexcept;

    std::string m_path;
};

} // namespace media::beta
