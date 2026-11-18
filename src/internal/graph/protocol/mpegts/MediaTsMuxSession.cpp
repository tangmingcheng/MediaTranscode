#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"
#include "internal/graph/diagnostics/MediaDiagnosticMasterClock.h"
#include "internal/graph/diagnostics/MediaOutputPreparationObservation.h"
#include "internal/graph/planner/realtime/MediaTsAcceptedWorkPlanner.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketBuilder.h"

#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsVideoAccessUnitFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportEmissionOrigin.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <limits>
#include <new>
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
    std::span<const std::size_t> cursorPacketCounts,
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
    Binding binding) try
{
    auto expectedEmission = MediaTsDatagramEmissionPlan::create(
        binding.plan,
        binding.emission.videoInitialServiceWindow(),
        binding.emission.audioInitialServiceWindow(),
        binding.emission.maximumQueuedBytes(),
        binding.emission.targetServiceResidence());
    if (!binding.masterClock || !binding.producerWakeup ||
        binding.activation.generation == 0 ||
        !materializedConfigMatches(binding) ||
        !expectedEmission || binding.emission != expectedEmission.value()) {
        return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
            invalid("MPEG-TS mux session binding is incomplete or inconsistent"));
    }
    if (!binding.transport || binding.transport->generation() != binding.activation.generation ||
        !binding.transport->storageResource() || !binding.transport->muxQueueStorage() ||
        !binding.transport->seriesStorage())
        return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
            invalid("TS mux requires its final generation storage contract"));
    const auto& storage = *binding.transport->muxQueueStorage();
    MediaOutputPreparationObservation observation("ts_mux", binding.activation.generation,
        storage.allocationBytes, storage.slots);
    auto validStorage = MediaTsMuxQueueStoragePlanner::validate(storage);
    if (!validStorage || storage.maximumQueuedBytes != binding.emission.maximumQueuedBytes())
        return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(!validStorage ? validStorage.error() :
            invalid("TS mux queue contract differs from emission capacity"));
    auto& resource = *binding.transport->storageResource();
    auto reservation = resource.reserveStorage(storage.allocationBytes, MediaStorageAdmission::RequiredAllocation);
    if (!reservation) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(reservation.error());
    auto allocation = resource.allocateFromReservation(storage.slots * sizeof(MediaBufferRef),
        alignof(MediaBufferRef), std::move(reservation).value());
    if (!allocation) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(allocation.error());
    auto queue = MediaFixedCapacityStorage<MediaBufferRef>::fromAllocation(std::move(allocation).value());
    if (!queue) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(queue.error());
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
    auto packetizer = [&] {
        const auto& series = *binding.transport->seriesStorage();
        MediaOutputPreparationObservation packetizerObservation("ts_packetizer", binding.activation.generation,
            series.allocationBytes, series.maximumLiveSeries);
        return MediaTsTransportPacketizer::create(binding.plan, binding.startsWithDiscontinuity,
            series, binding.transport->storageResource());
    }();
    if (!packetizer) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        packetizer.error());
    auto tables = MediaTsPsiSerializer::serialize(binding.plan);
    if (!tables) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        tables.error());
    return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::success(
        std::unique_ptr<MediaTsMuxSession>(new MediaTsMuxSession(
            std::move(binding), std::move(clock).value(),
            std::move(packetizer).value(), std::move(tables).value(), std::move(queue).value())));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        ::media::ErrorInfo::allocationFailed("TS mux session allocation failed"));
}

