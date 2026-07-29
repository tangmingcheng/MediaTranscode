#pragma once

#include "internal/graph/protocol/mpegts/MediaTsAccessUnitView.h"
#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h"

#include <memory>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsMuxSession final {
public:
    struct Binding final {
        MediaTsMuxPlan plan;
        MediaPlaybackEpoch epoch;
        MediaTsMaterializedVideoConfig video;
        MediaTsMaterializedAudioConfig audio;
        std::unique_ptr<MediaOutputByteSink> sink;
    };
    struct AdvanceResult final {
        MediaRunningTime nextDeadline;
        std::size_t packetsWritten;
    };
    static ::media::Result<std::unique_ptr<MediaTsMuxSession>> create(Binding binding);
    ~MediaTsMuxSession();

    ::media::Status start(MediaRunningTime emitOnMaster);
    ::media::Result<AdvanceResult> poll(MediaRunningTime masterNow);
    ::media::Result<AdvanceResult> advanceThrough(MediaRunningTime emitOnMaster);
    ::media::Result<AdvanceResult> writeAccessUnit(
        const MediaTsAccessUnitView& unit);
    ::media::Status finish();
    void abort() noexcept;

private:
    enum class State : std::uint8_t { Created, Open, Finished, Poisoned };

    MediaTsMuxSession(Binding binding,
                      MediaTsOutputClockGenerator clock,
                      MediaTsTransportPacketizer packetizer,
                      MediaTsProgramTables tables,
                      MediaTsPacketBatchWriter writer) noexcept;
    ::media::Result<std::size_t> writeTables();
    ::media::Status poison(::media::ErrorInfo error);
    ::media::Status stateFailure(const char* action);
    ::media::Result<AdvanceResult> advanceFailure(::media::ErrorInfo error);

    MediaTsMuxPlan m_plan;
    MediaPlaybackEpoch m_epoch;
    MediaTsMaterializedVideoConfig m_video;
    MediaTsMaterializedAudioConfig m_audio;
    MediaTsOutputClockGenerator m_clock;
    MediaTsTransportPacketizer m_packetizer;
    MediaTsProgramTables m_tables;
    MediaTsPacketBatchWriter m_writer;
    State m_state = State::Created;
    std::optional<::media::ErrorInfo> m_failure;
    std::optional<MediaRunningTime> m_lastAdvance;
    std::optional<MediaRunningTime> m_lastAccessUnitEmission;
    MediaRunningTime m_nextPsi;
    MediaRunningTime m_nextPcr;
    std::vector<std::uint8_t> m_videoFramingWorkspace;
    std::vector<std::uint8_t> m_audioFramingWorkspace;
};

} // namespace media::ffmpeg::graph
