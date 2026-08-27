#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"

#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsVideoAccessUnitFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportEmissionOrigin.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

bool materializedConfigMatches(const MediaTsMuxSession::Binding& binding) noexcept
{
    const auto& parameters = binding.plan.parameters();
    if (const auto* video = std::get_if<
            MediaTsMuxSession::VideoOnlyStreams>(&binding.streams)) {
        return binding.plan.videoOnlyProgram() &&
            video->video.contract() == parameters.video;
    }
    const auto* streams = std::get_if<
        MediaTsMuxSession::AudioVideoStreams>(&binding.streams);
    const auto* program = binding.plan.audioVideoProgram();
    return streams && program &&
        streams->video.contract() == parameters.video &&
        streams->audio.audioObjectType() == program->aac.audioObjectType &&
        streams->audio.samplingFrequencyIndex() ==
            program->aac.samplingFrequencyIndex &&
        streams->audio.channelConfiguration() ==
            program->aac.channelConfiguration;
}

::media::Result<std::size_t> checkedPacketCount(
    std::size_t current,
    std::size_t added)
{
    if (current > std::numeric_limits<std::size_t>::max() - added) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS mux session packet count overflow"));
    }
    return ::media::Result<std::size_t>::success(current + added);
}

struct MaintenanceGroupGeometry final {
    std::size_t payloadBytes;
    std::size_t datagramCount;
};

::media::Result<MaintenanceGroupGeometry> maintenanceGroupGeometry(
    const std::vector<std::size_t>& cursorPacketCounts,
    std::size_t packetSize,
    std::size_t maximumPacketsPerDatagram)
{
    if (cursorPacketCounts.size() == 0 || packetSize == 0 ||
        maximumPacketsPerDatagram == 0) {
        return ::media::Result<MaintenanceGroupGeometry>::failure(
            invalid("MPEG-TS maintenance group geometry is incomplete"));
    }
    std::size_t totalPackets = 0;
    std::size_t datagrams = 0;
    for (const std::size_t packets : cursorPacketCounts) {
        if (packets == 0 ||
            totalPackets > (std::numeric_limits<std::size_t>::max)() - packets) {
            return ::media::Result<MaintenanceGroupGeometry>::failure(
                invalid("MPEG-TS maintenance packet count is not representable"));
        }
        totalPackets += packets;
        const std::size_t cursorDatagrams =
            packets / maximumPacketsPerDatagram +
            (packets % maximumPacketsPerDatagram != 0 ? 1u : 0u);
        if (datagrams > (std::numeric_limits<std::size_t>::max)() -
                cursorDatagrams) {
            return ::media::Result<MaintenanceGroupGeometry>::failure(
                invalid("MPEG-TS maintenance datagram count is not representable"));
        }
        datagrams += cursorDatagrams;
    }
    if (totalPackets > (std::numeric_limits<std::size_t>::max)() /
            packetSize) {
        return ::media::Result<MaintenanceGroupGeometry>::failure(
            invalid("MPEG-TS maintenance payload bytes are not representable"));
    }
    return ::media::Result<MaintenanceGroupGeometry>::success(
        MaintenanceGroupGeometry{totalPackets * packetSize, datagrams});
}

} // namespace

::media::Result<std::unique_ptr<MediaTsMuxSession>> MediaTsMuxSession::create(
    Binding binding)
{
    auto expectedEmission = MediaTsDatagramEmissionPlan::create(
        binding.plan,
        binding.emission.videoInitialServiceWindow(),
        binding.emission.audioInitialServiceWindow(),
        binding.emission.maximumQueuedBytes(),
        binding.emission.targetServiceResidence());
    if (!binding.masterClock ||
        binding.activation.generation == 0 ||
        !materializedConfigMatches(binding) ||
        !expectedEmission || binding.emission != expectedEmission.value()) {
        return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
            invalid("MPEG-TS mux session binding is incomplete or inconsistent"));
    }
    auto pcrOrigin = mediaTsTransportEmissionOrigin(
        binding.plan, binding.activation);
    if (!pcrOrigin) {
        return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
            pcrOrigin.error());
    }
    auto clock = MediaTsOutputClockGenerator::create(
        binding.plan.clockPolicy(), binding.activation,
        pcrOrigin.value());
    if (!clock) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        clock.error());
    auto packetizer = MediaTsTransportPacketizer::create(
        binding.plan, binding.startsWithDiscontinuity);
    if (!packetizer) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        packetizer.error());
    auto tables = MediaTsPsiSerializer::serialize(binding.plan);
    if (!tables) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        tables.error());
    return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::success(
        std::unique_ptr<MediaTsMuxSession>(new MediaTsMuxSession(
            std::move(binding), std::move(clock).value(),
            std::move(packetizer).value(), std::move(tables).value())));
}

