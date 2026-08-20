#pragma once

#include "internal/graph/protocol/mpegts/MediaTsAccessUnitView.h"
#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"
#include "internal/graph/protocol/mpegts/MediaTsPendingEmission.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/diagnostics/MediaTsEmissionDiagnostics.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuilder.h"

#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsMuxSession final {
public:
    struct VideoOnlyStreams final {
        MediaTsMaterializedVideoConfig video;
    };
    struct AudioVideoStreams final {
        MediaTsMaterializedVideoConfig video;
        MediaTsMaterializedAudioConfig audio;
    };
    using MaterializedStreams = std::variant<
        VideoOnlyStreams,
        AudioVideoStreams>;

    struct Binding final {
        MediaTsMuxPlan plan;
        MediaTsDatagramEmissionPlan emission;
        MediaProtocolOutputActivation activation;
        std::shared_ptr<const MediaMasterClock> masterClock;
        MaterializedStreams streams;
        std::unique_ptr<MediaTsDatagramSink> sink;
        std::shared_ptr<MediaScheduledDatagramBatchBuilder> scheduledBatch;
        bool startsWithDiscontinuity;
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
        const MediaTsAccessUnitView& unit,
        MediaRunningTime actualMasterNow);
    ::media::Status finish();
    void abort() noexcept;
    bool hasPendingEmission() const noexcept;
    bool hasScheduledBatch() const noexcept;
    ::media::Result<MediaBufferRef> takeScheduledBatch();

private:
    enum class State : std::uint8_t { Created, Open, Finished, Poisoned };

    MediaTsMuxSession(Binding binding,
                      MediaTsOutputClockGenerator clock,
                      MediaTsTransportPacketizer packetizer,
                      MediaTsProgramTables tables,
                      MediaTsPacketBatchWriter writer) noexcept;
    ::media::Result<std::size_t> writeDueMaintenance(
        bool psiDue,
        bool pcrDue,
        MediaRunningTime deadline,
        MediaRunningTime availableThrough);
    ::media::Result<std::size_t> writeCursorThrough(
        MediaTsPacketCursor& cursor,
        MediaRunningTime availableThrough);
    ::media::Result<AdvanceResult> emitPendingThrough(
        MediaRunningTime masterNow);
    ::media::Result<std::size_t> preparePendingDatagram();
    ::media::Result<AdvanceResult> advanceThroughAvailable(
        MediaRunningTime emitOnMaster,
        MediaRunningTime availableThrough);
    ::media::Status completePending();
    ::media::Status poison(::media::ErrorInfo error);
    ::media::Status stateFailure(const char* action);
    ::media::Result<AdvanceResult> advanceFailure(::media::ErrorInfo error);
    void logEmissionProgress(bool force = false);
    void logEmissionFinal(const char* exitReason);

    MediaTsMuxPlan m_plan;
    MediaTsDatagramEmissionPlan m_emissionPlan;
    MediaProtocolOutputActivation m_activation;
    std::shared_ptr<const MediaMasterClock> m_masterClock;
    MaterializedStreams m_streams;
    MediaTsOutputClockGenerator m_clock;
    MediaTsTransportPacketizer m_packetizer;
    MediaTsProgramTables m_tables;
    MediaTsPacketBatchWriter m_writer;
    std::shared_ptr<MediaScheduledDatagramBatchBuilder> m_scheduledBatch;
    std::optional<MediaTsDatagramEmissionSchedule> m_emissionSchedule;
    std::optional<MediaTsPendingEmission> m_pendingEmission;
    MediaTsEmissionDiagnostics m_emissionDiagnostics;
    bool m_emissionFinalLogged = false;
    MediaGraphDiagnosticSampler m_emissionDiagnosticSampler{
        MediaGraphDiagnosticLevel::Flow};
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
