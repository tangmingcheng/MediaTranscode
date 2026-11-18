#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {

// Immediate construction prevents retaining addresses of automatic arrays.
class MediaDiagnosticLiteral final {
public:
    template<std::size_t N>
    consteval explicit MediaDiagnosticLiteral(const char (&text)[N])
        : m_text(text, N - 1)
    {
        if (text[N - 1] != '\0') throw "diagnostic literal must be terminated";
    }
    constexpr std::string_view view() const noexcept { return m_text; }
private:
    std::string_view m_text;
};

class MediaDiagnosticName final {
public:
    MediaDiagnosticName() noexcept : m_value(MediaDiagnosticLiteral{""}) {}
    MediaDiagnosticName(MediaDiagnosticLiteral text) noexcept : m_value(text) {}
    MediaDiagnosticName(std::string text) : m_value(std::move(text)) {}
    MediaDiagnosticName(const char* text) : m_value(std::string(text)) {}
    MediaDiagnosticName(std::nullptr_t) = delete;

    std::string_view view() const noexcept
    {
        if (const auto* literal = std::get_if<MediaDiagnosticLiteral>(&m_value))
            return literal->view();
        return std::get<std::string>(m_value);
    }
private:
    std::variant<MediaDiagnosticLiteral, std::string> m_value;
};

} // namespace media::ffmpeg::graph