MediaTsMuxSession::MediaTsMuxSession(
    Binding binding,
    MediaTsOutputClockGenerator clock,
    MediaTsTransportPacketizer packetizer,
    MediaTsProgramTables tables) noexcept
    : m_plan(std::move(binding.plan)),
      m_emissionPlan(binding.emission),
      m_activation(binding.activation),
      m_masterClock(std::move(binding.masterClock)),
      m_streams(std::move(binding.streams)),
      m_clock(std::move(clock)),
      m_packetizer(std::move(packetizer)),
      m_tables(std::move(tables)),
      m_nextPsi(binding.activation.masterRelease),
      m_nextPcr(binding.activation.masterRelease)
{
}

MediaTsMuxSession::~MediaTsMuxSession()
{
    abort();
}

::media::Status MediaTsMuxSession::stateFailure(const char* action)
{
    if (m_failure) return ::media::Status::failure(*m_failure);
    return poison(invalid(
        (std::string("MPEG-TS mux session cannot ") + action +
         " in its current state").c_str()));
}

::media::Status MediaTsMuxSession::poison(::media::ErrorInfo error)
{
    if (!m_failure) m_failure = std::move(error);
    m_state = State::Poisoned;
    logEmissionFinal(m_failure->describe().c_str());
    return ::media::Status::failure(*m_failure);
}

::media::Result<MediaTsMuxSession::AdvanceResult>
MediaTsMuxSession::advanceFailure(::media::ErrorInfo error)
{
    poison(std::move(error));
    return ::media::Result<AdvanceResult>::failure(*m_failure);
}

::media::Result<std::size_t> MediaTsMuxSession::writeCursorThrough(
    MediaTsPacketCursor& cursor,
    MediaRunningTime availableThrough)
{
    if (!m_emissionSchedule) {
        return ::media::Result<std::size_t>::failure(
            poison(invalid("MPEG-TS emission schedule is not active")).error());
    }
    const std::size_t packetCount = cursor.remainingPacketCount();
    const std::size_t packetSize = m_emissionPlan.packetSizeBytes();
    if (packetSize == 0 || packetCount == 0 ||
        packetCount > (std::numeric_limits<std::size_t>::max)() / packetSize) {
        return ::media::Result<std::size_t>::failure(
            poison(invalid("MPEG-TS protocol batch geometry is not representable")).error());
    }
    auto emission = m_emissionSchedule->prepareMaintenance(
        packetCount * packetSize);
    if (!emission) {
        return ::media::Result<std::size_t>::failure(
            poison(emission.error()).error());
    }
    const MediaRunningTime release = emission.value().deadline();
    const MediaRunningTime deadline = emission.value().latestEmissionTime();
    auto batch = MediaMpegTsProtocolDatagramBatchBuffer::create(
        m_activation.generation, std::move(cursor),
        m_plan.parameters().maximumPacketsPerDatagram,
        availableThrough, release, deadline);
    if (!batch) {
        return ::media::Result<std::size_t>::failure(
            poison(batch.error()).error());
    }
    auto committed = m_emissionSchedule->commit(
        std::move(emission).value(), release);
    if (!committed) {
        return ::media::Result<std::size_t>::failure(
            poison(committed.error()).error());
    }
    m_protocolBatches.push_back(std::move(batch).value());
    logEmissionProgress();
    return ::media::Result<std::size_t>::success(packetCount);
}

