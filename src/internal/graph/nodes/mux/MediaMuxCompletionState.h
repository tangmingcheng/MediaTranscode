#pragma once

#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

namespace media::ffmpeg::graph {

class MediaMuxCompletionState final {
public:
    void setExpectedConfigKeys(std::vector<std::string> inputs)
    {
        m_expectedConfigs = {inputs.begin(), inputs.end()};
    }

    void setExpectedTerminalChannels(std::vector<std::string> inputs)
    {
        m_terminals = std::make_unique<MediaInputTerminalTracker>(std::move(inputs));
    }

    bool markConfigReady(std::string input)
    {
        return m_expectedConfigs.contains(input) && m_configs.insert(std::move(input)).second;
    }
    void markHeaderWritten() noexcept { m_headerWritten = true; }
    void setPendingPackets(std::size_t count) noexcept { m_pendingPackets = count; }
    bool markInputEof(std::string_view input) { return m_terminals && m_terminals->markEof(input); }
    bool markInputClosed(std::string_view input) { return m_terminals && m_terminals->markClosed(input); }
    void markTrailerWritten() noexcept { m_trailerWritten = true; }

    bool readyForTrailer() const noexcept
    {
        return m_terminals && m_terminals->finished() && configsReady() &&
               m_headerWritten && m_pendingPackets == 0;
    }

    bool finished() const noexcept
    {
        return readyForTrailer() && m_trailerWritten;
    }

    void reset() noexcept
    {
        if (m_terminals) m_terminals->reset();
        m_configs.clear();
        m_headerWritten = false;
        m_trailerWritten = false;
        m_pendingPackets = 0;
    }

private:
    bool configsReady() const noexcept
    {
        if (m_expectedConfigs.empty() || m_configs.size() != m_expectedConfigs.size()) return false;
        for (const auto& key : m_expectedConfigs) {
            if (!m_configs.contains(key)) return false;
        }
        return true;
    }

    std::unique_ptr<MediaInputTerminalTracker> m_terminals;
    std::unordered_set<std::string> m_expectedConfigs;
    std::unordered_set<std::string> m_configs;
    bool m_headerWritten = false;
    bool m_trailerWritten = false;
    std::size_t m_pendingPackets = 0;
};

} // namespace media::ffmpeg::graph
