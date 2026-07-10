#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace media::ffmpeg::graph {

class MediaInputTerminalTracker final {
public:
    explicit MediaInputTerminalTracker(std::vector<std::string> expectedInputs)
        : m_expected(expectedInputs.begin(), expectedInputs.end())
    {
    }

    bool markEof(std::string_view input)
    {
        const std::string key(input);
        return m_expected.contains(key) && m_terminal.insert(key).second;
    }

    bool markClosed(std::string_view input)
    {
        return markEof(input);
    }

    bool terminal(std::string_view input) const
    {
        return m_terminal.contains(std::string(input));
    }

    bool finished() const noexcept
    {
        return !m_expected.empty() && m_terminal.size() == m_expected.size();
    }

    void reset() noexcept
    {
        m_terminal.clear();
    }

private:
    std::unordered_set<std::string> m_expected;
    std::unordered_set<std::string> m_terminal;
};

} // namespace media::ffmpeg::graph