::media::Result<std::size_t> MediaTsMuxSession::writeDueMaintenance(
    bool psiDue,
    bool pcrDue,
    MediaRunningTime deadline,
    MediaRunningTime availableThrough)
{
    if (!psiDue && !pcrDue) {
        return ::media::Result<std::size_t>::failure(
            poison(invalid("MPEG-TS maintenance group has no due member")).error());
    }
    std::optional<MediaTsPreparedPcrClock> preparedPcr;
    std::vector<std::size_t> packetCounts;
    std::optional<MediaRunningTime> completionDeadline;
    if (psiDue) {
        auto patPackets = m_packetizer.patPacketCount(m_tables.pat());
        auto pmtPackets = m_packetizer.pmtPacketCount(m_tables.pmt());
        if (!patPackets || !pmtPackets) {
            return ::media::Result<std::size_t>::failure(
                poison(!patPackets
                    ? patPackets.error()
                    : pmtPackets.error()).error());
        }
        packetCounts.push_back(patPackets.value());
        packetCounts.push_back(pmtPackets.value());
        auto psiCompletion = deadline.checkedAdd(
            m_plan.timingPolicy().psiRepeatInterval().value);
        if (!psiCompletion) {
            return ::media::Result<std::size_t>::failure(
                poison(psiCompletion.error()).error());
        }
        completionDeadline = psiCompletion.value();
    }
    if (pcrDue) {
        auto pcr = m_clock.preparePcr(
            m_activation.generation, deadline);
        if (!pcr) {
            return ::media::Result<std::size_t>::failure(
                poison(pcr.error()).error());
        }
        const auto& sample = pcr.value().clock();
        if (auto status = m_clock.validateSerializedPcr(
                sample, sample.extended27Mhz); !status) {
            return ::media::Result<std::size_t>::failure(
                poison(status.error()).error());
        }
        auto pcrPackets = m_packetizer.pcrOnlyPacketCount(sample);
        if (!pcrPackets) {
            return ::media::Result<std::size_t>::failure(
                poison(pcrPackets.error()).error());
        }
        packetCounts.push_back(pcrPackets.value());
        auto pcrCompletion = deadline.checkedAdd(
            m_plan.clockPolicy().pcrInterval);
        if (!pcrCompletion) {
            return ::media::Result<std::size_t>::failure(
                poison(pcrCompletion.error()).error());
        }
        completionDeadline = completionDeadline
            ? (std::min)(*completionDeadline, pcrCompletion.value())
            : pcrCompletion.value();
        preparedPcr.emplace(std::move(pcr).value());
    }
    auto geometry = maintenanceGroupGeometry(
        packetCounts, m_emissionPlan.packetSizeBytes(),
        m_plan.parameters().maximumPacketsPerDatagram);
    if (!geometry || !completionDeadline) {
        return ::media::Result<std::size_t>::failure(
            poison(geometry
                ? invalid("MPEG-TS maintenance completion deadline is absent")
                : geometry.error()).error());
    }
    if (auto status = m_emissionSchedule->beginMaintenanceGroup(
            geometry.value().payloadBytes, geometry.value().datagramCount,
            deadline, *completionDeadline); !status) {
        return ::media::Result<std::size_t>::failure(
            poison(status.error()).error());
    }
    std::size_t packetsWritten = 0;
    const auto writeMaintenanceCursor = [this, availableThrough, &packetsWritten](
        ::media::Result<MediaTsPacketCursor> cursorResult) -> ::media::Status {
        if (!cursorResult) {
            return ::media::Status::failure(cursorResult.error());
        }
        MediaTsPacketCursor cursor = std::move(cursorResult).value();
        auto written = writeCursorThrough(cursor, availableThrough);
        if (!written) {
            return ::media::Status::failure(written.error());
        }
        auto total = checkedPacketCount(packetsWritten, written.value());
        if (!total) {
            return ::media::Status::failure(total.error());
        }
        packetsWritten = total.value();
        return ::media::Status::success();
    };
    if (psiDue) {
        if (auto status = writeMaintenanceCursor(
                m_packetizer.beginPat(m_tables.pat())); !status) {
            return ::media::Result<std::size_t>::failure(
                poison(status.error()).error());
        }
        if (auto status = writeMaintenanceCursor(
                m_packetizer.beginPmt(m_tables.pmt())); !status) {
            return ::media::Result<std::size_t>::failure(
                poison(status.error()).error());
        }
    }
    if (preparedPcr) {
        if (auto status = writeMaintenanceCursor(
                m_packetizer.beginPcrOnly(preparedPcr->clock())); !status) {
            return ::media::Result<std::size_t>::failure(
                poison(status.error()).error());
        }
    }
    if (auto status = m_emissionSchedule->completeMaintenanceGroup();
        !status) {
        return ::media::Result<std::size_t>::failure(
            poison(status.error()).error());
    }
    if (preparedPcr) {
        if (auto status = m_clock.commitPcr(std::move(*preparedPcr));
            !status) {
            return ::media::Result<std::size_t>::failure(
                poison(status.error()).error());
        }
    }
    return ::media::Result<std::size_t>::success(packetsWritten);
}

