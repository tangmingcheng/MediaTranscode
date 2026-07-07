#pragma once

#include <string>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaNodeOptions final {
public:
    using Map = std::unordered_map<std::string, std::string>;

    void set(std::string key, std::string value);
    bool has(const std::string& key) const;
    std::string value(const std::string& key, std::string missingValue = {}) const;
    const Map& values() const noexcept;
    bool empty() const noexcept;
    void clear();

private:
    Map m_values;
};

} // namespace media::ffmpeg::graph
