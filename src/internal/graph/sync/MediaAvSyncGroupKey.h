#pragma once

#include <string>
#include <utility>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupKey final {
public:
    explicit MediaAvSyncGroupKey(std::string value) : m_value(std::move(value)) {}

    const std::string& value() const noexcept { return m_value; }
    bool valid() const noexcept { return !m_value.empty(); }
    friend bool operator==(const MediaAvSyncGroupKey&, const MediaAvSyncGroupKey&) = default;

private:
    std::string m_value;
};

struct MediaAvSyncGroupKeyHash final {
    std::size_t operator()(const MediaAvSyncGroupKey& key) const noexcept
    {
        return std::hash<std::string>{}(key.value());
    }
};

} // namespace media::ffmpeg::graph