MediaTsMuxSession::MediaTsMuxSession(
    Binding binding,
    MediaTsOutputClockGenerator clock,
    MediaTsTransportPacketizer packetizer,
    MediaTsProgramTables tables,
    MediaFixedCapacityStorage<MediaBufferRef> protocolBatches) noexcept
    : m_plan(std::move(binding.plan)),
      m_emissionPlan(binding.emission),
      m_activation(binding.activation),
      m_masterClock(std::move(binding.masterClock)),
      m_transport(std::move(binding.transport)),
      m_producerWakeup(std::move(binding.producerWakeup)),
      m_prefixService(std::move(binding.prefixService)),
      m_streams(std::move(binding.streams)),
      m_clock(std::move(clock)),
      m_packetizer(std::move(packetizer)),
      m_tables(std::move(tables)),
      m_protocolBatches(std::move(protocolBatches)),
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
    logAccessUnitObservations(false);
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
    MediaRunningTime availableThrough,
    MediaRunningTime completionLimit,
    const MediaTsAcceptedWorkObservation& acceptedWork)
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
    const std::size_t batchBytes = packetCount * packetSize;
    if (auto status = validateProtocolBatchCapacity(batchBytes); !status) {
        return ::media::Result<std::size_t>::failure(
            poison(status.error()).error());
    }
    auto emission = m_emissionSchedule->prepareMaintenance(
        batchBytes);
    if (!emission) {
        return ::media::Result<std::size_t>::failure(
            poison(emission.error()).error());
    }
    const MediaRunningTime release = emission.value().deadline();
    const MediaRunningTime deadline = emission.value().latestEmissionTime();
    const auto maximumReservationDatagrams = cursor.storagePlan().maximumDatagrams;
    auto batch = MediaMpegTsProtocolDatagramBatchBuffer::create(
        m_activation.generation, std::move(cursor),
        m_plan.parameters().maximumPacketsPerDatagram,
        availableThrough, release, deadline, MediaWireMediaBoundary::None, maximumReservationDatagrams,
        completionLimit, acceptedWork);
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
    batch.value()->observeMuxPublicationReady(mediaDiagnosticMasterClockNow(*m_masterClock));
    m_protocolBatches.push_back(std::move(batch).value());
    m_protocolBatchBytes += batchBytes;
    logEmissionProgress();
    return ::media::Result<std::size_t>::success(packetCount);
}