::media::Status MediaTsMuxSession::start(MediaRunningTime emitOnMaster)
{
    if (m_state != State::Created) return stateFailure("start");
    auto origin = mediaTsTransportEmissionOrigin(m_plan, m_activation);
    if (!origin) return poison(origin.error());
    if (emitOnMaster != origin.value()) {
        return poison(invalid(
            "MPEG-TS mux session start must equal its planned transport emission origin"));
    }
    m_nextPsi = emitOnMaster;
    m_nextPcr = emitOnMaster;
    m_lastAdvance = emitOnMaster;
    auto schedule = MediaTsDatagramEmissionSchedule::create(
        m_emissionPlan, emitOnMaster);
    if (!schedule) return poison(schedule.error());
    m_emissionSchedule = std::move(schedule).value();
    m_emissionDiagnostics.logPlan(m_emissionPlan, m_activation.generation);
    m_state = State::Open;
    return ::media::Status::success();
}

::media::Result<MediaTsMuxSession::AdvanceResult> MediaTsMuxSession::poll(
    MediaRunningTime masterNow)
{
    if (m_state != State::Open) {
        if (m_state == State::Created) {
            auto failure = advanceFailure(invalid(
                "MPEG-TS mux session cannot poll before start"));
            return ::media::Result<AdvanceResult>::failure(failure.error());
        }
        return ::media::Result<AdvanceResult>::failure(
            m_failure ? *m_failure : invalid("MPEG-TS mux session is not open"));
    }

    if (m_pendingEmission) {
        return emitPendingThrough(masterNow);
    }

    const MediaRunningTime deadline =
        m_nextPcr < m_nextPsi ? m_nextPcr : m_nextPsi;
    if (masterNow < deadline) {
        return ::media::Result<AdvanceResult>::success(
            AdvanceResult{deadline, 0});
    }

    // Poll advances exactly one transport deadline. The caller may poll again
    // to catch up, preserving the planned PCR cadence without turning transport
    // maintenance into a second access-unit pacing authority.
    auto advanced = advanceThroughAvailable(deadline, masterNow);
    if (!advanced) {
        return ::media::Result<AdvanceResult>::failure(advanced.error());
    }
    return ::media::Result<AdvanceResult>::success(AdvanceResult{
        advanced.value().nextDeadline, advanced.value().packetsWritten});
}

::media::Status MediaTsMuxSession::completePending()
{
    if (!m_pendingEmission || !m_pendingEmission->finished()) {
        return poison(invalid(
            "MPEG-TS pending emission cannot complete before its cursor"));
    }
    auto clock = m_pendingEmission->takePacketClock();
    if (!clock) {
        return poison(invalid(
            "MPEG-TS access-unit emission lost its clock transaction"));
    }
    auto emissionCommitted = m_emissionSchedule
        ? m_emissionSchedule->completeAccessUnit()
        : ::media::Status::failure(invalid(
              "MPEG-TS access-unit emission schedule is absent"));
    if (!emissionCommitted) return poison(emissionCommitted.error());
    auto committed = m_clock.commitPacket(std::move(*clock));
    if (!committed) return poison(committed.error());
    m_emissionDiagnostics.recordAccessUnitCompleted();
    m_lastAccessUnitEmission = m_pendingEmission->notBefore();
    m_pendingEmission.reset();
    return ::media::Status::success();
}

