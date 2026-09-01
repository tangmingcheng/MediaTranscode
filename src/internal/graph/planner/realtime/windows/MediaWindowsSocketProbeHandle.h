#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

namespace media::ffmpeg::graph {

class MediaWindowsWinsockProbeSession final {
public:
    MediaWindowsWinsockProbeSession() noexcept
    {
        m_error = WSAStartup(MAKEWORD(2, 2), &m_data);
    }

    ~MediaWindowsWinsockProbeSession()
    {
        if (m_error == 0) WSACleanup();
    }

    MediaWindowsWinsockProbeSession(
        const MediaWindowsWinsockProbeSession&) = delete;
    MediaWindowsWinsockProbeSession& operator=(
        const MediaWindowsWinsockProbeSession&) = delete;

    int error() const noexcept { return m_error; }

private:
    WSADATA m_data{};
    int m_error = 0;
};

class MediaWindowsSocketProbeHandle final {
public:
    explicit MediaWindowsSocketProbeHandle(
        SOCKET value = INVALID_SOCKET) noexcept
        : m_value(value)
    {
    }

    ~MediaWindowsSocketProbeHandle()
    {
        if (m_value != INVALID_SOCKET) closesocket(m_value);
    }

    MediaWindowsSocketProbeHandle(
        const MediaWindowsSocketProbeHandle&) = delete;
    MediaWindowsSocketProbeHandle& operator=(
        const MediaWindowsSocketProbeHandle&) = delete;

    SOCKET get() const noexcept { return m_value; }
    void close() noexcept
    {
        if (m_value == INVALID_SOCKET) return;
        closesocket(m_value);
        m_value = INVALID_SOCKET;
    }

private:
    SOCKET m_value;
};

} // namespace media::ffmpeg::graph

#endif
