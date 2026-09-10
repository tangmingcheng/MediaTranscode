#pragma once

#include "internal/graph/time/MediaMasterClock.h"
#include <chrono>
#include <memory>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaSteadyClockDomain final {
    explicit MediaSteadyClockDomain(std::chrono::steady_clock::time_point value) noexcept
        : anchor(value) {}
    const std::chrono::steady_clock::time_point anchor;
};

// Equality denotes the same owned clock instance, not coincident timestamps
// or optional NTP correlation. Keeping the identity also retains its domain.
class MediaClockDomainIdentity final {
public:
    explicit MediaClockDomainIdentity(std::shared_ptr<const MediaMasterClock> master)
        : m_domain(std::move(master)) {}
    explicit MediaClockDomainIdentity(std::shared_ptr<const MediaSteadyClockDomain> steady)
        : m_domain(std::move(steady)) {}
    bool valid() const noexcept {
        return std::visit([](const auto& value) { return bool(value); }, m_domain);
    }
    friend bool operator==(const MediaClockDomainIdentity&, const MediaClockDomainIdentity&) = default;
private:
    std::variant<std::shared_ptr<const MediaMasterClock>,
                 std::shared_ptr<const MediaSteadyClockDomain>> m_domain;
};

} // namespace media::ffmpeg::graph
