#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"
#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"

#include "internal/graph/protocol/rtp/MediaRtcpWireDatagramComposer.h"
#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpWirePacketComposer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuilder.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"

#include <limits>
#include <array>
#include <mutex>
#include <new>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t RtpFixedHeaderBytes = 12;
constexpr std::uint64_t MaximumProtocolCounter =
    (std::numeric_limits<std::uint64_t>::max)();

} // namespace

MediaRtpWireDatagramMaterializer::MediaRtpWireDatagramMaterializer(
    std::shared_ptr<MediaRtpWireProtocolState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Result<MediaRtpWireDatagramMaterializer>
MediaRtpWireDatagramMaterializer::create(
    MediaRtpWireDatagramMaterializerConfig config)
{
    using Result = ::media::Result<MediaRtpWireDatagramMaterializer>;
    if (config.sessionKey.empty() || config.serviceScopeId.empty() ||
        config.generation == 0 || config.rtpEndpointId == 0 ||
        config.rtcpEndpointId == 0 ||
        config.rtpEndpointId == config.rtcpEndpointId ||
        config.rtpDeadline.endpointId != config.rtpEndpointId ||
        config.rtcpDeadline.endpointId != config.rtcpEndpointId ||
        !config.globalSequence || config.identity.ssrc() == 0 ||
        config.clockMapper.clockRate() <= 0 ||
        config.senderReportSchedule.generation() != config.generation ||
        config.maximumDatagramBytes <= RtpFixedHeaderBytes ||
        config.maximumOutstandingDatagrams == 0 ||
        !config.reservationWakeup ||
        config.batchPlan.maximumDatagrams == 0 ||
        config.batchPlan.maximumBytes < config.maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire materializer requires complete generation, endpoint, identity, clock, schedule, counter, and datagram facts"));
    }
    const auto global = config.globalSequence->snapshot();
    if (config.globalSequence->sessionKey() != config.sessionKey ||
        config.globalSequence->serviceScopeId() != config.serviceScopeId ||
        global.generation != config.generation || global.poisoned) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire materializer global sequence session or service scope identity differs"));
    }
    auto registeredRtp = config.globalSequence->registerReservationWakeup(
        config.rtpEndpointId, config.reservationWakeup);
    if (!registeredRtp) return Result::failure(registeredRtp.error());
    auto registeredRtcp = config.globalSequence->registerReservationWakeup(
        config.rtcpEndpointId, config.reservationWakeup);
    if (!registeredRtcp) return Result::failure(registeredRtcp.error());
    auto cname = MediaRtcpSdesTextValidator::validateCname(config.cname);
    if (!cname) return Result::failure(cname.error());
    try {
        return Result::success(MediaRtpWireDatagramMaterializer(
            std::make_shared<MediaRtpWireProtocolState>(
                std::move(config))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaRtpWireDatagramMaterializer"));
    }
}

::media::Result<MediaWireDatagramBatchCollection>
MediaRtpWireDatagramMaterializer::materialize(
    std::span<const std::uint8_t> packetizedRtp,
    std::size_t payloadOctets,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease,
    MediaRunningTime materializedAt)
{
    const std::array<MediaPacketizedRtpDatagramView, 1> datagrams{{
        {packetizedRtp, payloadOctets, presentationOnMaster,
         canonicalRelease}}};
    return materializeBatch(datagrams, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaRtpWireDatagramMaterializer::materializeBatch(
    std::span<const MediaPacketizedRtpDatagramView> datagrams,
    MediaRunningTime materializedAt)
{
    return materializeBatchReserved(datagrams, nullptr, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaRtpWireDatagramMaterializer::materializeProtocolBatch(
    std::span<const MediaPacketizedRtpDatagramView> datagrams,
    MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch,
    MediaRunningTime materializedAt)
{
    return materializeBatchReserved(
        datagrams, &protocolBatch, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaRtpWireDatagramMaterializer::materializeBatchReserved(
    std::span<const MediaPacketizedRtpDatagramView> datagrams,
    MediaMpegTsProtocolDatagramBatchBuffer* protocolBatch,
    MediaRunningTime materializedAt)
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    if (datagrams.empty() ||
        (protocolBatch &&
         protocolBatch->datagrams().size() != datagrams.size())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire batch requires at least one packetized datagram"));
    }
    std::unique_lock protocolLock(m_state->mutex);
    if (m_state->poisoned || m_state->terminalCommitted) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP wire protocol state is terminal or poisoned"));
    }
    std::vector<std::vector<std::uint8_t>> materializedRtp;
    std::vector<MediaRtpTimestamp> timestamps;
    std::vector<std::uint64_t> reservedPayloadOctets;
    std::vector<MediaRunningTime> rtpDeadlines;
    try {
        materializedRtp.reserve(datagrams.size());
        timestamps.reserve(datagrams.size());
        reservedPayloadOctets.reserve(datagrams.size());
        rtpDeadlines.reserve(datagrams.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP wire batch workspace"));
    }
    std::uint64_t totalPayloadOctets = 0;
    std::optional<MediaRtpTimestamp> previousTimestamp =
        m_state->projectedLastTimestamp;
    std::optional<MediaRunningTime> previousRelease =
        m_state->projectedLastCanonicalRelease;
    for (std::size_t index = 0; index < datagrams.size(); ++index) {
        const auto& datagram = datagrams[index];
        auto deadline = m_state->rtpDeadline.canonicalDeadline(
            datagram.canonicalRelease, materializedAt);
        if (!deadline) return Result::failure(deadline.error());
        if (datagram.bytes.size() > m_state->maximumDatagramBytes ||
            datagram.payloadOctets == 0 ||
            datagram.payloadOctets >
                MaximumProtocolCounter - totalPayloadOctets) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTP wire packet exceeds its planned datagram or counter bound"));
        }
        auto timestamp = m_state->clockMapper.map(
            datagram.presentationOnMaster);
        if (!timestamp) return Result::failure(timestamp.error());
        if (previousTimestamp && timestamp.value().extendedTicks() <
                                     previousTimestamp->extendedTicks()) {
            std::ostringstream message;
            message << "RTP wire timestamp regressed across projected order"
                    << " index=" << index
                    << " previous_extended_ticks="
                    << previousTimestamp->extendedTicks()
                    << " current_extended_ticks="
                    << timestamp.value().extendedTicks()
                    << " previous_wire=" << previousTimestamp->wire()
                    << " current_wire=" << timestamp.value().wire()
                    << " previous_release_ns="
                    << (previousRelease
                            ? previousRelease->nanoseconds()
                            : -1)
                    << " current_presentation_ns="
                    << datagram.presentationOnMaster.nanoseconds()
                    << " current_release_ns="
                    << datagram.canonicalRelease.nanoseconds();
            return Result::failure(::media::ErrorInfo::invalidArgument(
                message.str()));
        }
        auto bytes = MediaRtpWirePacketComposer::compose(
            datagram.bytes, datagram.payloadOctets, m_state->identity,
            timestamp.value(),
            static_cast<std::uint16_t>(
                m_state->projectedNextRtpSequence + index),
            m_state->maximumDatagramBytes);
        if (!bytes) return Result::failure(bytes.error());
        totalPayloadOctets +=
            static_cast<std::uint64_t>(datagram.payloadOctets);
        try {
            materializedRtp.push_back(std::move(bytes).value());
            timestamps.push_back(timestamp.value());
            reservedPayloadOctets.push_back(
                static_cast<std::uint64_t>(datagram.payloadOctets));
            rtpDeadlines.push_back(deadline.value());
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "RTP wire batch materialization"));
        }
        previousTimestamp = timestamp.value();
        previousRelease = datagram.canonicalRelease;
    }
    if (datagrams.size() >
            MaximumProtocolCounter - m_state->projectedPacketCount ||
        totalPayloadOctets >
            MaximumProtocolCounter - m_state->projectedOctetCount) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire counter reservation would overflow"));
    }

    auto reportDecision = m_state->projectedSenderReportSchedule.prepare(
        datagrams.front().canonicalRelease, m_state->generation);
    if (!reportDecision) return Result::failure(reportDecision.error());
    std::optional<std::vector<std::uint8_t>> rtcpBytes;
    if (reportDecision.value()) {
        auto report = MediaRtcpWireDatagramComposer::composeSenderReport(
            m_state->identity.ssrc(),
            m_state->cname,
            *reportDecision.value(),
            m_state->ntpEpoch,
            m_state->clockMapper,
            m_state->projectedPacketCount,
            m_state->projectedOctetCount);
        if (!report) return Result::failure(report.error());
        if (report.value().size() > m_state->maximumDatagramBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTCP Sender Report exceeds the planned datagram bound"));
        }
        rtcpBytes = std::move(report).value();
    }
    auto rtcpDeadline = m_state->rtcpDeadline.canonicalDeadline(
        datagrams.front().canonicalRelease, materializedAt);
    if (!rtcpDeadline) return Result::failure(rtcpDeadline.error());

    if (datagrams.size() > (std::numeric_limits<std::size_t>::max)() -
                               (rtcpBytes ? 1U : 0U)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire batch entry count is not representable"));
    }
    const std::size_t entryCount =
        datagrams.size() + (rtcpBytes ? 1U : 0U);
    if (!m_state->canAdmitReservation(entryCount)) {
        if (m_state->blockedReservationDatagrams &&
            *m_state->blockedReservationDatagrams != entryCount) {
            m_state->poisoned = true;
            return Result::failure(::media::ErrorInfo::internalError(
                "RTP wire materializer changed an armed reservation demand"));
        }
        m_state->blockedReservationDatagrams = entryCount;
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "RTP wire reservation exceeds planner backlog capacity"));
    }
    if (m_state->blockedReservationDatagrams &&
        *m_state->blockedReservationDatagrams != entryCount) {
        m_state->poisoned = true;
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP wire materializer resumed with a different reservation demand"));
    }
    m_state->blockedReservationDatagrams.reset();
    std::vector<MediaWireGlobalSequenceReservationEntry> globalEntries;
    try {
        globalEntries.reserve(entryCount);
        if (rtcpBytes) {
            globalEntries.push_back({
                m_state->rtcpEndpointId,
                static_cast<std::uint64_t>(rtcpBytes->size()),
                materializedAt});
        }
        for (const auto& bytes : materializedRtp) {
            globalEntries.push_back({
                m_state->rtpEndpointId,
                static_cast<std::uint64_t>(bytes.size()),
                materializedAt});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP wire service backlog reservation"));
    }
    auto global = m_state->globalSequence->reserve(globalEntries);
    if (!global) return Result::failure(global.error());
    std::optional<MediaProtocolDatagramCommitTransaction> protocolCommit;
    if (protocolBatch) {
        auto transaction = protocolBatch->takeCommitTransaction();
        if (!transaction || transaction.value().size() != datagrams.size()) {
            m_state->poisoned = true;
            return Result::failure(
                transaction
                    ? ::media::ErrorInfo::internalError(
                          "RTP wire nested protocol transaction cardinality differs")
                    : transaction.error());
        }
        protocolCommit.emplace(std::move(transaction).value());
    }
    std::vector<MediaRtpWireCommitAction> actions;
    try {
        actions.reserve(entryCount);
        if (reportDecision.value()) {
            actions.push_back(MediaRtpWireCommitAction{
                MediaRtpWireCommitActionKind::SenderReport,
                reportDecision.value()->commitToken,
                0,
                std::nullopt});
        }
        for (std::size_t index = 0; index < datagrams.size(); ++index) {
            actions.push_back(MediaRtpWireCommitAction{
                MediaRtpWireCommitActionKind::Media,
                std::nullopt,
                reservedPayloadOctets[index],
                timestamps[index],
                protocolBatch != nullptr});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP wire commit actions"));
    }
    const auto reservationIdentity = m_state->nextReservationIdentity++;
    try {
        m_state->reservations.push_back(
            MediaRtpWireProtocolState::ReservationRecord{
                reservationIdentity, entryCount, 0});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP wire protocol reservation ledger"));
    }
    m_state->outstandingDatagrams += entryCount;
    if (reportDecision.value()) {
        auto projected = m_state->projectedSenderReportSchedule.commit(
            reportDecision.value()->commitToken);
        if (!projected) {
            m_state->poisoned = true;
            return Result::failure(projected.error());
        }
    }
    m_state->projectedNextRtpSequence = static_cast<std::uint16_t>(
        m_state->projectedNextRtpSequence + datagrams.size());
    m_state->projectedPacketCount += datagrams.size();
    m_state->projectedOctetCount += totalPayloadOctets;
    m_state->projectedLastTimestamp = timestamps.back();
    m_state->projectedLastCanonicalRelease =
        datagrams.back().canonicalRelease;
    const auto generation = m_state->generation;
    MediaRtpWireCommitTransaction commitReservation(
        m_state,
        reservationIdentity,
        std::move(global).value(),
        std::move(actions),
        std::move(protocolCommit));
    protocolLock.unlock();
    auto transaction = MediaDatagramCommitTransaction::create(
        generation, std::move(commitReservation));
    if (!transaction) return Result::failure(transaction.error());
    const auto firstGlobalSequence = transaction.value().firstGlobalSequence();

    auto builderResult = MediaWireDatagramBatchPartitionBuilder::create(
        m_state->globalSequence->sessionKey(),
        m_state->globalSequence->serviceScopeId(),
        m_state->generation, m_state->batchPlan,
        std::move(transaction).value());
    if (!builderResult) return Result::failure(builderResult.error());
    auto builder = std::move(builderResult).value();
    std::size_t index = 0;
    if (rtcpBytes) {
        const auto sequence = firstGlobalSequence +
            static_cast<std::uint64_t>(index);
        auto appended = builder.append(
            *rtcpBytes,
            m_state->rtcpEndpointId,
            datagrams.front().canonicalRelease,
            rtcpDeadline.value(),
            sequence);
        if (!appended) return Result::failure(appended.error());
        ++index;
    }
    for (std::size_t datagramIndex = 0;
         datagramIndex < datagrams.size(); ++datagramIndex, ++index) {
        const auto sequence = firstGlobalSequence +
            static_cast<std::uint64_t>(index);
        auto appended = builder.append(
            materializedRtp[datagramIndex],
            m_state->rtpEndpointId,
            datagrams[datagramIndex].canonicalRelease,
            rtpDeadlines[datagramIndex],
            sequence);
        if (!appended) return Result::failure(appended.error());
    }
    return builder.finish();
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaRtpWireDatagramMaterializer::materializeTerminalReport(
    MediaRunningTime reportInstant,
    MediaRunningTime canonicalRelease,
    MediaRunningTime materializedAt)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    auto canonicalDeadline = m_state->rtcpDeadline.canonicalDeadline(
        canonicalRelease, materializedAt);
    if (!canonicalDeadline) return Result::failure(canonicalDeadline.error());
    std::unique_lock protocolLock(m_state->mutex);
    if (m_state->poisoned || m_state->projectedTerminal) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP terminal report state is already terminal or poisoned"));
    }
    auto datagram = MediaRtcpWireDatagramComposer::composeTerminalReport(
        m_state->identity.ssrc(),
        m_state->cname,
        reportInstant,
        m_state->ntpEpoch,
        m_state->clockMapper,
        m_state->projectedPacketCount,
        m_state->projectedOctetCount);
    if (!datagram) return Result::failure(datagram.error());
    if (datagram.value().size() > m_state->maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTCP terminal report exceeds the planned datagram bound"));
    }
    if (!m_state->canAdmitReservation(1U)) {
        if (m_state->blockedReservationDatagrams &&
            *m_state->blockedReservationDatagrams != 1U) {
            m_state->poisoned = true;
            return Result::failure(::media::ErrorInfo::internalError(
                "RTP terminal materializer changed an armed reservation demand"));
        }
        m_state->blockedReservationDatagrams = 1U;
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "RTP terminal reservation exceeds planner backlog capacity"));
    }
    if (m_state->blockedReservationDatagrams &&
        *m_state->blockedReservationDatagrams != 1U) {
        m_state->poisoned = true;
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP terminal materializer resumed with a different reservation demand"));
    }
    m_state->blockedReservationDatagrams.reset();
    const std::array<MediaWireGlobalSequenceReservationEntry, 1>
        globalEntries{{{
            m_state->rtcpEndpointId,
            static_cast<std::uint64_t>(datagram.value().size()),
            materializedAt}}};
    auto global = m_state->globalSequence->reserve(globalEntries);
    if (!global) return Result::failure(global.error());
    std::vector<MediaRtpWireCommitAction> actions;
    try {
        actions.push_back(MediaRtpWireCommitAction{
            MediaRtpWireCommitActionKind::TerminalReport,
            std::nullopt,
            0,
            std::nullopt});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP terminal commit action"));
    }
    const auto reservationIdentity = m_state->nextReservationIdentity++;
    try {
        m_state->reservations.push_back(
            MediaRtpWireProtocolState::ReservationRecord{
                reservationIdentity, 1, 0});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP terminal protocol reservation ledger"));
    }
    ++m_state->outstandingDatagrams;
    m_state->projectedTerminal = true;
    const auto generation = m_state->generation;
    MediaRtpWireCommitTransaction commitReservation(
        m_state,
        reservationIdentity,
        std::move(global).value(),
        std::move(actions), std::nullopt);
    protocolLock.unlock();
    auto transaction = MediaDatagramCommitTransaction::create(
        generation, std::move(commitReservation));
    if (!transaction) return Result::failure(transaction.error());
    const auto sequence = transaction.value().firstGlobalSequence();
    auto builderResult = MediaWireDatagramBatchPartitionBuilder::create(
        m_state->globalSequence->sessionKey(),
        m_state->globalSequence->serviceScopeId(),
        m_state->generation, m_state->batchPlan,
        std::move(transaction).value());
    if (!builderResult) return Result::failure(builderResult.error());
    auto builder = std::move(builderResult).value();
    auto appended = builder.append(
        datagram.value(),
        m_state->rtcpEndpointId,
        canonicalRelease,
        canonicalDeadline.value(),
        sequence);
    if (!appended) return Result::failure(appended.error());
    auto finished = builder.finish();
    if (!finished) return Result::failure(finished.error());
    if (finished.value().size() != 1) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP terminal report did not produce exactly one wire partition"));
    }
    return Result::success(std::move(finished).value().front());
}

::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
MediaRtpWireDatagramMaterializer::snapshot() const noexcept
{
    using Result =
        ::media::Result<MediaRtpWireDatagramMaterializerSnapshot>;
    std::lock_guard lock(m_state->mutex);
    return Result::success(MediaRtpWireDatagramMaterializerSnapshot{
        m_state->nextRtpSequence,
        m_state->packetCount,
        m_state->octetCount,
        m_state->terminalCommitted,
        m_state->poisoned});
}

std::uint64_t MediaRtpWireDatagramMaterializer::generation() const noexcept
{
    return m_state ? m_state->generation : 0;
}

int MediaRtpWireDatagramMaterializer::payloadType() const noexcept
{
    return m_state->identity.payloadType();
}

int MediaRtpWireDatagramMaterializer::clockRate() const noexcept
{
    return m_state->clockMapper.clockRate();
}

std::uint32_t MediaRtpWireDatagramMaterializer::ssrc() const noexcept
{
    return m_state->identity.ssrc();
}

std::size_t
MediaRtpWireDatagramMaterializer::maximumDatagramBytes() const noexcept
{
    return m_state->maximumDatagramBytes;
}

} // namespace media::ffmpeg::graph
