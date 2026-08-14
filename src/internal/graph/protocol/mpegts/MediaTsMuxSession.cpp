#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"

#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.h"
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
            video->video.layout() == parameters.h264InputLayout &&
            video->video.nalLengthBytes() == parameters.h264NalLengthBytes;
    }
    const auto* streams = std::get_if<
        MediaTsMuxSession::AudioVideoStreams>(&binding.streams);
    const auto* program = binding.plan.audioVideoProgram();
    return streams && program &&
        streams->video.layout() == parameters.h264InputLayout &&
        streams->video.nalLengthBytes() == parameters.h264NalLengthBytes &&
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

} // namespace

::media::Result<std::unique_ptr<MediaTsMuxSession>> MediaTsMuxSession::create(
    Binding binding)
{
    auto expectedEmission = MediaTsDatagramEmissionPlan::create(
        binding.plan,
        binding.emission.videoInitialServiceWindow(),
        binding.emission.audioInitialServiceWindow());
    if (!binding.sink || !binding.masterClock ||
        binding.activation.generation == 0 ||
        !materializedConfigMatches(binding) ||
        !expectedEmission || binding.emission != expectedEmission.value()) {
        return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
            invalid("MPEG-TS mux session binding is incomplete or inconsistent"));
    }
    auto clock = MediaTsOutputClockGenerator::create(
        binding.plan.clockPolicy(), binding.activation);
    if (!clock) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        clock.error());
    auto packetizer = MediaTsTransportPacketizer::create(
        binding.plan, binding.startsWithDiscontinuity);
    if (!packetizer) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        packetizer.error());
    auto tables = MediaTsPsiSerializer::serialize(binding.plan);
    if (!tables) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        tables.error());
    auto writer = MediaTsPacketBatchWriter::create(
        binding.plan.parameters().maximumPacketsPerDatagram,
        std::move(binding.sink),
        std::make_unique<MediaTsPacketCursorCommitter>());
    if (!writer) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        writer.error());
    return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::success(
        std::unique_ptr<MediaTsMuxSession>(new MediaTsMuxSession(
            std::move(binding), std::move(clock).value(),
            std::move(packetizer).value(), std::move(tables).value(),
            std::move(writer).value())));
}

MediaTsMuxSession::MediaTsMuxSession(
    Binding binding,
    MediaTsOutputClockGenerator clock,
    MediaTsTransportPacketizer packetizer,
    MediaTsProgramTables tables,
    MediaTsPacketBatchWriter writer) noexcept
    : m_plan(std::move(binding.plan)),
      m_emissionPlan(binding.emission),
      m_activation(binding.activation),
      m_masterClock(std::move(binding.masterClock)),
      m_streams(std::move(binding.streams)),
      m_clock(std::move(clock)),
      m_packetizer(std::move(packetizer)),
      m_tables(std::move(tables)),
      m_writer(std::move(writer)),
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
    MediaRunningTime notBefore,
    MediaRunningTime availableThrough)
{
    if (!m_emissionSchedule) {
        return ::media::Result<std::size_t>::failure(
            poison(invalid("MPEG-TS emission schedule is not active")).error());
    }
    std::size_t packetsWritten = 0;
    while (!cursor.finished()) {
        auto batch = m_writer.prepareNext(cursor);
        if (!batch) {
            return ::media::Result<std::size_t>::failure(
                poison(batch.error()).error());
        }
        const std::size_t packetCount = batch.value().packets().size();
        const std::size_t packetSize = m_emissionPlan.packetSizeBytes();
        if (packetSize == 0 || packetCount == 0 ||
            packetCount > (std::numeric_limits<std::size_t>::max)() /
                packetSize) {
            return ::media::Result<std::size_t>::failure(
                poison(invalid(
                    "MPEG-TS maintenance batch size is not representable")).error());
        }
        auto emission = m_emissionSchedule->prepareMaintenance(
            packetCount * packetSize, notBefore);
        if (!emission) {
            return ::media::Result<std::size_t>::failure(
                poison(emission.error()).error());
        }
        if (emission.value().deadline() > availableThrough ||
            availableThrough > emission.value().latestEmissionTime()) {
            return ::media::Result<std::size_t>::failure(
                poison(invalid(
                    "MPEG-TS maintenance emission is outside its canonical window")).error());
        }
        auto actualLateness = availableThrough.checkedSubtract(
            emission.value().deadline());
        if (!actualLateness) {
            return ::media::Result<std::size_t>::failure(
                poison(actualLateness.error()).error());
        }
        const MediaRunningTime plannedWait = emission.value().plannedWait();
        const std::size_t wireBytes = emission.value().wireBytes();
        const MediaRunningTime selectedDeadline = emission.value().deadline();
        auto written = m_writer.writeNext(
            cursor, std::move(batch).value(),
            emission.value().deadline());
        if (!written) {
            if (written.error().code == ::media::ErrorCode::WouldBlock) {
                m_emissionDiagnostics.recordPressureFailure();
            }
            return ::media::Result<std::size_t>::failure(
                poison(written.error()).error());
        }
        auto actualEmission = m_masterClock->now();
        if (!actualEmission ||
            actualEmission.value() < selectedDeadline ||
            actualEmission.value() > emission.value().latestEmissionTime()) {
            return ::media::Result<std::size_t>::failure(
                poison(actualEmission
                    ? invalid(
                          "MPEG-TS maintenance write completed outside its canonical window")
                    : actualEmission.error()).error());
        }
        auto committed = m_emissionSchedule->commit(
            std::move(emission).value(), actualEmission.value());
        if (!committed) {
            return ::media::Result<std::size_t>::failure(
                poison(committed.error()).error());
        }
        m_emissionDiagnostics.recordCommittedDatagram(
            wireBytes, plannedWait, selectedDeadline, actualEmission.value());
        logEmissionProgress();
        auto total = checkedPacketCount(
            packetsWritten, written.value().packetsWritten);
        if (!total) {
            return ::media::Result<std::size_t>::failure(
                poison(total.error()).error());
        }
        packetsWritten = total.value();
    }
    return ::media::Result<std::size_t>::success(packetsWritten);
}

