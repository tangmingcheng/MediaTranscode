#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"
#include "internal/graph/nodes/output/MediaRtpWireCommitState.h"

#include "internal/graph/protocol/rtp/MediaRtcpWireDatagramComposer.h"
#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpWirePacketComposer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuilder.h"

#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t RtpFixedHeaderBytes = 12;
constexpr std::uint64_t MaximumProtocolCounter =
    (std::numeric_limits<std::uint64_t>::max)();

::media::Status validateTimes(MediaRunningTime release,
                              MediaRunningTime deadline)
{
    if (release < MediaRunningTime::fromNanoseconds(0) ||
        deadline < release) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP wire materialization requires a non-negative ordered canonical window"));
    }
    return ::media::Status::success();
}

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
    if (config.sessionKey.empty() || config.serviceScopeId == 0 ||
        config.generation == 0 || config.rtpEndpointId == 0 ||
        config.rtcpEndpointId == 0 ||
        config.rtpEndpointId == config.rtcpEndpointId ||
        !config.globalSequence || config.identity.ssrc() == 0 ||
        config.clockMapper.clockRate() <= 0 ||
        config.senderReportSchedule.generation() != config.generation ||
        config.maximumDatagramBytes <= RtpFixedHeaderBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire materializer requires complete generation, endpoint, identity, clock, schedule, counter, and datagram facts"));
    }
    const auto global = config.globalSequence->snapshot();
    if (config.globalSequence->sessionKey() != config.sessionKey ||
        config.globalSequence->serviceScopeId() != config.serviceScopeId ||
        global.generation != config.generation || global.poisoned ||
        global.reservationActive) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire materializer global sequence session or service scope identity differs"));
    }
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

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaRtpWireDatagramMaterializer::materialize(
    std::span<const std::uint8_t> packetizedRtp,
    std::size_t payloadOctets,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    auto times = validateTimes(canonicalRelease, canonicalDeadline);
    if (!times) return Result::failure(times.error());
    if (packetizedRtp.size() > m_state->maximumDatagramBytes ||
        payloadOctets == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire packet exceeds its planned datagram or counter bound"));
    }
    std::unique_lock protocolLock(m_state->mutex, std::try_to_lock);
    if (!protocolLock.owns_lock()) {
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "RTP wire protocol state already has an exclusive reservation"));
    }
    if (m_state->poisoned || m_state->terminalCommitted) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP wire protocol state is terminal or poisoned"));
    }
    const auto reservedPayloadOctets =
        static_cast<std::uint64_t>(payloadOctets);
    if (m_state->packetCount == MaximumProtocolCounter ||
        reservedPayloadOctets >
            MaximumProtocolCounter - m_state->octetCount) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire counter reservation would overflow"));
    }
    auto timestamp = m_state->clockMapper.map(presentationOnMaster);
    if (!timestamp) return Result::failure(timestamp.error());
    if (m_state->lastCommittedTimestamp &&
        timestamp.value().extendedTicks() <
            m_state->lastCommittedTimestamp->extendedTicks()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP wire timestamp regressed from committed protocol state"));
    }
    auto rtpBytes = MediaRtpWirePacketComposer::compose(
        packetizedRtp,
        payloadOctets,
        m_state->identity,
        timestamp.value(),
        m_state->nextRtpSequence,
        m_state->maximumDatagramBytes);
    if (!rtpBytes) return Result::failure(rtpBytes.error());

    auto reportDecision = m_state->senderReportSchedule.prepare(
        canonicalRelease, m_state->generation);
    if (!reportDecision) return Result::failure(reportDecision.error());
    std::optional<std::vector<std::uint8_t>> rtcpBytes;
    if (reportDecision.value()) {
        auto report = MediaRtcpWireDatagramComposer::composeSenderReport(
            m_state->identity.ssrc(),
            m_state->cname,
            *reportDecision.value(),
            m_state->ntpEpoch,
            m_state->clockMapper,
            m_state->packetCount,
            m_state->octetCount);
        if (!report) return Result::failure(report.error());
        if (report.value().size() > m_state->maximumDatagramBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTCP Sender Report exceeds the planned datagram bound"));
        }
        rtcpBytes = std::move(report).value();
    }

    const std::size_t entryCount = rtcpBytes ? 2 : 1;
    auto global = m_state->globalSequence->reserve(entryCount);
    if (!global) return Result::failure(global.error());
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
        actions.push_back(MediaRtpWireCommitAction{
            MediaRtpWireCommitActionKind::Media,
            std::nullopt,
            reservedPayloadOctets,
            timestamp.value()});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP wire commit actions"));
    }
    std::shared_ptr<MediaRtpWireCommitTransaction> transaction;
    try {
        transaction = std::make_shared<MediaRtpWireCommitTransaction>(
            m_state,
            std::move(protocolLock),
            std::move(global).value(),
            std::move(actions));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP wire commit transaction"));
    }

    MediaWireDatagramBatchBuilder builder(m_state->generation);
    std::size_t index = 0;
    if (rtcpBytes) {
        auto sequence = transaction->sequence(index);
        if (!sequence) return Result::failure(sequence.error());
        auto lease = makeMediaRtpWireCommitLease(
            transaction, index, m_state->generation, sequence.value());
        if (!lease) return Result::failure(lease.error());
        auto appended = builder.append(
            *rtcpBytes,
            m_state->rtcpEndpointId,
            canonicalRelease,
            canonicalDeadline,
            sequence.value(),
            std::move(lease).value());
        if (!appended) return Result::failure(appended.error());
        ++index;
    }
    auto sequence = transaction->sequence(index);
    if (!sequence) return Result::failure(sequence.error());
    auto lease = makeMediaRtpWireCommitLease(
        transaction, index, m_state->generation, sequence.value());
    if (!lease) return Result::failure(lease.error());
    auto appended = builder.append(
        rtpBytes.value(),
        m_state->rtpEndpointId,
        canonicalRelease,
        canonicalDeadline,
        sequence.value(),
        std::move(lease).value());
    if (!appended) return Result::failure(appended.error());
    return builder.finish();
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaRtpWireDatagramMaterializer::materializeTerminalReport(
    MediaRunningTime reportInstant,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    auto times = validateTimes(canonicalRelease, canonicalDeadline);
    if (!times) return Result::failure(times.error());
    std::unique_lock protocolLock(m_state->mutex, std::try_to_lock);
    if (!protocolLock.owns_lock()) {
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "RTP terminal report waits for the active commit reservation"));
    }
    if (m_state->poisoned || m_state->terminalCommitted) {
        return Result::failure(::media::ErrorInfo::internalError(
            "RTP terminal report state is already terminal or poisoned"));
    }
    auto datagram = MediaRtcpWireDatagramComposer::composeTerminalReport(
        m_state->identity.ssrc(),
        m_state->cname,
        reportInstant,
        m_state->ntpEpoch,
        m_state->clockMapper,
        m_state->packetCount,
        m_state->octetCount);
    if (!datagram) return Result::failure(datagram.error());
    if (datagram.value().size() > m_state->maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTCP terminal report exceeds the planned datagram bound"));
    }
    auto global = m_state->globalSequence->reserve(1);
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
    std::shared_ptr<MediaRtpWireCommitTransaction> transaction;
    try {
        transaction = std::make_shared<MediaRtpWireCommitTransaction>(
            m_state,
            std::move(protocolLock),
            std::move(global).value(),
            std::move(actions));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "RTP terminal commit transaction"));
    }
    auto sequence = transaction->sequence(0);
    if (!sequence) return Result::failure(sequence.error());
    auto lease = makeMediaRtpWireCommitLease(
        transaction, 0, m_state->generation, sequence.value());
    if (!lease) return Result::failure(lease.error());
    MediaWireDatagramBatchBuilder builder(m_state->generation);
    auto appended = builder.append(
        datagram.value(),
        m_state->rtcpEndpointId,
        canonicalRelease,
        canonicalDeadline,
        sequence.value(),
        std::move(lease).value());
    if (!appended) return Result::failure(appended.error());
    return builder.finish();
}

::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
MediaRtpWireDatagramMaterializer::snapshot() const noexcept
{
    using Result =
        ::media::Result<MediaRtpWireDatagramMaterializerSnapshot>;
    std::unique_lock lock(m_state->mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "RTP wire snapshot cannot observe an active commit reservation"));
    }
    return Result::success(MediaRtpWireDatagramMaterializerSnapshot{
        m_state->nextRtpSequence,
        m_state->packetCount,
        m_state->octetCount,
        m_state->terminalCommitted,
        m_state->poisoned});
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
