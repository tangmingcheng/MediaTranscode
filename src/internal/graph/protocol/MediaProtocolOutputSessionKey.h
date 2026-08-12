#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace media::ffmpeg::graph {

class MediaProtocolOutputSessionKey final {
public:
    MediaProtocolOutputSessionKey() = default;
    explicit MediaProtocolOutputSessionKey(std::string value)
        : m_value(std::move(value))
    {
    }

    bool valid() const noexcept { return !m_value.empty(); }
    const std::string& value() const noexcept { return m_value; }
    friend bool operator==(const MediaProtocolOutputSessionKey&,
                           const MediaProtocolOutputSessionKey&) = default;

private:
    std::string m_value;
};

} // namespace media::ffmpeg::graph