::media::Result<MediaTsMuxSession::AdvanceResult>
MediaTsMuxSession::emitPendingThrough(
    MediaRunningTime masterNow)
{
    if (!m_pendingEmission || !m_emissionSchedule) {
        return advanceFailure(invalid(
            "MPEG-TS mux session has no pending canonical emission"));
    }
    (void)masterNow;
    std::size_t packetsWritten = 0;
    if (!m_pendingEmission->finished()) {
        auto materialized = preparePendingDatagram();
        if (!materialized) return advanceFailure(materialized.error());
        packetsWritten = materialized.value();
    }
    if (m_pendingEmission->finished()) {
        auto completed = completePending();
        if (!completed) {
            return ::media::Result<AdvanceResult>::failure(completed.error());
        }
    }
    const MediaRunningTime nextDeadline = m_pendingEmission
        ? m_pendingEmission->deadline()
        : (m_nextPcr < m_nextPsi ? m_nextPcr : m_nextPsi);
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{nextDeadline, packetsWritten});
}

::media::Result<std::size_t> MediaTsMuxSession::preparePendingDatagram()
{
    if (!m_pendingEmission || !m_emissionSchedule) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS pending datagram preparation has no active access unit"));
    }
    const std::size_t packets = m_pendingEmission->pendingBytes() /
        m_emissionPlan.packetSizeBytes();
    auto batch = m_pendingEmission->materializeProtocolBatch(
        m_activation.generation, *m_emissionSchedule);
    if (!batch) return ::media::Result<std::size_t>::failure(batch.error());
    m_protocolBatches.push_back(std::move(batch).value());
    return ::media::Result<std::size_t>::success(packets);
}

::media::Result<MediaTsMuxSession::AdvanceResult> MediaTsMuxSession::advanceThrough(
    MediaRunningTime emitOnMaster)
{
    return advanceThroughAvailable(emitOnMaster, emitOnMaster);
}

::media::Result<MediaTsMuxSession::AdvanceResult>
MediaTsMuxSession::advanceThroughAvailable(
    MediaRunningTime emitOnMaster,
    MediaRunningTime availableThrough)
{
    if (m_state != State::Open) {
        if (m_state == State::Created) {
            return advanceFailure(invalid(
                "MPEG-TS mux session cannot advance before start"));
        }
        return ::media::Result<AdvanceResult>::failure(
            m_failure ? *m_failure : invalid("MPEG-TS mux session is not open"));
    }
    if (m_lastAdvance && emitOnMaster < *m_lastAdvance) {
        return advanceFailure(invalid("MPEG-TS mux session emission time regressed"));
    }
    if (m_lastAdvance) {
        auto gap = emitOnMaster.checkedSubtract(*m_lastAdvance);
        if (!gap || gap.value() > m_plan.clockPolicy().maximumPcrGap) {
            return advanceFailure(invalid(
                "MPEG-TS mux session advance exceeded the maximum PCR gap"));
        }
    }
    auto packetCount = materializeMaintenanceThrough(
        emitOnMaster, availableThrough);
    if (!packetCount) {
        return ::media::Result<AdvanceResult>::failure(packetCount.error());
    }
    m_lastAdvance = emitOnMaster;
    const auto deadline = m_nextPcr < m_nextPsi ? m_nextPcr : m_nextPsi;
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{deadline, packetCount.value()});
}

::media::Result<std::size_t>
MediaTsMuxSession::materializeMaintenanceThrough(
    MediaRunningTime through,
    MediaRunningTime availableThrough)
{
    std::size_t packetCount = 0;
    while (m_nextPcr <= through || m_nextPsi <= through) {
        const MediaRunningTime deadline =
            m_nextPsi < m_nextPcr ? m_nextPsi : m_nextPcr;
        const bool psiDue = m_nextPsi == deadline;
        const bool pcrDue = m_nextPcr == deadline;
        auto written = writeDueMaintenance(
            psiDue, pcrDue, deadline, availableThrough);
        if (!written) {
            return ::media::Result<std::size_t>::failure(written.error());
        }
        auto count = checkedPacketCount(packetCount, written.value());
        if (!count) {
            return ::media::Result<std::size_t>::failure(count.error());
        }
        packetCount = count.value();
        if (psiDue) {
            auto next = m_nextPsi.checkedAdd(
                m_plan.timingPolicy().psiRepeatInterval().value);
            if (!next) {
                return ::media::Result<std::size_t>::failure(next.error());
            }
            m_nextPsi = next.value();
        }
        if (pcrDue) {
            auto next = m_nextPcr.checkedAdd(m_plan.clockPolicy().pcrInterval);
            if (!next) {
                return ::media::Result<std::size_t>::failure(next.error());
            }
            m_nextPcr = next.value();
        }
    }
    return ::media::Result<std::size_t>::success(packetCount);
}