::media::Result<std::array<MediaTsMaintenanceServiceObligation, 2>>
MediaTsMuxSession::maintenanceObligations(
    MediaRunningTime nextPcr, MediaRunningTime nextPsi) const
{
    using Result = ::media::Result<std::array<MediaTsMaintenanceServiceObligation, 2>>;
    auto patPackets = m_packetizer.patPacketCount(m_tables.pat());
    auto pmtPackets = m_packetizer.pmtPacketCount(m_tables.pmt());
    if (!patPackets || !pmtPackets)
        return Result::failure(!patPackets ? patPackets.error() : pmtPackets.error());
    const auto duration = [this](std::size_t packets) -> ::media::Result<MediaRunningTime> {
        const auto packetBytes = m_emissionPlan.packetSizeBytes();
        if (!packetBytes || packets > (std::numeric_limits<std::size_t>::max)() / packetBytes)
            return ::media::Result<MediaRunningTime>::failure(invalid(
                "MPEG-TS maintenance payload size is not representable"));
        const auto datagrams = packets / m_plan.parameters().maximumPacketsPerDatagram +
            (packets % m_plan.parameters().maximumPacketsPerDatagram != 0);
        return m_prefixService.durationForPayload(packets * packetBytes,
            m_prefixService.materializationBatches(datagrams));
    };
    auto pat = duration(patPackets.value());
    auto pmt = duration(pmtPackets.value());
    auto pcr = duration(MediaTsTransportPacketBuilder::PcrOnlyPacketCount);
    if (!pat || !pmt || !pcr)
        return Result::failure(!pat ? pat.error() : !pmt ? pmt.error() : pcr.error());
    auto psi = pat.value().checkedAdd(pmt.value());
    auto pcrDeadline = nextPcr.checkedAdd(m_plan.clockPolicy().pcrInterval);
    auto psiDeadline = nextPsi.checkedAdd(m_plan.timingPolicy().psiRepeatInterval().value);
    if (!psi || !pcrDeadline || !psiDeadline)
        return Result::failure(!psi ? psi.error() :
            !pcrDeadline ? pcrDeadline.error() : psiDeadline.error());
    // PSI is emitted first when both are due, matching writeDueMaintenance.
    std::array<MediaTsMaintenanceServiceObligation, 2> result{{
        {nextPsi, psiDeadline.value(), psi.value()},
        {nextPcr, pcrDeadline.value(), pcr.value()}}};
    if (nextPcr < nextPsi) std::swap(result[0], result[1]);
    return Result::success(result);
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
    std::array<std::size_t, MediaTsTransportPacketizer::maintenancePreparationSlots()> packetCounts{};
    std::size_t groupSeries = 0;
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
        packetCounts[groupSeries++] = patPackets.value();
        packetCounts[groupSeries++] = pmtPackets.value();
        auto psiCompletion = deadline.checkedAdd(
            m_plan.timingPolicy().psiRepeatInterval().value);
        if (!psiCompletion) {
            return ::media::Result<std::size_t>::failure(
                poison(psiCompletion.error()).error());
        }
        completionDeadline = psiCompletion.value();
    }
    if (pcrDue) {
        packetCounts[groupSeries++] = MediaTsTransportPacketBuilder::PcrOnlyPacketCount;
        auto pcrCompletion = deadline.checkedAdd(
            m_plan.clockPolicy().pcrInterval);
        if (!pcrCompletion) {
            return ::media::Result<std::size_t>::failure(
                poison(pcrCompletion.error()).error());
        }
        completionDeadline = completionDeadline
            ? (std::min)(*completionDeadline, pcrCompletion.value())
            : pcrCompletion.value();
    }
    const auto activePacketCounts = std::span<const std::size_t>(packetCounts).first(groupSeries);
    auto geometry = maintenanceGroupGeometry(
        activePacketCounts, m_emissionPlan.packetSizeBytes(),
        m_plan.parameters().maximumPacketsPerDatagram);
    if (!geometry || !completionDeadline) {
        return ::media::Result<std::size_t>::failure(
            poison(geometry
                ? invalid("MPEG-TS maintenance completion deadline is absent")
                : geometry.error()).error());
    }
    MediaRunningTime publicationLimit = *completionDeadline;
    // availableThrough may be a future AU lookahead horizon. Feasibility
    // uses the real clock observation, not that scheduling horizon.
    auto preparationNow = m_masterClock->now();
    if (!preparationNow)
        return ::media::Result<std::size_t>::failure(poison(preparationNow.error()).error());
    std::optional<MediaTsUnpublishedTail> tail;
    if (m_pendingEmission) {
        auto observedTail = m_pendingEmission->unpublishedTail(*m_emissionSchedule);
        if (!observedTail) return ::media::Result<std::size_t>::failure(poison(observedTail.error()).error());
        tail = observedTail.value();
    }
    auto accepted = acceptedWorkObservation(preparationNow.value());
    if (!accepted)
        return ::media::Result<std::size_t>::failure(poison(accepted.error()).error());
    auto requiredService = MediaRunningTime::fromNanoseconds(0);
    for (const auto packets : activePacketCounts) {
        const auto datagrams = packets / m_plan.parameters().maximumPacketsPerDatagram +
            (packets % m_plan.parameters().maximumPacketsPerDatagram != 0);
        auto duration = m_prefixService.durationForPayload(packets * m_emissionPlan.packetSizeBytes(),
            m_prefixService.materializationBatches(datagrams));
        if (!duration) return ::media::Result<std::size_t>::failure(poison(duration.error()).error());
        auto total = requiredService.checkedAdd(duration.value());
        if (!total) return ::media::Result<std::size_t>::failure(poison(total.error()).error());
        requiredService = total.value();
    }
    if (tail) {
        auto maintenanceFinish = (std::max)(accepted.value().availableAt, deadline).checkedAdd(requiredService);
        if (!maintenanceFinish) return ::media::Result<std::size_t>::failure(poison(maintenanceFinish.error()).error());
        auto fits = MediaTsAcceptedWorkPlanner::validateTail(*tail, maintenanceFinish.value(), m_prefixService);
        if (!fits) return ::media::Result<std::size_t>::failure(poison(fits.error()).error());
    }
    if (m_pendingEmission) {
        auto nextPcr = pcrDue
            ? deadline.checkedAdd(m_plan.clockPolicy().pcrInterval)
            : ::media::Result<MediaRunningTime>::success(m_nextPcr);
        auto nextPsi = psiDue
            ? deadline.checkedAdd(m_plan.timingPolicy().psiRepeatInterval().value)
            : ::media::Result<MediaRunningTime>::success(m_nextPsi);
        if (!nextPcr || !nextPsi)
            return ::media::Result<std::size_t>::failure(
                poison(!nextPcr ? nextPcr.error() : nextPsi.error()).error());
        auto obligations = maintenanceObligations(nextPcr.value(), nextPsi.value());
        if (!obligations)
            return ::media::Result<std::size_t>::failure(poison(obligations.error()).error());
        auto publication = MediaTsPrefixPublicationPlanner::plan(
            deadline, (std::min)(*completionDeadline, m_pendingEmission->completionDeadline()),
            obligations.value(), preparationNow.value(), accepted.value(), requiredService,
            MediaTsPublicationOrder::PreserveSelectedMaintenance);
        if (!publication)
            return ::media::Result<std::size_t>::failure(poison(publication.error()).error());
        const auto* window = std::get_if<MediaTsPrefixPublicationWindow>(&publication.value());
        if (!window)
            return ::media::Result<std::size_t>::failure(poison(invalid(
                "MPEG-TS maintenance missed its next publication boundary")).error());
        publicationLimit = window->completionLimit;
    } else {
        auto publication = MediaTsPrefixPublicationPlanner::plan(
            deadline, *completionDeadline, {}, preparationNow.value(), accepted.value(), requiredService,
            MediaTsPublicationOrder::PreserveSelectedMaintenance);
        if (!publication)
            return ::media::Result<std::size_t>::failure(poison(publication.error()).error());
    }
    auto storage = m_packetizer.prepareMaintenance(activePacketCounts);
    if (!storage) {
        if (storage.error().code == ::media::ErrorCode::WouldBlock)
            return ::media::Result<std::size_t>::failure(storage.error());
        return ::media::Result<std::size_t>::failure(poison(storage.error()).error());
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
        preparedPcr.emplace(std::move(pcr).value());
    }
    std::size_t storageIndex = 0;
    if (auto status = m_emissionSchedule->beginMaintenanceGroup(
            geometry.value().payloadBytes, geometry.value().datagramCount,
            deadline, *completionDeadline); !status) {
        return ::media::Result<std::size_t>::failure(
            poison(status.error()).error());
    }
    std::size_t packetsWritten = 0;
    const auto writeMaintenanceCursor = [this, availableThrough, publicationLimit, &accepted, &packetsWritten](
        ::media::Result<MediaTsPacketCursor> cursorResult) -> ::media::Status {
        if (!cursorResult) {
            return ::media::Status::failure(cursorResult.error());
        }
        MediaTsPacketCursor cursor = std::move(cursorResult).value();
        auto written = writeCursorThrough(cursor, availableThrough, publicationLimit, accepted.value().observation);
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
                m_packetizer.beginPat(m_tables.pat(), std::move(*storage.value()[storageIndex++]))); !status) {
            return ::media::Result<std::size_t>::failure(
                poison(status.error()).error());
        }
        if (auto status = writeMaintenanceCursor(
                m_packetizer.beginPmt(m_tables.pmt(), std::move(*storage.value()[storageIndex++]))); !status) {
            return ::media::Result<std::size_t>::failure(
                poison(status.error()).error());
        }
    }
    if (preparedPcr) {
        if (auto status = writeMaintenanceCursor(
                m_packetizer.beginPcrOnly(preparedPcr->clock(), std::move(*storage.value()[storageIndex++]))); !status) {
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
    if (m_pendingObservation)
        m_previousCompletedObservation.emplace(*m_pendingObservation, m_pendingEmission->commitObservation());
    m_emissionDiagnostics.recordAccessUnitCompleted();
    m_lastAccessUnitEmission = m_pendingEmission->notBefore();
    m_pendingEmission.reset();
    m_pendingObservation.reset();
    m_emissionDiagnostics.recordPendingBytes(0);
    return ::media::Status::success();
}

::media::Result<MediaTsPublishedWorkService>
MediaTsMuxSession::acceptedWorkObservation(MediaRunningTime observedNow)
{
    using Result = ::media::Result<MediaTsPublishedWorkService>;
    auto observation = m_packetizer.lockAcceptedWork();
    if (!observation) return Result::failure(observation.error());
    const auto snapshot = m_transport->snapshot();
    auto records = observation.value().unlockForPlanning();
    return MediaTsAcceptedWorkPlanner::plan(records, snapshot, m_activation.generation,
        observedNow, m_prefixService);
}

::media::Result<MediaTsMuxSession::AdvanceResult>
MediaTsMuxSession::emitPendingThrough(MediaRunningTime masterNow)
{
    if (!m_pendingEmission || !m_emissionSchedule)
        return advanceFailure(invalid("MPEG-TS mux session has no pending canonical emission"));
    auto committed = m_pendingEmission->waitForCommit(m_producerWakeup);
    if (!committed) {
        if (committed.error().code == ::media::ErrorCode::WouldBlock)
            return ::media::Result<AdvanceResult>::failure(committed.error());
        return advanceFailure(committed.error());
    }
    if (m_pendingEmission->finished()) {
        auto completed = completePending();
        if (!completed) return ::media::Result<AdvanceResult>::failure(completed.error());
        return ::media::Result<AdvanceResult>::success(
            AdvanceResult{(std::min)(m_nextPcr, m_nextPsi), 0});
    }
    auto obligations = maintenanceObligations(m_nextPcr, m_nextPsi);
    if (!obligations) return advanceFailure(obligations.error());
    auto tail = m_pendingEmission->unpublishedTail(*m_emissionSchedule);
    if (!tail) return advanceFailure(tail.error());
    auto accepted = acceptedWorkObservation(masterNow);
    if (!accepted) return advanceFailure(accepted.error());
    if (tail.value()) {
        auto fits = MediaTsAcceptedWorkPlanner::validateTail(*tail.value(), accepted.value().availableAt, m_prefixService);
        if (!fits) return advanceFailure(fits.error());
    }
    if (!tail.value() || m_protocolBatchBytes > m_emissionPlan.maximumQueuedBytes() ||
        m_protocolBatches.size() > m_protocolBatches.capacity())
        return advanceFailure(invalid("MPEG-TS pending prefix capacity facts are inconsistent"));
    auto decision = MediaTsPrefixPublicationPlanner::planBatch(*tail.value(),
        MediaTsPrefixPublicationCapacity{
            m_emissionPlan.maximumQueuedBytes() - m_protocolBatchBytes,
            m_protocolBatches.capacity() - m_protocolBatches.size()},
        m_prefixService, obligations.value(), masterNow, accepted.value());
    if (!decision) return advanceFailure(decision.error());
    if (const auto* boundary = std::get_if<MediaTsMaintenancePublicationBoundary>(&decision.value())) {
        if (masterNow < boundary->release)
            return ::media::Result<AdvanceResult>::success(AdvanceResult{boundary->release, 0});
        // Reserve maintenance before resuming the same PID's payload cursor.
        // The adapter drains these batches before polling the AU again.
        return advanceThroughAvailable(boundary->release, masterNow);
    }
    const auto& prefix = std::get<MediaTsPrefixBatch>(decision.value());
    auto materialized = preparePendingPrefix(prefix);
    if (!materialized) {
        if (materialized.error().code == ::media::ErrorCode::WouldBlock)
            return ::media::Result<AdvanceResult>::failure(materialized.error());
        return advanceFailure(materialized.error());
    }
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{prefix.window.completionLimit, materialized.value()});
}

