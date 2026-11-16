#include "internal/graph/nodes/output/MediaRtpWirePreparedTransaction.h"
#include "internal/graph/nodes/output/MediaWireCommitPreparation.h"
#include "internal/graph/nodes/output/MediaRtpWirePreparationWorkspace.h"
#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"
#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"

#include "internal/graph/protocol/rtp/MediaRtcpWireDatagramComposer.h"
#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpWirePacketComposer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"

#include <algorithm>
#include <limits>
#include <array>
#include <mutex>
#include <new>
#include <optional>
#include <sstream>
#include <utility>
#include <type_traits>

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
    auto storageGeometry = MediaRtpWireProtocolState::reservationStorageGeometry(config.maximumOutstandingDatagrams);
    if (!storageGeometry) return Result::failure(storageGeometry.error());
    const auto global = config.globalSequence->snapshot();
    if (config.globalSequence->sessionKey() != config.sessionKey ||
        config.globalSequence->serviceScopeId() != config.serviceScopeId ||
        global.generation != config.generation || global.poisoned) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire materializer global sequence session or service scope identity differs"));
    }
    auto cname = MediaRtcpSdesTextValidator::validateCname(config.cname);
    if (!cname) return Result::failure(cname.error());
    auto permit = config.globalSequence->claimRtpProtocolStorage(config.rtpEndpointId,
        config.rtcpEndpointId, config.maximumOutstandingDatagrams, config.reservationWakeup);
    if (!permit) return Result::failure(permit.error());
    if (permit.value().allocationGeometry() != storageGeometry.value()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP record backing differs from the planned allocation footprint"));
    }
    try {
        return Result::success(MediaRtpWireDatagramMaterializer(
            std::make_shared<MediaRtpWireProtocolState>(
                std::move(config), std::move(permit).value())));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaRtpWireDatagramMaterializer"));
    }
}

