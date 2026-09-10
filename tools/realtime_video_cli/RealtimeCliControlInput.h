#pragma once

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace media::ffmpeg::graph::cli {

// Sole stdin reader. Reads only ready input; owns no background thread.
class RealtimeCliControlInput final {
public:
    RealtimeCliControlInput()
    {
#ifdef _WIN32
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        if (!system.dwPageSize) throw std::runtime_error("system page size is unavailable");
        m_chunk.resize(system.dwPageSize);
        // CreateProcessW command-line limit; three UTF-8 bytes per UTF-16 unit.
        constexpr std::size_t WindowsCommandLineCodeUnits = 32767;
        m_maximumLineBytes = WindowsCommandLineCodeUnits * 3;
        m_handle = GetStdHandle(STD_INPUT_HANDLE);
        if (!m_handle || m_handle == INVALID_HANDLE_VALUE) { m_closed = true; return; }
        DWORD mode{};
        m_console = GetConsoleMode(m_handle, &mode) != FALSE;
        m_fileType = GetFileType(m_handle);
        if (!m_console && m_fileType != FILE_TYPE_PIPE && m_fileType != FILE_TYPE_DISK)
            m_closed = true;
#else
        const auto pageSize = ::sysconf(_SC_PAGESIZE);
        const auto argumentBytes = ::sysconf(_SC_ARG_MAX);
        if (pageSize <= 0 || argumentBytes <= 0)
            throw std::runtime_error("system command-input bounds are unavailable");
        m_chunk.resize(static_cast<std::size_t>(pageSize));
        m_maximumLineBytes = static_cast<std::size_t>(argumentBytes);
        m_originalFlags = ::fcntl(STDIN_FILENO, F_GETFL);
        if (m_originalFlags < 0) { m_closed = true; return; }
        if (::fcntl(STDIN_FILENO, F_SETFL, m_originalFlags | O_NONBLOCK) < 0)
            throw std::runtime_error("cannot make stdin nonblocking");
        m_restoreFlags = true;
#endif
    }

    ~RealtimeCliControlInput() noexcept
    {
#ifndef _WIN32
        if (m_restoreFlags) (void)::fcntl(STDIN_FILENO, F_SETFL, m_originalFlags);
#endif
    }
    RealtimeCliControlInput(const RealtimeCliControlInput&) = delete;
    RealtimeCliControlInput& operator=(const RealtimeCliControlInput&) = delete;

    bool closed() const noexcept { return m_closed && m_position == m_size; }

    // At most one OS-sized batch and one command per controller progress tick.
    std::optional<std::string> pollLine()
    {
        if (m_position == m_size && !m_closed) readChunk();
        while (m_position < m_size) {
            const char character = m_chunk[m_position++];
            if (character == '\n') {
                if (m_discarding) {
                    m_discarding = false;
                    throw std::invalid_argument("control command exceeds the operating-system argument bound");
                }
                if (!m_line.empty() && m_line.back() == '\r') m_line.pop_back();
                auto line = std::move(m_line);
                m_line.clear();
                return line;
            }
#ifdef _WIN32
            if (m_console && character == '\b') {
                if (!m_line.empty()) {
                    auto first = m_line.size() - 1;
                    while (first && (static_cast<unsigned char>(m_line[first]) & 0xc0U) == 0x80U) --first;
                    m_line.resize(first);
                }
                continue;
            }
#endif
            if (!m_discarding) {
                if (m_line.size() == m_maximumLineBytes) {
                    m_line.clear();
                    m_discarding = true;
                } else m_line.push_back(character);
            }
        }
        if (m_closed && (!m_line.empty() || m_discarding)) {
            m_line.clear();
            m_discarding = false;
            throw std::invalid_argument("EOF discarded an incomplete control command; terminate commands with a newline");
        }
        return std::nullopt;
    }

private:
    void failRead(const char* message)
    {
        m_closed = true;
        throw std::runtime_error(message);
    }

    void readChunk()
    {
        m_position = m_size = 0;
#ifdef _WIN32
        if (m_console) { readConsole(); return; }
        DWORD available = static_cast<DWORD>(m_chunk.size());
        if (m_fileType == FILE_TYPE_PIPE) {
            if (!PeekNamedPipe(m_handle, nullptr, 0, nullptr, &available, nullptr)) {
                if (GetLastError() == ERROR_BROKEN_PIPE) { m_closed = true; return; }
                failRead("stdin pipe readiness failed");
            }
            if (!available) return;
        }
        DWORD bytes{};
        if (!ReadFile(m_handle, m_chunk.data(),
                (std::min)(available, static_cast<DWORD>(m_chunk.size())), &bytes, nullptr)) {
            if (GetLastError() == ERROR_BROKEN_PIPE || GetLastError() == ERROR_HANDLE_EOF) {
                m_closed = true;
                return;
            }
            failRead("stdin read failed");
        }
        m_size = bytes;
        m_closed = bytes == 0;
#else
        const auto bytes = ::read(STDIN_FILENO, m_chunk.data(), m_chunk.size());
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return;
            failRead("stdin nonblocking read failed");
        }
        m_size = static_cast<std::size_t>(bytes);
        m_closed = bytes == 0;
#endif
    }

#ifdef _WIN32
    void readConsole()
    {
        DWORD available{};
        if (!GetNumberOfConsoleInputEvents(m_handle, &available))
            failRead("console readiness failed");
        // UTF-8 scalar values occupy at most four bytes.
        for (std::size_t budget = m_chunk.size() / 4; budget &&
             (available || m_consoleRepeat); --budget) {
            if (!m_consoleRepeat) {
                INPUT_RECORD record{};
                DWORD read{};
                if (!ReadConsoleInputW(m_handle, &record, 1, &read))
                    failRead("console event read failed");
                --available;
                if (!read || record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                    continue;
                m_consoleCharacter = record.Event.KeyEvent.uChar.UnicodeChar;
                m_consoleRepeat = record.Event.KeyEvent.wRepeatCount;
                if (!m_consoleRepeat) continue;
            }
            --m_consoleRepeat;
            const wchar_t character = m_consoleCharacter;
            if (!character) continue;
            if (character == 26) { m_closed = true; return; } // Console Ctrl-Z EOF.
            if (character >= 0xd800 && character <= 0xdbff) {
                m_highSurrogate = character;
                continue;
            }
            wchar_t units[2]{};
            int count = 1;
            units[0] = character;
            if (m_highSurrogate) {
                units[0] = m_highSurrogate;
                units[1] = character;
                count = 2;
                m_highSurrogate = 0;
            }
            const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, units, count,
                m_chunk.data() + m_size, static_cast<int>(m_chunk.size() - m_size), nullptr, nullptr);
            if (bytes <= 0) throw std::invalid_argument("console command contains invalid Unicode");
            if (character == L'\r') m_chunk[m_size] = '\n';
            m_size += static_cast<std::size_t>(bytes);
            const wchar_t* echo = character == L'\r' ? L"\r\n" : character == L'\b' ? L"\b \b" : units;
            const DWORD echoSize = character == L'\r' ? 2 : character == L'\b' ? 3 : static_cast<DWORD>(count);
            DWORD written{};
            (void)WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), echo, echoSize, &written, nullptr);
            if (character == L'\r') return;
        }
    }
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    DWORD m_fileType = FILE_TYPE_UNKNOWN;
    bool m_console = false;
    wchar_t m_highSurrogate = 0;
    wchar_t m_consoleCharacter = 0;
    WORD m_consoleRepeat = 0;
#else
    int m_originalFlags = 0;
    bool m_restoreFlags = false;
#endif
    std::vector<char> m_chunk;
    std::size_t m_position = 0;
    std::size_t m_size = 0;
    std::size_t m_maximumLineBytes = 0;
    std::string m_line;
    bool m_discarding = false;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph::cli