::media::Result<std::size_t> MediaTsMuxSession::writeTables(
    MediaRunningTime emitOnMaster,
    MediaRunningTime availableThrough)
{
    auto pat = m_packetizer.beginPat(m_tables.pat());
    if (!pat) return ::media::Result<std::size_t>::failure(poison(pat.error()).error());
    auto patCursor = std::move(pat).value();
    auto patWritten = writeCursorThrough(
        patCursor, emitOnMaster, availableThrough);
    if (!patWritten) return ::media::Result<std::size_t>::failure(
        poison(patWritten.error()).error());
    auto pmt = m_packetizer.beginPmt(m_tables.pmt());
    if (!pmt) return ::media::Result<std::size_t>::failure(poison(pmt.error()).error());
    auto pmtCursor = std::move(pmt).value();
    auto pmtWritten = writeCursorThrough(
        pmtCursor, emitOnMaster, availableThrough);
    if (!pmtWritten) return ::media::Result<std::size_t>::failure(
        poison(pmtWritten.error()).error());
    auto total = checkedPacketCount(patWritten.value(), pmtWritten.value());
    if (!total) return ::media::Result<std::size_t>::failure(
        poison(total.error()).error());
    return total;
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
        return emitPending(masterNow, true);
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
MediaTsMuxSession::emitPending(
    MediaRunningTime masterNow,
    bool oneDatagramOnly)
{
    if (!m_pendingEmission || !m_emissionSchedule) {
        return advanceFailure(invalid(
            "MPEG-TS mux session has no pending canonical emission"));
    }
    std::size_t packetsWritten = 0;
    bool emitted = false;
    while (m_pendingEmission &&
           m_pendingEmission->deadline() <= masterNow &&
           (!oneDatagramOnly || !emitted)) {
        auto written = m_pendingEmission->emitPrepared(
            m_writer, *m_emissionSchedule, m_emissionDiagnostics,
            *m_masterClock, masterNow);
        if (!written) {
            if (written.error().code == ::media::ErrorCode::WouldBlock) {
                m_emissionDiagnostics.recordPressureFailure();
            }
            return advanceFailure(written.error());
        }
        logEmissionProgress();
        auto total = checkedPacketCount(
            packetsWritten, written.value().packetsWritten);
        if (!total) return advanceFailure(total.error());
        packetsWritten = total.value();
        emitted = true;
        if (m_pendingEmission->finished()) {
            auto completed = completePending();
            if (!completed) {
                return ::media::Result<AdvanceResult>::failure(
                    completed.error());
            }
            break;
        }
        auto next = m_pendingEmission->prepareNext(
            m_writer, *m_emissionSchedule);
        if (!next) return advanceFailure(next.error());
    }
    const MediaRunningTime nextDeadline = m_pendingEmission
        ? m_pendingEmission->deadline()
        : (m_nextPcr < m_nextPsi ? m_nextPcr : m_nextPsi);
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{nextDeadline, packetsWritten});
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
    std::size_t packetCount = 0;
    while (m_nextPcr <= emitOnMaster || m_nextPsi <= emitOnMaster) {
        const MediaRunningTime deadline =
            m_nextPsi < m_nextPcr ? m_nextPsi : m_nextPcr;
        const bool psiDue = m_nextPsi == deadline;
        const bool pcrDue = m_nextPcr == deadline;
        if (psiDue) {
            auto tables = writeTables(deadline, availableThrough);
            if (!tables) return ::media::Result<AdvanceResult>::failure(tables.error());
            auto count = checkedPacketCount(packetCount, tables.value());
            if (!count) return advanceFailure(count.error());
            packetCount = count.value();
        }
        if (pcrDue) {
            auto prepared = m_clock.preparePcr(
                m_activation.generation, m_nextPcr);
            if (!prepared) return advanceFailure(prepared.error());
            const auto& sample = prepared.value().clock();
            auto validation = m_clock.validateSerializedPcr(
                sample, sample.extended27Mhz);
            if (!validation) return advanceFailure(validation.error());
            auto cursor = m_packetizer.beginPcrOnly(sample);
            if (!cursor) return advanceFailure(cursor.error());
            auto packetCursor = std::move(cursor).value();
            auto result = writeCursorThrough(
                packetCursor, deadline, availableThrough);
            if (!result) return advanceFailure(result.error());
            auto clockCommit = m_clock.commitPcr(std::move(prepared).value());
            if (!clockCommit) return advanceFailure(clockCommit.error());
            auto count = checkedPacketCount(packetCount, result.value());
            if (!count) return advanceFailure(count.error());
            packetCount = count.value();
        }
        if (psiDue) {
            auto next = m_nextPsi.checkedAdd(m_plan.parameters().psiRepeatInterval);
            if (!next) return advanceFailure(next.error());
            m_nextPsi = next.value();
        }
        if (pcrDue) {
            auto next = m_nextPcr.checkedAdd(m_plan.clockPolicy().pcrInterval);
            if (!next) return advanceFailure(next.error());
            m_nextPcr = next.value();
        }
    }
    m_lastAdvance = emitOnMaster;
    const auto deadline = m_nextPcr < m_nextPsi ? m_nextPcr : m_nextPsi;
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{deadline, packetCount});
}