::media::Result<std::size_t> MediaTsMuxSession::preparePendingPrefix(
    const MediaTsPrefixBatch& prefix)
{
    using Result = ::media::Result<std::size_t>;
    if (!m_pendingEmission || !m_emissionSchedule)
        return Result::failure(invalid("MPEG-TS pending prefix has no active access unit"));
    if (auto status = validateProtocolBatchCapacity(prefix.payloadBytes); !status)
        return Result::failure(status.error());
    auto batch = m_pendingEmission->materializePrefix(
        m_activation.generation, *m_emissionSchedule, prefix);
    if (!batch) return Result::failure(batch.error());
    std::static_pointer_cast<MediaMpegTsProtocolDatagramBatchBuffer>(batch.value())
        ->observeMuxPublicationReady(mediaDiagnosticMasterClockNow(*m_masterClock));
    m_protocolBatches.push_back(std::move(batch).value());
    // Session serialization prevents takeScheduledBatch before this stamp.
    m_pendingEmission->recordPublished();
    m_protocolBatchBytes += prefix.payloadBytes;
    m_emissionDiagnostics.recordPendingBytes(m_pendingEmission->pendingBytes());
    logEmissionProgress();
    return Result::success(prefix.payloadBytes / m_emissionPlan.packetSizeBytes());
}