::media::Result<MediaTsMuxSession::AdvanceResult>
MediaTsMuxSession::writeAccessUnit(
    const MediaTsAccessUnitView& unit,
    MediaRunningTime actualMasterNow)
{
    m_emissionDiagnostics.recordAccessUnitReady(
        actualMasterNow, unit.emitOnMaster, unit.dispatchOnMaster);
    if (m_pendingEmission) {
        return advanceFailure(invalid(
            "MPEG-TS mux session rejects a second access unit while emission is pending"));
    }
    const MediaRunningTime maintenanceAvailableThrough = (std::max)(
        actualMasterNow, unit.emitOnMaster);
    auto advanced = advanceThroughAvailable(
        unit.emitOnMaster, maintenanceAvailableThrough);
    if (!advanced) {
        return ::media::Result<AdvanceResult>::failure(advanced.error());
    }
    if (unit.generation != m_activation.generation || unit.payload.empty() ||
        (m_lastAccessUnitEmission &&
         unit.emitOnMaster < *m_lastAccessUnitEmission)) {
        return advanceFailure(
            invalid("MPEG-TS access unit identity or order is invalid"));
    }
    if (unit.stream == MediaScheduledStream::Audio && unit.randomAccess) {
        return advanceFailure(
            invalid("MPEG-TS audio access unit cannot be random access"));
    }
    auto clock = m_clock.preparePacket(
        unit.generation, unit.stream, unit.presentationOnMaster,
        unit.dispatchOnMaster, unit.emitOnMaster,
        m_plan.transportDecodeLead());
    if (!clock) return advanceFailure(clock.error());

    auto framed = [&]() -> ::media::Result<std::span<const std::uint8_t>> {
        switch (unit.stream) {
        case MediaScheduledStream::Video:
            {
            const MediaTsMaterializedVideoConfig* video = nullptr;
            if (const auto* streams = std::get_if<VideoOnlyStreams>(
                    &m_streams)) {
                video = &streams->video;
            } else if (const auto* streams = std::get_if<AudioVideoStreams>(
                           &m_streams)) {
                video = &streams->video;
            }
            if (!video) {
                return ::media::Result<std::span<const std::uint8_t>>::failure(
                    invalid("MPEG-TS video materialization is absent"));
            }
            return MediaTsVideoAccessUnitFramer::frame(
                m_plan, *video, unit.payload, unit.randomAccess,
                m_videoFramingWorkspace);
            }
        case MediaScheduledStream::Audio:
            {
            const auto* streams = std::get_if<AudioVideoStreams>(&m_streams);
            const auto* program = m_plan.audioVideoProgram();
            if (!streams || !program) {
                return ::media::Result<std::span<const std::uint8_t>>::failure(
                    invalid("MPEG-TS audio access unit is absent from the typed program"));
            }
            return MediaTsAacAdtsFramer::frame(
                program->aac, unit.payload,
                m_audioFramingWorkspace);
            }
        default:
            return ::media::Result<std::span<const std::uint8_t>>::failure(
                invalid("MPEG-TS access unit stream is unsupported"));
        }
    }();
    if (!framed) return advanceFailure(framed.error());
    const auto framedBytes = framed.value();
    auto header = MediaTsPesSerializer::header(
        unit.stream, clock.value().clock(), framedBytes.size());
    if (!header) return advanceFailure(header.error());
    auto cursor = m_packetizer.beginPes(
        unit.stream, header.value(), framedBytes, unit.randomAccess);
    if (!cursor) return advanceFailure(cursor.error());
    auto packetCursor = std::move(cursor).value();
    m_pendingEmission.emplace(
        std::move(packetCursor), unit.emitOnMaster,
        std::move(clock).value(), m_emissionPlan.packetSizeBytes(),
        m_plan.parameters().maximumPacketsPerDatagram);
    m_emissionDiagnostics.recordPendingBytes(
        m_pendingEmission->pendingBytes());
    auto schedulingNow = m_masterClock->now();
    if (!schedulingNow) return advanceFailure(schedulingNow.error());
    if (schedulingNow.value() < actualMasterNow) {
        return advanceFailure(invalid(
            "MPEG-TS master clock regressed while preparing an access unit"));
    }
    auto emissionDecision = m_emissionSchedule->beginAccessUnit(
        unit.stream, m_pendingEmission->pendingBytes(),
        unit.emitOnMaster, unit.dispatchOnMaster,
        schedulingNow.value());
    if (!emissionDecision) return advanceFailure(emissionDecision.error());
    m_emissionDiagnostics.recordAccessUnitDecision(
        emissionDecision.value());
    auto interleaved = preparePendingDatagram();
    if (!interleaved) return advanceFailure(interleaved.error());
    auto result = emitPendingThrough(actualMasterNow);
    if (!result) return result;
    auto packetsWritten = checkedPacketCount(
        advanced.value().packetsWritten, interleaved.value());
    if (!packetsWritten) return advanceFailure(packetsWritten.error());
    packetsWritten = checkedPacketCount(
        packetsWritten.value(), result.value().packetsWritten);
    if (!packetsWritten) return advanceFailure(packetsWritten.error());
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{result.value().nextDeadline, packetsWritten.value()});
}

