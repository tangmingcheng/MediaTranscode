#pragma once
#include "internal/graph/diagnostics/MediaAccessUnitProcessingTrace.h"

#include "internal/graph/protocol/mpegts/MediaTsAccessUnitView.h"
#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"
#include "internal/graph/protocol/mpegts/MediaTsPendingEmission.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/diagnostics/MediaTsEmissionDiagnostics.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"

#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/planner/realtime/MediaTsPrefixServicePlan.h"
#include <memory>
#include <array>
#include <optional>
#include <variant>
#include <utility>
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
        bool startsWithDiscontinuity;
        std::shared_ptr<MediaWireGlobalSequenceState> transport;
        std::shared_ptr<MediaNodeWakeup> producerWakeup;
        MediaTsPrefixServicePlan prefixService;
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
        MediaRunningTime actualMasterNow,
        const MediaAccessUnitMuxObservation& observation);
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
                      MediaFixedCapacityStorage<MediaBufferRef> protocolBatches) noexcept;
    ::media::Result<std::size_t> writeDueMaintenance(
        bool psiDue,
        bool pcrDue,
        MediaRunningTime deadline,
        MediaRunningTime availableThrough);
    ::media::Result<std::array<MediaTsMaintenanceServiceObligation, 2>>
    maintenanceObligations(MediaRunningTime nextPcr, MediaRunningTime nextPsi) const;
    ::media::Result<MediaTsPublishedWorkService> acceptedWorkObservation(MediaRunningTime observedNow);
    ::media::Result<std::size_t> writeCursorThrough(
        MediaTsPacketCursor& cursor,
        MediaRunningTime availableThrough,
        MediaRunningTime completionLimit);
    ::media::Result<AdvanceResult> emitPendingThrough(
        MediaRunningTime masterNow);
    ::media::Result<std::size_t> preparePendingPrefix(
        const MediaTsPrefixBatch& prefix);
    ::media::Status validateProtocolBatchCapacity(std::uint64_t bytes) const;
    ::media::Result<AdvanceResult> advanceThroughAvailable(
        MediaRunningTime emitOnMaster,
        MediaRunningTime availableThrough);
    ::media::Result<std::size_t> materializeMaintenanceThrough(
        MediaRunningTime deadline,
        MediaRunningTime availableThrough);
    ::media::Status completePending();
    ::media::Status poison(::media::ErrorInfo error);
    ::media::Status stateFailure(const char* action);
    ::media::Result<AdvanceResult> advanceFailure(::media::ErrorInfo error);
    void logEmissionProgress(bool force = false);
    void logEmissionFinal(const char* exitReason);
    void logAccessUnitObservations(bool aborted) noexcept;

    MediaTsMuxPlan m_plan;
    MediaTsDatagramEmissionPlan m_emissionPlan;
    MediaProtocolOutputActivation m_activation;
    std::shared_ptr<const MediaMasterClock> m_masterClock;
    std::shared_ptr<MediaWireGlobalSequenceState> m_transport;
    std::shared_ptr<MediaNodeWakeup> m_producerWakeup;
    MediaTsPrefixServicePlan m_prefixService;
    MaterializedStreams m_streams;
    MediaTsOutputClockGenerator m_clock;
    MediaTsTransportPacketizer m_packetizer;
    MediaTsProgramTables m_tables;
    MediaFixedCapacityStorage<MediaBufferRef> m_protocolBatches;
    std::uint64_t m_protocolBatchBytes = 0;
    std::optional<MediaTsDatagramEmissionSchedule> m_emissionSchedule;
    std::optional<MediaTsPendingEmission> m_pendingEmission;
    std::optional<MediaAccessUnitMuxObservation> m_pendingObservation;
    std::optional<std::pair<MediaAccessUnitMuxObservation, MediaTsPrefixCommitObservation>> m_previousCompletedObservation;
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