::media::Result<MediaRtpWirePreparedTransaction>
MediaRtpWireDatagramMaterializer::prepareBatch(
    MediaRtpWireInputBatch datagrams,
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocolBatch,
    MediaRunningTime materializedAt,
    MediaNetworkStorageLease* prepaid)
{
    using Result = ::media::Result<MediaRtpWirePreparedTransaction>;
    if (static_cast<bool>(protocolBatch) == (prepaid != nullptr) || datagrams.size() == 0 ||
        (protocolBatch &&
         protocolBatch->datagrams().size() < datagrams.size())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire batch requires at least one packetized datagram"));
    }
    if (datagrams.size() > m_state->maximumOutstandingDatagrams) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire transaction exceeds the admitted datagram bound"));
    }
    std::unique_lock protocolLock(m_state->mutex);
    if (m_state->poisoned || m_state->projectedTerminal || m_state->terminalCommitted) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP wire protocol state is terminal or poisoned"));
    }
    if (m_state->preparationActive) return Result::failure(
        ::media::ErrorInfo::invalidArgument("RTP state already has an unpublished preparation"));
    if (protocolBatch && protocolBatch->generation() != m_state->generation)
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP protocol preparation generation differs"));
    auto reportDecision = m_state->projectedSenderReportSchedule.prepare(
        datagrams.timing(0).canonicalRelease, m_state->generation);
    if (!reportDecision) return Result::failure(reportDecision.error());
    std::optional<MediaRtcpSenderReportPlan> rtcpPlan;
    if (reportDecision.value()) {
        auto report = MediaRtcpWireDatagramComposer::prepare(
            m_state->identity.ssrc(),
            m_state->cname,
            reportDecision.value()->reportInstant,
            m_state->ntpEpoch,
            m_state->clockMapper,
            m_state->projectedPacketCount,
            m_state->projectedOctetCount, false);
        if (!report) return Result::failure(report.error());
        if (report.value().bytes() > m_state->maximumDatagramBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTCP Sender Report exceeds the planned datagram bound"));
        }
        rtcpPlan.emplace(std::move(report).value());
    }
    auto rtcpDeadline = m_state->rtcpDeadline.canonicalDeadline(
        datagrams.timing(0).canonicalRelease, materializedAt, std::nullopt);
    if (!rtcpDeadline) return Result::failure(rtcpDeadline.error());

    const auto mediaDeadline = [&](std::size_t index) {
        return m_state->rtpDeadline.canonicalDeadline(
            datagrams.timing(index).canonicalRelease, materializedAt,
            protocolBatch ? std::optional<MediaRunningTime>(
                protocolBatch->datagrams()[index].completionLimit()) : std::nullopt);
    };
    std::optional<MediaWireProtocolDeadlineObservation> reportDeadlineObservation;
    if (rtcpPlan && protocolBatch) {
        auto protectedDeadline = mediaDeadline(0);
        if (!protectedDeadline) return Result::failure(protectedDeadline.error());
        // The report precedes its media in the same FIFO transaction.
        // Preserve an earlier RTCP deadline and the protected AU facts.
        if (protectedDeadline.value() < rtcpDeadline.value())
            rtcpDeadline = std::move(protectedDeadline);
        reportDeadlineObservation.emplace(MediaWireProtocolDeadlineObservation{
            protocolBatch->datagrams()[0].canonicalDeadline(),
            protocolBatch->datagrams()[0].completionLimit(), materializedAt, protocolBatch->publicationObservation(), true});
    }

    struct PreparedDatagram {
        MediaRtpWirePacketPlan plan;
        MediaRtpTimestamp timestamp;
        MediaRunningTime deadline;
        std::size_t payloadOctets;
    };
    const auto prepareDatagram = [&](std::size_t index) -> ::media::Result<PreparedDatagram> {
        using ItemResult = ::media::Result<PreparedDatagram>;
        auto input = datagrams.at(index);
        if (!input) return ItemResult::failure(input.error());
        const auto& datagram = input.value().timing;
        auto deadline = mediaDeadline(index);
        if (!deadline) return ItemResult::failure(deadline.error());
        auto timestamp = m_state->clockMapper.map(datagram.presentationOnMaster);
        if (!timestamp) return ItemResult::failure(timestamp.error());
        const auto sequence = static_cast<std::uint16_t>(m_state->projectedNextRtpSequence + index);
        auto plan = std::visit([&](const auto& payload) {
            if constexpr (std::is_same_v<std::decay_t<decltype(payload)>, MediaPacketizedRtpDatagramView>)
                return MediaRtpWirePacketComposer::prepare(payload.bytes, payload.payloadOctets,
                    m_state->identity, m_state->maximumDatagramBytes, timestamp.value(), sequence);
            else return MediaRtpWirePacketComposer::prepare(payload,
                m_state->identity, m_state->maximumDatagramBytes, timestamp.value(), sequence);
        }, input.value().payload);
        if (!plan) return ItemResult::failure(plan.error());
        return ItemResult::success({std::move(plan).value(), timestamp.value(), deadline.value(), input.value().payloadOctets()});
    };
    MediaWireDatagramBatchPartitionBuilder::Sizing outputSizing(
        m_state->batchPlan, m_state->globalSequence->commitStorage().batchGeometry);
    if (rtcpPlan) {
        auto appended = outputSizing.append(rtcpPlan->bytes(), rtcpDeadline.value());
        if (!appended) return Result::failure(appended.error());
    }
    std::uint64_t totalPayloadOctets = 0;
    std::optional<MediaRtpTimestamp> previousTimestamp =
        m_state->projectedLastTimestamp;
    std::optional<MediaRunningTime> previousRelease =
        m_state->projectedLastCanonicalRelease;
    for (std::size_t index = 0; index < datagrams.size(); ++index) {
        const auto datagram = datagrams.timing(index);
        auto item = prepareDatagram(index);
        if (!item) return Result::failure(item.error());
        const auto& timestamp = item.value().timestamp;
        if (item.value().payloadOctets > MaximumProtocolCounter - totalPayloadOctets)
            return Result::failure(::media::ErrorInfo::invalidArgument("RTP wire payload counter would overflow"));
        if (previousTimestamp && timestamp.extendedTicks() <
                                     previousTimestamp->extendedTicks()) {
            std::ostringstream message;
            message << "RTP wire timestamp regressed across projected order"
                    << " index=" << index
                    << " previous_extended_ticks="
                    << previousTimestamp->extendedTicks()
                    << " current_extended_ticks="
                    << timestamp.extendedTicks()
                    << " previous_wire=" << previousTimestamp->wire()
                    << " current_wire=" << timestamp.wire()
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
        totalPayloadOctets += static_cast<std::uint64_t>(item.value().payloadOctets);
        auto appended = outputSizing.append(item.value().plan.bytes(), item.value().deadline);
        if (!appended) return Result::failure(appended.error());
        previousTimestamp = timestamp;
        previousRelease = datagram.canonicalRelease;
    }
    if (datagrams.size() >
            MaximumProtocolCounter - m_state->projectedPacketCount ||
        totalPayloadOctets >
            MaximumProtocolCounter - m_state->projectedOctetCount) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire counter reservation would overflow"));
    }

    if (datagrams.size() > (std::numeric_limits<std::size_t>::max)() -
                               (rtcpPlan ? 1U : 0U)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire batch entry count is not representable"));
    }
    const std::size_t entryCount =
        datagrams.size() + (rtcpPlan ? 1U : 0U);
    // The actual atomic transaction includes any due RTCP report.
    // A permanent excess cannot be satisfied by waiting for earlier commits.
    if (entryCount > m_state->maximumOutstandingDatagrams) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP and RTCP transaction exceeds the admitted datagram bound"));
    }
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
    auto outputCost = outputSizing.storageFootprint();
    if (!outputCost) return Result::failure(outputCost.error());
    auto workspaceCost = MediaRtpWirePreparationWorkspace::storageFootprint(datagrams.size(), entryCount);
    if (!workspaceCost) return Result::failure(workspaceCost.error());
    auto lease = [&]() -> ::media::Result<MediaNetworkStorageLease> {
        using LeaseResult = ::media::Result<MediaNetworkStorageLease>;
        if (protocolBatch) return MediaWireCommitPreparation::admit(
            m_state->globalSequence, entryCount, outputCost.value(), workspaceCost.value());
        auto commitCost = MediaWireCommitPreparation::storageFootprint(
            m_state->globalSequence->commitStorage(), entryCount);
        if (!commitCost) return LeaseResult::failure(commitCost.error());
        std::size_t total = commitCost.value();
        for (const auto cost : {outputCost.value(), workspaceCost.value()}) {
            if (cost > (std::numeric_limits<std::size_t>::max)() - total)
                return LeaseResult::failure(::media::ErrorInfo::invalidArgument("Prepaid RTP preparation overflows"));
            total += cost;
        }
        auto settled = prepaid->shrinkTo(total);
        if (!settled) return LeaseResult::failure(settled.error());
        return LeaseResult::success(std::move(*prepaid));
    }();
    if (!lease) return Result::failure(lease.error());
    auto workspace = MediaRtpWirePreparationWorkspace::prepare(
        m_state->globalSequence->storageResource(), lease.value(), datagrams.size(), entryCount);
    if (!workspace) return Result::failure(workspace.error());
    auto& mediaPlans = workspace.value().plans;
    auto& timestamps = workspace.value().timestamps;
    auto& reservedPayloadOctets = workspace.value().payloadOctets;
    auto& rtpDeadlines = workspace.value().deadlines;
    auto& globalEntries = workspace.value().globalEntries;
    auto& outputInputs = workspace.value().outputInputs;
    for (std::size_t index = 0; index < datagrams.size(); ++index) {
        auto item = prepareDatagram(index);
        if (!item) return Result::failure(item.error());
        mediaPlans.push_back(std::move(item.value().plan));
        timestamps.push_back(item.value().timestamp);
        reservedPayloadOctets.push_back(static_cast<std::uint64_t>(item.value().payloadOctets));
        rtpDeadlines.push_back(item.value().deadline);
    }
    try {
        if (rtcpPlan) {
            globalEntries.push_back({
                m_state->rtcpEndpointId,
                static_cast<std::uint64_t>(rtcpPlan->bytes()),
                materializedAt});
        }
        for (const auto& plan : mediaPlans) {
            globalEntries.push_back({
                m_state->rtpEndpointId,
                static_cast<std::uint64_t>(plan.bytes()),
                materializedAt});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP wire service backlog reservation"));
    }
    try {
        if (rtcpPlan) outputInputs.push_back({MediaByteSerializationView::fromPlan<&MediaRtcpSenderReportGenerator::writeInto>(std::cref(*rtcpPlan)), m_state->rtcpEndpointId,
            datagrams.timing(0).canonicalRelease, rtcpDeadline.value(), MediaWireMediaBoundary::None, reportDeadlineObservation});
        for (std::size_t index = 0; index < datagrams.size(); ++index)
            outputInputs.push_back({MediaByteSerializationView::fromPlan<&MediaRtpWirePacketComposer::writeInto>(std::cref(mediaPlans[index])), m_state->rtpEndpointId,
                datagrams.timing(index).canonicalRelease, rtpDeadlines[index], datagrams.timing(index).mediaBoundary,
                protocolBatch ? std::optional<MediaWireProtocolDeadlineObservation>(MediaWireProtocolDeadlineObservation{
                    protocolBatch->datagrams()[index].canonicalDeadline(),
                    protocolBatch->datagrams()[index].completionLimit(), materializedAt, protocolBatch->publicationObservation(), false}) : std::nullopt});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("RTP output layout workspace"));
    }
    auto storage = MediaWireCommitPreparation::fromReservation(
        m_state->globalSequence, outputInputs.view(), m_state->batchPlan, std::move(lease).value());
    if (!storage) return Result::failure(storage.error());
    if (!storage.value().actions) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP commit preparation is missing action backing"));
    auto actionStorage = MediaFixedCapacityStorage<MediaRtpWireCommitAction>::fromAllocation(
        std::move(*storage.value().actions));
    if (!actionStorage) return Result::failure(actionStorage.error());
    auto actions = std::move(actionStorage).value();
    try {
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
    if (m_state->reservations.size() == m_state->reservations.capacity()) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP protocol reservation ledger exceeds its datagram capacity"));
    }
    auto prepared = MediaDatagramCommitTransaction::prepare<MediaRtpWireCommitTransaction>(
        m_state->generation, m_state->globalSequence->commitStorage(),
        m_state->globalSequence->storageResource(), std::move(storage.value().control));
    if (!prepared) return Result::failure(prepared.error());

    auto deadline = rtpDeadlines.front();
    for (const auto& value : rtpDeadlines) deadline = (std::min)(deadline, value);
    if (rtcpPlan) deadline = (std::min)(deadline, rtcpDeadline.value());
    if (protocolBatch) {
        auto registered = protocolBatch->registerPreparedWire(outputInputs.view(), globalEntries.view(),
            reportDecision.value() ? 1 : 0);
        if (!registered) return Result::failure(registered.error());
    }
    m_state->preparationActive = true;
    return Result::success(MediaRtpWirePreparedTransaction(
        m_state, std::move(storage).value(), std::move(globalEntries),
        std::move(actions), std::move(prepared).value(),
        reportDecision.value() ? std::optional(reportDecision.value()->commitToken) : std::nullopt,
        false, datagrams.size(), totalPayloadOctets, timestamps.back(),
        datagrams.timing(datagrams.size() - 1).canonicalRelease, deadline,
        std::move(protocolBatch)));
}