::media::Status MediaTsMuxSession::finish()
{
    if (m_state == State::Finished) {
        return m_failure ? ::media::Status::failure(*m_failure)
                         : ::media::Status::success();
    }
    if (m_state == State::Poisoned) {
        return ::media::Status::failure(*m_failure);
    }
    if (m_state == State::Created) return stateFailure("finish before start");
    if (m_pendingEmission) {
        return poison(invalid(
            "MPEG-TS mux session cannot finish with a pending datagram"));
    }
    auto status = ::media::Status::success();
    m_state = m_state == State::Open ? State::Finished : State::Poisoned;
    logEmissionFinal(
        m_failure ? m_failure->describe().c_str() : "completed");
    return m_failure ? ::media::Status::failure(*m_failure) : status;
}

void MediaTsMuxSession::abort() noexcept
{
    m_emissionDiagnostics.recordPendingBytes(0);
    logEmissionFinal("aborted");
    m_pendingEmission.reset();
    m_emissionSchedule.reset();
    m_protocolBatches.clear();
    if (m_state != State::Finished && m_state != State::Poisoned) {
        m_failure = ::media::ErrorInfo::cancelled("MPEG-TS mux session aborted");
        m_state = State::Poisoned;
    }
}

void MediaTsMuxSession::logEmissionProgress(bool force)
{
    const auto decision = m_emissionDiagnosticSampler.sample(force);
    if (decision.shouldLog) {
        m_emissionDiagnostics.logSnapshot(
            "periodic", "running", m_activation.generation);
    }
}

void MediaTsMuxSession::logEmissionFinal(const char* exitReason)
{
    if (m_emissionFinalLogged) return;
    m_emissionFinalLogged = true;
    m_emissionDiagnostics.logSnapshot(
        "final", exitReason ? exitReason : "unknown",
        m_activation.generation);
}

bool MediaTsMuxSession::hasPendingEmission() const noexcept
{
    return m_pendingEmission.has_value();
}

bool MediaTsMuxSession::hasScheduledBatch() const noexcept
{
    return !m_protocolBatches.empty();
}

::media::Result<MediaBufferRef> MediaTsMuxSession::takeScheduledBatch()
{
    if (m_protocolBatches.empty()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS mux session has no scheduled datagram batch"));
    }
    MediaBufferRef batch = std::move(m_protocolBatches.front());
    m_protocolBatches.pop_front();
    const auto* protocolBatch =
        dynamic_cast<const MediaMpegTsProtocolDatagramBatchBuffer*>(
            batch.get());
    auto produced = m_masterClock->now();
    if (!protocolBatch || protocolBatch->datagrams().empty() || !produced) {
        return ::media::Result<MediaBufferRef>::failure(
            !produced
                ? produced.error()
                : ::media::ErrorInfo::internalError(
                      "MPEG-TS mux session produced an invalid protocol batch"));
    }
    const auto& first = protocolBatch->datagrams().front();
    m_emissionDiagnostics.recordProtocolBatchProduced(
        produced.value(), first.canonicalRelease(),
        first.canonicalDeadline(), protocolBatch->datagrams().size());
    return ::media::Result<MediaBufferRef>::success(std::move(batch));
}

} // namespace media::ffmpeg::graph
