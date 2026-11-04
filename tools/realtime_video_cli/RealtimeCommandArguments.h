#pragma once

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace media::ffmpeg::graph::cli {

// Quotes delimit tokens; backslash only escapes a matching quote. No shell expansion.
inline std::vector<std::string> tokenizeRealtimeCommand(std::string_view line)
{
    std::vector<std::string> arguments;
    std::string token;
    char quote = 0;
    bool started = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (!character) throw std::invalid_argument("control command contains a NUL byte");
        if (character == '\\' && quote && index + 1 < line.size() && line[index + 1] == quote) {
            token.push_back(line[++index]);
        } else if (quote) {
            if (character == quote) quote = 0;
            else token.push_back(character);
        } else if (character == '\'' || character == '"') {
            quote = character;
            started = true;
        } else if (std::isspace(static_cast<unsigned char>(character))) {
            if (started) { arguments.push_back(std::move(token)); token.clear(); started = false; }
        } else { token.push_back(character); started = true; }
    }
    if (quote) throw std::invalid_argument("control command has an unterminated quote");
    if (started) arguments.push_back(std::move(token));
    return arguments;
}

} // namespace media::ffmpeg::graph::cli
