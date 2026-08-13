#pragma once

#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace media::ffmpeg::graph {

class MediaTsDatagramEmissionScheduleState;

class MediaTsPreparedDatagramEmission final {
public:
    ~MediaTsPreparedDatagramEmission();
    MediaTsPreparedDatagramEmission(
        MediaTsPreparedDatagramEmission&& other) noexcept;
    MediaTsPreparedDatagramEmission& operator=(
        MediaTsPreparedDatagramEmission&& other) noexcept;
    MediaTsPreparedDatagramEmission(
        const MediaTsPreparedDatagramEmission&) = delete;
    MediaTsPreparedDatagramEmission& operator=(
        const MediaTsPreparedDatagramEmission&) = delete;

    MediaRunningTime deadline() const noexcept;
    MediaRunningTime plannedWait() const noexcept;
    std::size_t wireBytes() const noexcept;

private:
    friend class MediaTsDatagramEmissionSchedule;

    MediaTsPreparedDatagramEmission(
        std::shared_ptr<MediaTsDatagramEmissionScheduleState> state,
        std::uint64_t revision,
        MediaRunningTime deadline,
        MediaRunningTime plannedWait,
        MediaRunningTime nextVirtualFinish,
        std::size_t wireBytes) noexcept;

    void cancel() noexcept;

    std::shared_ptr<MediaTsDatagramEmissionScheduleState> m_state;
    std::uint64_t m_revision = 0;
    MediaRunningTime m_deadline =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_plannedWait =
        MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_nextVirtualFinish =
        MediaRunningTime::fromNanoseconds(0);
    std::size_t m_wireBytes = 0;
    bool m_active = false;
};

class MediaTsDatagramEmissionSchedule final {
public:
    static ::media::Result<MediaTsDatagramEmissionSchedule> create(
        MediaTsDatagramEmissionPlan plan,
        MediaRunningTime origin);

    MediaTsDatagramEmissionSchedule(
        MediaTsDatagramEmissionSchedule&&) noexcept = default;
    MediaTsDatagramEmissionSchedule& operator=(
        MediaTsDatagramEmissionSchedule&&) noexcept = default;
    MediaTsDatagramEmissionSchedule(
        const MediaTsDatagramEmissionSchedule&) = delete;
    MediaTsDatagramEmissionSchedule& operator=(
        const MediaTsDatagramEmissionSchedule&) = delete;

    ::media::Result<MediaTsPreparedDatagramEmission> prepare(
        std::size_t payloadBytes,
        MediaRunningTime notBefore);
    ::media::Status commit(MediaTsPreparedDatagramEmission&& prepared);
    const MediaTsDatagramEmissionPlan& plan() const noexcept;

private:
    explicit MediaTsDatagramEmissionSchedule(
        std::shared_ptr<MediaTsDatagramEmissionScheduleState> state) noexcept;

    std::shared_ptr<MediaTsDatagramEmissionScheduleState> m_state;
};

} // namespace media::ffmpeg::graph
