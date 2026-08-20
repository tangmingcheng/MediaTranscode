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
    static ::media::Result<MediaRealtimeBetaTemporaryDescription>
        createAtomicTemporaryFile();

    MediaRealtimeBetaTemporaryDescription(
#ifdef _WIN32
        std::wstring nativePath,
#else
        std::string nativePath,
#endif
        std::string plannerPath) noexcept;

    void removeOwnedFile() noexcept;

#ifdef _WIN32
    std::wstring m_nativePath;
#else
    std::string m_nativePath;
#endif
    std::string m_plannerPath;
};

} // namespace media::beta
