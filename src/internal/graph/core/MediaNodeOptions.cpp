#include "internal/graph/core/MediaNodeOptions.h"

#include <utility>

namespace media::ffmpeg::graph {

void MediaNodeOptions::set(std::string key, std::string value)
{
    if (key.empty()) {
        return;
    }

    m_values[std::move(key)] = std::move(value);
}

bool MediaNodeOptions::has(const std::string& key) const
{
    return m_values.find(key) != m_values.end();
}

std::string MediaNodeOptions::value(const std::string& key, std::string missingValue) const
{
    const auto it = m_values.find(key);
    return it == m_values.end() ? std::move(missingValue) : it->second;
}

const MediaNodeOptions::Map& MediaNodeOptions::values() const noexcept
{
    return m_values;
}

bool MediaNodeOptions::empty() const noexcept
{
    return m_values.empty();
}

void MediaNodeOptions::clear()
{
    m_values.clear();
}

} // namespace media::ffmpeg::graph
