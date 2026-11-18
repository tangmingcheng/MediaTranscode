#pragma once

#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"

#include <algorithm>
#include <sstream>
#include <type_traits>

namespace media::ffmpeg::graph {

// Borrowed snapshots are inspected synchronously, before either store is cleared.
// Frontier extrema describe individual candidates, not one continuous window.
inline std::string describeMediaAvStartupExpiry(
    const MediaAvStartupConfig& config,
    MediaAvStartupPresentationSnapshot video,
    MediaAvStartupPresentationSnapshot audio)
{
    std::ostringstream out;
    out << " video_identity=" << config.videoIdentity
        << " video_readiness=" << static_cast<int>(config.videoReadiness)
        << " require_key_frame=" << config.requireVideoKeyFrame
        << " preroll_ns=" << config.preroll.nanoseconds()
        << " maximum_gap_ns=" << config.maximumGap.nanoseconds()
        << " maximum_wait_ns=" << config.maximumWait.nanoseconds()
        << " key_frame_wait_ns=" << config.keyFrameWait.nanoseconds();
    if (config.audio) {
        out << " audio_identity=" << config.audio->identity
            << " audio_maximum_initial_skew_ns=" << config.audio->maximumInitialSkew.nanoseconds()
            << " audio_trim_to_common_start=" << config.audio->trimToCommonStart
            << " audio_maximum_trim_ns=" << config.audio->maximumTrim.nanoseconds();
    }
    const auto appendSnapshot = [&out](const char* name,
                                      MediaAvStartupPresentationSnapshot snapshot) {
        out << ' ' << name << "_count=" << snapshot.size();
        if (snapshot.empty()) return;
        out << ' ' << name << "_first_pts_ns=" << snapshot.front().unit->presentationTime->nanoseconds()
            << ' ' << name << "_last_pts_ns=" << snapshot.back().unit->presentationTime->nanoseconds();
        std::optional<std::int64_t> minimum;
        std::optional<std::int64_t> maximum;
        std::size_t keyFrames = 0;
        for (const auto& candidate : snapshot) {
            const auto frontier = std::visit([](const auto& evidence) {
                using T = std::decay_t<decltype(evidence)>;
                if constexpr (std::is_same_v<T, MediaAvStartupIntervalFrontier>)
                    return evidence.end.nanoseconds();
                else return evidence.lastPresentation.nanoseconds();
            }, candidate.readiness);
            minimum = minimum ? std::min(*minimum, frontier) : frontier;
            maximum = maximum ? std::max(*maximum, frontier) : frontier;
            if (candidate.unit->keyFrame) ++keyFrames;
        }
        out << ' ' << name << "_candidate_frontier_min_ns=" << *minimum
            << ' ' << name << "_candidate_frontier_max_ns=" << *maximum
            << ' ' << name << "_key_frame_count=" << keyFrames;
    };
    appendSnapshot("video", video);
    appendSnapshot("audio", audio);
    return out.str();
}

} // namespace media::ffmpeg::graph