::media::Result<MediaWireDatagramBatchCollection>
MediaRtpWireDatagramMaterializer::complete(
    MediaRtpWirePreparedTransaction& pending, MediaRunningTime now)
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    std::unique_lock protocolLock(m_state->mutex);
    if (pending.m_state != m_state || !m_state->preparationActive ||
        m_state->poisoned || m_state->projectedTerminal || m_state->terminalCommitted)
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP completion requires its active preparation"));
    if (now < pending.entries().front().materializedAt || now > pending.m_deadline)
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP prepared transaction exceeded its immutable emission deadline"));
    auto& protocol = pending.m_protocol;
    if (protocol && (protocol->generation() != m_state->generation ||
        protocol->materializedDatagrams() != pending.m_protocolOffset ||
        protocol->datagrams().size() < pending.m_mediaCount))
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP protocol preparation cursor changed"));
    const auto entryCount = pending.entries().size();
    if (!m_state->canAdmitReservation(entryCount))
        return Result::failure(::media::ErrorInfo::internalError("RTP preparation lost its exclusive protocol capacity"));
    if (m_state->nextReservationIdentity == (std::numeric_limits<std::uint64_t>::max)())
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP reservation identity overflows"));
    if (protocol && !pending.m_storage.protocolPrefix)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS RTP completion requires prepared protocol prefix storage"));
    auto global = m_state->globalSequence->reserve(pending.entries(), pending.m_storage.global);
    if (!global) return Result::failure(global.error());
    std::optional<MediaProtocolDatagramCommitTransaction> protocolCommit;
    if (protocol) {
        auto transaction = protocol->takeCommitTransaction(
            pending.m_mediaCount, std::move(*pending.m_storage.protocolPrefix),
            m_state->globalSequence, global.value(), pending.m_report ? 1 : 0);
        if (transaction) pending.m_protocol.reset();
        if (!transaction || transaction.value().size() != pending.m_mediaCount) {
            m_state->poisoned = true;
            return Result::failure(transaction
                ? ::media::ErrorInfo::internalError("RTP nested protocol transaction cardinality differs")
                : transaction.error());
        }
        protocolCommit.emplace(std::move(transaction).value());
    }
    const auto reservationIdentity = m_state->nextReservationIdentity++;
    m_state->reservations.push_back(MediaRtpWireProtocolState::ReservationRecord{
        reservationIdentity, entryCount, 0});
    m_state->outstandingDatagrams += entryCount;
    if (pending.m_report) {
        auto projected = m_state->projectedSenderReportSchedule.commit(*pending.m_report);
        if (!projected) { m_state->poisoned = true; return Result::failure(projected.error()); }
    }
    m_state->projectedNextRtpSequence = static_cast<std::uint16_t>(
        m_state->projectedNextRtpSequence + pending.m_mediaCount);
    m_state->projectedPacketCount += pending.m_mediaCount;
    m_state->projectedOctetCount += pending.m_payloadOctets;
    if (pending.m_lastTimestamp) m_state->projectedLastTimestamp = pending.m_lastTimestamp;
    if (pending.m_terminal) m_state->projectedTerminal = true;
    m_state->projectedLastCanonicalRelease = pending.m_lastRelease;
    MediaRtpWireCommitTransaction commitReservation(m_state, reservationIdentity,
        std::move(global).value(), std::move(pending.m_actions), std::move(protocolCommit));
    m_state->preparationActive = false;
    pending.m_state.reset();
    protocolLock.unlock();
    auto transaction = std::move(pending.m_control).bind(std::move(commitReservation));
    if (!transaction) return Result::failure(transaction.error());
    return std::move(pending.m_storage.output).bind(std::move(transaction).value());
}

