#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

class MediaCanonicalTimeMapper;

enum class MediaTimeMappingConfidence {
    Locked,
    Degraded
};

class MediaCanonicalSourceTimestamp final {
public:
    MediaCanonicalSourceTimestamp(std::optional<MediaRunningTime> presentationTime,
                                  std::optional<MediaRunningTime> decodeTime,
                                  std::optional<MediaRunningTime> duration,
                                  std::uint64_t generation,
                                  std::string sourceIdentity,
                                  MediaTimeMappingConfidence confidence)
        : m_presentationTime(presentationTime)
        , m_decodeTime(decodeTime)
        , m_duration(duration)
        , m_generation(generation)
        , m_sourceIdentity(std::move(sourceIdentity))
        , m_confidence(confidence)
    {
    }

    const std::optional<MediaRunningTime>& presentationTime() const noexcept
    {
        return m_presentationTime;
    }
    const std::optional<MediaRunningTime>& decodeTime() const noexcept
    {
        return m_decodeTime;
    }
    const std::optional<MediaRunningTime>& duration() const noexcept
    {
        return m_duration;
    }
    std::uint64_t generation() const noexcept { return m_generation; }
    const std::string& sourceIdentity() const noexcept { return m_sourceIdentity; }
    MediaTimeMappingConfidence confidence() const noexcept { return m_confidence; }

private:
    std::optional<MediaRunningTime> m_presentationTime;
    std::optional<MediaRunningTime> m_decodeTime;
    std::optional<MediaRunningTime> m_duration;
    std::uint64_t m_generation;
    std::string m_sourceIdentity;
    MediaTimeMappingConfidence m_confidence;
};

class MediaMappedTimestamp final {
public:
    MediaRunningTime presentationTime() const noexcept { return m_presentationTime; }
    const std::optional<MediaRunningTime>& decodeTime() const noexcept
    {
        return m_decodeTime;
    }
    const std::optional<MediaRunningTime>& duration() const noexcept
    {
        return m_duration;
    }
    std::uint64_t generation() const noexcept { return m_generation; }
    const std::string& sourceIdentity() const noexcept { return m_sourceIdentity; }
    MediaTimeMappingConfidence confidence() const noexcept { return m_confidence; }

private:
    friend class MediaCanonicalTimeMapper;

    MediaMappedTimestamp(MediaRunningTime presentationTime,
                         std::optional<MediaRunningTime> decodeTime,
                         std::optional<MediaRunningTime> duration,
                         std::uint64_t generation,
                         std::string sourceIdentity,
                         MediaTimeMappingConfidence confidence)
        : m_presentationTime(presentationTime)
        , m_decodeTime(decodeTime)
        , m_duration(duration)
        , m_generation(generation)
        , m_sourceIdentity(std::move(sourceIdentity))
        , m_confidence(confidence)
    {
    }

    MediaRunningTime m_presentationTime;
    std::optional<MediaRunningTime> m_decodeTime;
    std::optional<MediaRunningTime> m_duration;
    std::uint64_t m_generation;
    std::string m_sourceIdentity;
    MediaTimeMappingConfidence m_confidence;
};

} // namespace media::ffmpeg::graph
