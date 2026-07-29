#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"

#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportEmissionOrigin.h"

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
    return binding.video.layout() == parameters.h264InputLayout &&
           binding.video.nalLengthBytes() == parameters.h264NalLengthBytes &&
           binding.audio.audioObjectType() == parameters.aac.audioObjectType &&
           binding.audio.samplingFrequencyIndex() ==
               parameters.aac.samplingFrequencyIndex &&
           binding.audio.channelConfiguration() ==
               parameters.aac.channelConfiguration;
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
    if (!binding.sink || binding.epoch.generation == 0 ||
        !materializedConfigMatches(binding)) {
        return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
            invalid("MPEG-TS mux session binding is incomplete or inconsistent"));
    }
    auto clock = MediaTsOutputClockGenerator::create(
        binding.plan.clockPolicy(), binding.epoch);
    if (!clock) return ::media::Result<std::unique_ptr<MediaTsMuxSession>>::failure(
        clock.error());
    auto packetizer = MediaTsTransportPacketizer::create(binding.plan);
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
      m_epoch(binding.epoch),
      m_video(std::move(binding.video)),
      m_audio(std::move(binding.audio)),
      m_clock(std::move(clock)),
      m_packetizer(std::move(packetizer)),
      m_tables(std::move(tables)),
      m_writer(std::move(writer)),
      m_nextPsi(binding.epoch.masterRelease),
      m_nextPcr(binding.epoch.masterRelease)
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
    return ::media::Status::failure(*m_failure);
}

::media::Result<MediaTsMuxSession::AdvanceResult>
MediaTsMuxSession::advanceFailure(::media::ErrorInfo error)
{
    poison(std::move(error));
    return ::media::Result<AdvanceResult>::failure(*m_failure);
}

::media::Result<std::size_t> MediaTsMuxSession::writeTables()
{
    auto pat = m_packetizer.beginPat(m_tables.pat());
    if (!pat) return ::media::Result<std::size_t>::failure(poison(pat.error()).error());
    auto patCursor = std::move(pat).value();
    auto patWritten = m_writer.writeCursor(patCursor);
    if (!patWritten) return ::media::Result<std::size_t>::failure(
        poison(patWritten.error()).error());
    auto pmt = m_packetizer.beginPmt(m_tables.pmt());
    if (!pmt) return ::media::Result<std::size_t>::failure(poison(pmt.error()).error());
    auto pmtCursor = std::move(pmt).value();
    auto pmtWritten = m_writer.writeCursor(pmtCursor);
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
    auto origin = mediaTsTransportEmissionOrigin(m_plan, m_epoch);
    if (!origin) return poison(origin.error());
    if (emitOnMaster != origin.value()) {
        return poison(invalid(
            "MPEG-TS mux session start must equal its planned transport emission origin"));
    }
    auto nextPsi = emitOnMaster.checkedAdd(m_plan.parameters().psiRepeatInterval);
    if (!nextPsi) return poison(nextPsi.error());
    m_nextPsi = nextPsi.value();
    m_lastAdvance = emitOnMaster;
    auto tables = writeTables();
    if (!tables) return ::media::Status::failure(tables.error());
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

    const MediaRunningTime deadline =
        m_nextPcr < m_nextPsi ? m_nextPcr : m_nextPsi;
    if (masterNow < deadline) {
        return ::media::Result<AdvanceResult>::success(
            AdvanceResult{deadline, 0});
    }

    // Poll advances exactly one transport deadline. The caller may poll again
    // to catch up, preserving the planned PCR cadence without turning transport
    // maintenance into a second access-unit pacing authority.
    auto advanced = advanceThrough(deadline);
    if (!advanced) {
        return ::media::Result<AdvanceResult>::failure(advanced.error());
    }
    return ::media::Result<AdvanceResult>::success(AdvanceResult{
        advanced.value().nextDeadline, advanced.value().packetsWritten});
}

::media::Result<MediaTsMuxSession::AdvanceResult> MediaTsMuxSession::advanceThrough(
    MediaRunningTime emitOnMaster)
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
            auto tables = writeTables();
            if (!tables) return ::media::Result<AdvanceResult>::failure(tables.error());
            auto count = checkedPacketCount(packetCount, tables.value());
            if (!count) return advanceFailure(count.error());
            packetCount = count.value();
        }
        if (pcrDue) {
            auto prepared = m_clock.preparePcr(m_epoch.generation, m_nextPcr);
            if (!prepared) return advanceFailure(prepared.error());
            const auto& sample = prepared.value().clock();
            auto validation = m_clock.validateSerializedPcr(
                sample, sample.extended27Mhz);
            if (!validation) return advanceFailure(validation.error());
            auto cursor = m_packetizer.beginPcrOnly(sample);
            if (!cursor) return advanceFailure(cursor.error());
            auto packetCursor = std::move(cursor).value();
            auto result = m_writer.writeCursor(packetCursor);
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
    const MediaTsAccessUnitView& unit)
{
    auto advanced = advanceThrough(unit.emitOnMaster);
    if (!advanced) {
        return ::media::Result<AdvanceResult>::failure(advanced.error());
    }
    if (unit.generation != m_epoch.generation || unit.payload.empty() ||
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
            return MediaTsH264AccessUnitFramer::frame(
                m_plan, m_video, unit.payload, unit.randomAccess,
                m_videoFramingWorkspace);
        case MediaScheduledStream::Audio:
            return MediaTsAacAdtsFramer::frame(
                m_plan.parameters().aac, unit.payload,
                m_audioFramingWorkspace);
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
    auto result = m_writer.writeCursor(packetCursor);
    if (!result) return advanceFailure(result.error());
    auto clockCommit = m_clock.commitPacket(std::move(clock).value());
    if (!clockCommit) return advanceFailure(clockCommit.error());
    auto packetsWritten = checkedPacketCount(
        advanced.value().packetsWritten, result.value());
    if (!packetsWritten) return advanceFailure(packetsWritten.error());
    m_lastAccessUnitEmission = unit.emitOnMaster;
    return ::media::Result<AdvanceResult>::success(
        AdvanceResult{advanced.value().nextDeadline, packetsWritten.value()});
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
    auto status = m_writer.finish();
    m_state = status && m_state == State::Open ? State::Finished : State::Poisoned;
    if (!status && !m_failure) m_failure = status.error();
    return m_failure ? ::media::Status::failure(*m_failure) : status;
}

void MediaTsMuxSession::abort() noexcept
{
    m_writer.abort();
    if (m_state != State::Finished && m_state != State::Poisoned) {
        m_failure = ::media::ErrorInfo::cancelled("MPEG-TS mux session aborted");
        m_state = State::Poisoned;
    }
}

} // namespace media::ffmpeg::graph
