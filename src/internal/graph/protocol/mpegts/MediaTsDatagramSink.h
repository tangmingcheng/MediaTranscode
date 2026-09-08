#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaTsDatagramEnqueueWindow final {
public:
    static ::media::Result<MediaTsDatagramEnqueueWindow> create(
        MediaRunningTime notBefore,
        MediaRunningTime deadline)
    {
        if (deadline < notBefore) {
            return ::media::Result<MediaTsDatagramEnqueueWindow>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS datagram enqueue window is inverted"));
        }
        return ::media::Result<MediaTsDatagramEnqueueWindow>::success(
            MediaTsDatagramEnqueueWindow(
                notBefore, deadline));
    }

    MediaRunningTime notBefore() const noexcept { return m_notBefore; }
    MediaRunningTime deadline() const noexcept { return m_deadline; }

private:
    MediaTsDatagramEnqueueWindow(
        MediaRunningTime notBefore,
        MediaRunningTime deadline) noexcept
        : m_notBefore(notBefore),
          m_deadline(deadline) {}

    MediaRunningTime m_notBefore;
    MediaRunningTime m_deadline;
};

class MediaTsDatagramSink {
public:
    virtual ~MediaTsDatagramSink() = default;

    virtual ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> completeTsPackets,
        const MediaTsDatagramEnqueueWindow& enqueueWindow) = 0;
    virtual ::media::Status flush() = 0;
    virtual ::media::Status close() = 0;
    virtual void abort() noexcept = 0;
};

} // namespace media::ffmpeg::graph
