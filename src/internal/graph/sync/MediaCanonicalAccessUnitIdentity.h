#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

enum class MediaScheduledStream : std::uint8_t { Video, Audio };

enum class MediaDecodeOrderMode : std::uint8_t {
    ReorderedRequiresDecodeTime,
    PresentationOrderNoReorder
};

class MediaSourceAccessUnitSequence final {
public:
    explicit constexpr MediaSourceAccessUnitSequence(std::uint64_t value) noexcept
        : m_value(value) {}
    constexpr std::uint64_t value() const noexcept { return m_value; }
    friend constexpr bool operator==(MediaSourceAccessUnitSequence,
                                     MediaSourceAccessUnitSequence) noexcept = default;
private:
    std::uint64_t m_value;
};

class MediaOutputAccessUnitSequence final {
public:
    explicit constexpr MediaOutputAccessUnitSequence(std::uint64_t value) noexcept
        : m_value(value) {}
    constexpr std::uint64_t value() const noexcept { return m_value; }
    friend constexpr bool operator==(MediaOutputAccessUnitSequence,
                                     MediaOutputAccessUnitSequence) noexcept = default;
private:
    std::uint64_t m_value;
};

class MediaCanonicalAccessUnitSequence final {
public:
    explicit constexpr MediaCanonicalAccessUnitSequence(std::uint64_t value) noexcept
        : m_value(value) {}
    constexpr std::uint64_t value() const noexcept { return m_value; }
    friend constexpr bool operator==(MediaCanonicalAccessUnitSequence,
                                     MediaCanonicalAccessUnitSequence) noexcept = default;
private:
    std::uint64_t m_value;
};

struct MediaSourceAccessUnitIdentity final {
    std::string sourceIdentity;
    MediaSourceAccessUnitSequence sourceSequence;
    friend bool operator==(const MediaSourceAccessUnitIdentity&,
                           const MediaSourceAccessUnitIdentity&) noexcept = default;
};

struct MediaOutputAccessUnitIdentity final {
    std::string outputIdentity;
    MediaScheduledStream stream;
    MediaOutputAccessUnitSequence outputSequence;
    friend bool operator==(const MediaOutputAccessUnitIdentity&,
                           const MediaOutputAccessUnitIdentity&) noexcept = default;
};

using MediaCanonicalAccessUnitIdentity = std::variant<
    MediaSourceAccessUnitIdentity, MediaOutputAccessUnitIdentity>;

} // namespace media::ffmpeg::graph