::media::Status MediaTsMuxSession::validateProtocolBatchCapacity(
    std::uint64_t bytes) const
{
    const std::uint64_t limit = m_emissionPlan.maximumQueuedBytes();
    if (bytes == 0 || bytes % m_emissionPlan.packetSizeBytes() != 0 ||
        m_protocolBatches.size() == m_protocolBatches.capacity() ||
        m_protocolBatchBytes > limit || bytes > limit - m_protocolBatchBytes) {
        return ::media::Status::failure(invalid(
            "MPEG-TS protocol batches exceed the planned queued byte capacity"));
    }
    return ::media::Status::success();
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
    // One call may cross multiple PCR deadlines. Materialize each planned
    // sample below; preparePcr validates every adjacent interval, rather
    // than mistaking the caller's advancement span for a PCR sample gap.
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
    MediaRunningTime actualMasterNow,
    const MediaAccessUnitMuxObservation& observation)
{
    m_emissionDiagnostics.recordAccessUnitReady(
        actualMasterNow, unit.emitOnMaster, unit.dispatchOnMaster);
    if (m_pendingEmission) {
        return advanceFailure(invalid(
            "MPEG-TS mux session rejects a second access unit while emission is pending"));
    }
    const MediaRunningTime maintenanceAvailableThrough = (std::max)(
        actualMasterNow, unit.emitOnMaster);
    // PCR maintenance may already have advanced while waiting for this AU.
    // Keep that clock monotonic without changing the AU's PTS/DTS or release.
    const MediaRunningTime maintenanceThrough = m_lastAdvance
        ? (std::max)(unit.emitOnMaster, *m_lastAdvance)
        : unit.emitOnMaster;
    auto advanced = advanceThroughAvailable(
        maintenanceThrough, maintenanceAvailableThrough);
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
    if (!cursor) {
        if (cursor.error().code == ::media::ErrorCode::WouldBlock)
            return ::media::Result<AdvanceResult>::failure(cursor.error());
        return advanceFailure(cursor.error());
    }
    auto packetCursor = std::move(cursor).value();
    auto deferred = packetCursor.deferFirstReservation();
    if (!deferred) return advanceFailure(deferred.error());
    m_pendingObservation = observation;
    m_pendingEmission.emplace(
        std::move(packetCursor), unit.emitOnMaster, unit.dispatchOnMaster,
        std::move(clock).value(), m_emissionPlan.packetSizeBytes(),
        m_plan.parameters().maximumPacketsPerDatagram,
        unit.stream == MediaScheduledStream::Video && unit.randomAccess
            ? MediaWireMediaBoundary::VideoRandomAccessUnitEnd
            : MediaWireMediaBoundary::None);
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
    // Ownership has transferred. Publication and pressure are handled by poll;
    // returning WouldBlock now would make the caller submit this AU twice.
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{unit.emitOnMaster, advanced.value().packetsWritten});
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

