#pragma once

#ifndef _WIN32

#include <unistd.h>

namespace media::ffmpeg::graph {

class MediaLinuxSocketProbeHandle final {
public:
    explicit MediaLinuxSocketProbeHandle(int value) noexcept : m_value(value) {}
    ~MediaLinuxSocketProbeHandle()
    {
        if (m_value >= 0) ::close(m_value);
    }

    MediaLinuxSocketProbeHandle(const MediaLinuxSocketProbeHandle&) = delete;
    MediaLinuxSocketProbeHandle& operator=(
        const MediaLinuxSocketProbeHandle&) = delete;

    int get() const noexcept { return m_value; }

private:
    int m_value;
};

} // namespace media::ffmpeg::graph

#endif