::media::Result<std::optional<MediaRtpWirePreparedTransaction>>
MediaRtpWireDatagramMaterializer::prepareTerminalReport(
    MediaRunningTime reportInstant,
    MediaRunningTime canonicalRelease,
    MediaRunningTime materializedAt)
{
    using Result =
        ::media::Result<std::optional<MediaRtpWirePreparedTransaction>>;
    std::unique_lock protocolLock(m_state->mutex);
    if (m_state->preparationActive)
        return Result::failure(::media::ErrorInfo::invalidArgument("RTP terminal report cannot overtake an active preparation"));
    // BYE follows every projected media reservation, including media whose
    // release instant is still in the future. Preserve that temporal order
    // as well as the global wire sequence before deriving its deadline.
    if (m_state->projectedLastCanonicalRelease &&
        canonicalRelease < *m_state->projectedLastCanonicalRelease)
        canonicalRelease = *m_state->projectedLastCanonicalRelease;
    auto canonicalDeadline = m_state->rtcpDeadline.canonicalDeadline(
        canonicalRelease, materializedAt, std::nullopt);
    if (!canonicalDeadline) return Result::failure(canonicalDeadline.error());
    if (m_state->poisoned || m_state->projectedTerminal) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP terminal report state is already terminal or poisoned"));
    }
    // RFC 3550 section 6.3.7: a source that never sent RTP or RTCP
    // leaves silently. Sender reports here are only projected with media.
    if (m_state->projectedPacketCount == 0) return Result::success(std::nullopt);
    auto report = MediaRtcpWireDatagramComposer::prepare(
        m_state->identity.ssrc(),
        m_state->cname,
        reportInstant,
        m_state->ntpEpoch,
        m_state->clockMapper,
        m_state->projectedPacketCount,
        m_state->projectedOctetCount, true);
    if (!report) return Result::failure(report.error());
    if (report.value().bytes() > m_state->maximumDatagramBytes) {
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
            static_cast<std::uint64_t>(report.value().bytes()),
            materializedAt}}};
    const std::array<MediaWireDatagramBatchInput, 1> outputInputs{{{
        MediaByteSerializationView::fromPlan<&MediaRtcpSenderReportGenerator::writeInto>(std::cref(report.value())), m_state->rtcpEndpointId, canonicalRelease,
        canonicalDeadline.value(), MediaWireMediaBoundary::None, std::nullopt}}};
    auto storage = MediaWireCommitPreparation::prepare(
        m_state->globalSequence, outputInputs, m_state->batchPlan);
    if (!storage) return Result::failure(storage.error());
    if (!storage.value().actions) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP commit preparation is missing action backing"));
    auto actionStorage = MediaFixedCapacityStorage<MediaRtpWireCommitAction>::fromAllocation(
        std::move(*storage.value().actions));
    if (!actionStorage) return Result::failure(actionStorage.error());
    auto actions = std::move(actionStorage).value();
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
    if (m_state->reservations.size() == m_state->reservations.capacity()) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP protocol reservation ledger exceeds its datagram capacity"));
    }
    auto prepared = MediaDatagramCommitTransaction::prepare<MediaRtpWireCommitTransaction>(
        m_state->generation, m_state->globalSequence->commitStorage(),
        m_state->globalSequence->storageResource(), std::move(storage.value().control));
    if (!prepared) return Result::failure(prepared.error());
    m_state->preparationActive = true;
    return Result::success(MediaRtpWirePreparedTransaction(
        m_state, std::move(storage).value(), globalEntries,
        std::move(actions), std::move(prepared).value(), std::nullopt,
        true, 0, 0, std::nullopt, canonicalRelease, canonicalDeadline.value(), nullptr));
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

std::size_t MediaRtpWireDatagramMaterializer::maximumBatchDatagrams() const noexcept
{
    return m_state->batchPlan.maximumDatagrams;
}

} // namespace media::ffmpeg::graph
