#pragma once

#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>

namespace media::ffmpeg::graph {

struct MediaTsOutputClockPolicy final {
    MediaRunningTime pcrInterval;
    MediaRunningTime maximumPcrGap;
    MediaRunningTime maximumPcrJitter;
    int timestampTimeBaseNumerator;
    int timestampTimeBaseDenominator;
};

struct MediaTsPacketClock final {
    std::int64_t extendedPts;
    std::int64_t extendedDts;
    std::uint64_t wirePts;
    std::uint64_t wireDts;
};

struct MediaTsPcrClock final {
    std::uint64_t generation;
    MediaRunningTime masterTime;
    std::int64_t extended27Mhz;
    std::uint64_t wire27Mhz;
};

struct MediaTsOutputClockControlState;

class MediaTsPreparedPacketClock final {
public:
    ~MediaTsPreparedPacketClock();
    MediaTsPreparedPacketClock(const MediaTsPreparedPacketClock&) = delete;
    MediaTsPreparedPacketClock& operator=(const MediaTsPreparedPacketClock&) = delete;
    MediaTsPreparedPacketClock(MediaTsPreparedPacketClock&& other) noexcept;
    MediaTsPreparedPacketClock& operator=(MediaTsPreparedPacketClock&& other) noexcept;

    const MediaTsPacketClock& clock() const noexcept { return m_clock; }

private:
    friend class MediaTsOutputClockGenerator;
    MediaTsPreparedPacketClock(
        std::weak_ptr<MediaTsOutputClockControlState> owner,
        std::uint64_t revision,
        MediaScheduledStream stream,
        MediaTsPacketClock clock) noexcept;
    void cancel() noexcept;

    std::weak_ptr<MediaTsOutputClockControlState> m_owner;
    std::uint64_t m_revision = 0;
    MediaScheduledStream m_stream = MediaScheduledStream::Video;
    MediaTsPacketClock m_clock{};
    bool m_valid = false;
};

class MediaTsPreparedPcrClock final {
public:
    ~MediaTsPreparedPcrClock();
    MediaTsPreparedPcrClock(const MediaTsPreparedPcrClock&) = delete;
    MediaTsPreparedPcrClock& operator=(const MediaTsPreparedPcrClock&) = delete;
    MediaTsPreparedPcrClock(MediaTsPreparedPcrClock&& other) noexcept;
    MediaTsPreparedPcrClock& operator=(MediaTsPreparedPcrClock&& other) noexcept;

    const MediaTsPcrClock& clock() const noexcept { return m_clock; }

private:
    friend class MediaTsOutputClockGenerator;
    MediaTsPreparedPcrClock(
        std::weak_ptr<MediaTsOutputClockControlState> owner,
        std::uint64_t revision,
        MediaTsPcrClock clock) noexcept;
    void cancel() noexcept;

    std::weak_ptr<MediaTsOutputClockControlState> m_owner;
    std::uint64_t m_revision = 0;
    MediaTsPcrClock m_clock{
        0, MediaRunningTime::fromNanoseconds(0), 0, 0};
    bool m_valid = false;
};

class MediaTsOutputClockGenerator final {
public:
    static ::media::Result<MediaTsOutputClockGenerator> create(
        MediaTsOutputClockPolicy policy,
        MediaPlaybackEpoch epoch);

    MediaTsOutputClockGenerator(const MediaTsOutputClockGenerator&) = delete;
    MediaTsOutputClockGenerator& operator=(const MediaTsOutputClockGenerator&) = delete;
    MediaTsOutputClockGenerator(MediaTsOutputClockGenerator&&) noexcept = default;
    MediaTsOutputClockGenerator& operator=(MediaTsOutputClockGenerator&&) noexcept = default;

    ::media::Result<MediaTsPreparedPacketClock> preparePacket(
        std::uint64_t generation,
        MediaScheduledStream stream,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster,
        MediaRunningTime emitOnMaster,
        MediaRunningTime transportDecodeLead);
    ::media::Status commitPacket(MediaTsPreparedPacketClock&& prepared);
    ::media::Result<MediaTsPreparedPcrClock> preparePcr(
        std::uint64_t generation,
        MediaRunningTime exactDeadline);
    ::media::Status commitPcr(MediaTsPreparedPcrClock&& prepared);
    ::media::Status validateSerializedPcr(
        const MediaTsPcrClock& planned,
        std::int64_t serializedExtended27Mhz) const;

    const MediaTsOutputClockPolicy& policy() const noexcept { return m_policy; }
    const MediaPlaybackEpoch& epoch() const noexcept { return m_epoch; }

private:
    MediaTsOutputClockGenerator(
        MediaTsOutputClockPolicy policy,
        MediaPlaybackEpoch epoch);

    ::media::Result<std::int64_t> outputNanoseconds(
        MediaRunningTime masterTime) const;
    ::media::Result<std::int64_t> timestampTicks(
        MediaRunningTime masterTime) const;
    ::media::Result<std::int64_t> pcrTicks(
        MediaRunningTime masterTime) const;
    ::media::Status validateGeneration(std::uint64_t generation) const;

    MediaTsOutputClockPolicy m_policy;
    MediaPlaybackEpoch m_epoch;
    std::shared_ptr<MediaTsOutputClockControlState> m_control;
};

} // namespace media::ffmpeg::graph