void MediaTsMuxSession::logAccessUnitObservations(bool aborted) noexcept
{
    // Finished sessions have no failed/aborted AU to diagnose. Both paths
    // consume the fixed snapshots so later teardown cannot log them twice.
    if (m_state != State::Finished) {
        if (m_pendingObservation) {
            m_pendingObservation->log(aborted ? "pending_au_abort" : "pending_au_failure");
            if (m_pendingEmission)
                m_pendingEmission->commitObservation().log(
                    aborted ? "current_au_abort" : "current_au_failure", *m_pendingObservation);
        }
        if (m_previousCompletedObservation) {
            m_previousCompletedObservation->first.log(aborted
                ? "previous_completed_au_trace_on_abort" : "previous_completed_au_trace");
            m_previousCompletedObservation->second.log(
                aborted ? "previous_completed_au_on_abort" : "previous_completed_au",
                m_previousCompletedObservation->first);
        }
    }
    m_pendingObservation.reset();
    m_previousCompletedObservation.reset();
}

void MediaTsMuxSession::abort() noexcept
{
    logAccessUnitObservations(true);
    m_emissionDiagnostics.recordPendingBytes(0);
    logEmissionFinal("aborted");
    m_pendingEmission.reset();
    m_emissionSchedule.reset();
    m_protocolBatches.clear();
    m_protocolBatchBytes = 0;
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
    auto* protocolBatch =
        dynamic_cast<MediaMpegTsProtocolDatagramBatchBuffer*>(
            batch.get());
    const auto bytes = batch->payloadFootprintBytes();
    if (!bytes || *bytes > m_protocolBatchBytes) {
        return ::media::Result<MediaBufferRef>::failure(
            poison(::media::ErrorInfo::internalError(
                "MPEG-TS protocol batch byte accounting is inconsistent")).error());
    }
    m_protocolBatchBytes -= *bytes;
    auto produced = m_masterClock->now();
    if (!protocolBatch || protocolBatch->datagrams().empty() || !produced) {
        return ::media::Result<MediaBufferRef>::failure(
            !produced
                ? produced.error()
                : ::media::ErrorInfo::internalError(
                      "MPEG-TS mux session produced an invalid protocol batch"));
    }
    const auto& first = protocolBatch->datagrams().front();
    protocolBatch->observeMuxDequeued(produced.value());
    m_emissionDiagnostics.recordProtocolBatchProduced(
        produced.value(), first.canonicalRelease(),
        first.canonicalDeadline(), protocolBatch->datagrams().size(), *bytes);
    return ::media::Result<MediaBufferRef>::success(std::move(batch));
}

} // namespace media::ffmpeg::graph