::media::Result<MediaTsMuxSession::AdvanceResult>
MediaTsMuxSession::writeAccessUnit(
    const MediaTsAccessUnitView& unit,
    MediaRunningTime actualMasterNow)
{
    if (m_pendingEmission) {
        return advanceFailure(invalid(
            "MPEG-TS mux session rejects a second access unit while emission is pending"));
    }
    if (actualMasterNow < unit.emitOnMaster) {
        return advanceFailure(invalid(
            "MPEG-TS access unit was presented before its canonical emission time"));
    }
    auto advanced = advanceThroughAvailable(
        unit.emitOnMaster, actualMasterNow);
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
            return MediaTsH264AccessUnitFramer::frame(
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
        std::move(clock).value(), m_emissionPlan.packetSizeBytes());
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
        unit.emitOnMaster, unit.dispatchOnMaster, schedulingNow.value());
    if (!emissionDecision) return advanceFailure(emissionDecision.error());
    m_emissionDiagnostics.recordAccessUnitDecision(
        emissionDecision.value());
    auto prepared = m_pendingEmission->prepareNext(
        m_writer, *m_emissionSchedule);
    if (!prepared) return advanceFailure(prepared.error());
    auto result = emitPending(actualMasterNow, true);
    if (!result) return result;
    auto packetsWritten = checkedPacketCount(
        advanced.value().packetsWritten, result.value().packetsWritten);
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
        m_writer.abort();
        return ::media::Status::failure(*m_failure);
    }
    if (m_state == State::Created) return stateFailure("finish before start");
    if (m_pendingEmission) {
        return poison(invalid(
            "MPEG-TS mux session cannot finish with a pending datagram"));
    }
    auto status = m_writer.finish();
    m_state = status && m_state == State::Open ? State::Finished : State::Poisoned;
    if (!status && !m_failure) m_failure = status.error();
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
    m_writer.abort();
    if (m_state != State::Finished && m_state != State::Poisoned) {
        m_failure = ::media::ErrorInfo::cancelled("MPEG-TS mux session aborted");
        m_state = State::Poisoned;
    }
}

void MediaTsMuxSession::logEmissionProgress(bool force)
{
    const auto decision = mediaGraphDiagnosticSample(
        MediaGraphDiagnosticLevel::Flow,
        "mpegts_emission_" + std::to_string(m_activation.generation),
        force);
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

} // namespace media::ffmpeg::graph
