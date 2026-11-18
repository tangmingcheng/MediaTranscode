#pragma once
#include "media_transcode/Result.h"
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#include "internal/graph/planner/MediaPlanningCatalog.h"

namespace media::ffmpeg::graph {
// Connection declarations are identical in both modes. Rebinding preserves
// their order and identity, changing only the already-planned edge policy.
// On failure the caller must discard the partially updated candidate.
template<class Connection, bool Updating> class MediaConnectionPlanWriter final {
    using Target = std::conditional_t<Updating, std::span<Connection>, MediaPlanningCatalog<Connection>&>;
public:
    explicit MediaConnectionPlanWriter(Target target) : m_target(target) {}
    void reject(::media::ErrorInfo error) { if (!m_error) m_error.emplace(std::move(error)); }
    void append(const Connection& value) {
        if (m_error) return;
        if constexpr (Updating) {
            if (m_position >= m_target.size()) {
                reject(::media::ErrorInfo::invalidArgument("Connection catalog gained an edge"));
                return;
            }
            auto& old = m_target[m_position];
            bool same = old.from == value.from && old.to == value.to && old.output == value.output &&
                old.input == value.input && old.payload == value.payload && old.stream == value.stream &&
                old.label == value.label;
            if constexpr (requires { value.phase; }) same = same && old.phase == value.phase;
            if (!same) {
                reject(::media::ErrorInfo::invalidArgument("Connection catalog changed edge identity"));
                return;
            }
            old.policy = value.policy;
        } else m_target.push_back(value);
        ++m_position;
    }
    ::media::Status finish() const {
        if (m_error) return ::media::Status::failure(*m_error);
        if (m_position != m_target.size()) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Connection catalog lost an edge"));
        return ::media::Status::success();
    }
private:
    Target m_target;
    std::size_t m_position = 0;
    std::optional<::media::ErrorInfo> m_error;
};
} // namespace media::ffmpeg::graph
