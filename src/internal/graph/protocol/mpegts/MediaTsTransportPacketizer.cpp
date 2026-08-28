#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketBuilder.h"

#include <algorithm>
#include <deque>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t PacketSize = 188;

struct ContinuityState final {
    std::uint8_t nextPayload;
};

enum class CursorPidKind : std::uint8_t { Pat, Pmt, Video, Audio, Pcr };

struct VideoContinuityState final {
    std::array<ContinuityState, 3> next;
    std::array<bool, 3> discontinuityPending;
};

struct AudioVideoContinuityState final {
    std::array<ContinuityState, 4> next;
    std::array<bool, 4> discontinuityPending;
};

using ProgramContinuityState = std::variant<
    VideoContinuityState,
    AudioVideoContinuityState>;

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

} // namespace

struct MediaTsPacketizerControlState final {
    struct ContinuityReservation final {
        std::uint64_t cursorIdentity;
        std::uint8_t nextPayload;
        bool clearsDiscontinuity;
    };

    MediaTsPacketizerControlState(
        MediaTsMuxPlan planValue,
        ProgramContinuityState continuityValue) noexcept
        : plan(std::move(planValue)),
          continuity(std::move(continuityValue))
    {
    }

    MediaTsMuxPlan plan;
    ProgramContinuityState continuity;
    std::mutex mutex;
    std::array<std::optional<std::uint64_t>, 4> activeCursors;
    std::array<std::deque<ContinuityReservation>, 4> reservations;
    std::uint64_t nextCursorIdentity = 1;
    std::vector<std::array<std::uint8_t, PacketSize>> packetWorkspace;
    bool poisoned = false;
};

struct MediaTsPacketCursorState final {
    std::shared_ptr<MediaTsPacketizerControlState> owner;
    std::uint64_t identity;
    std::uint64_t nextRevision = 1;
    CursorPidKind pidKind;
    std::shared_ptr<std::vector<std::array<std::uint8_t, PacketSize>>> packets;
    std::uint8_t initialPayloadContinuity;
    bool advancesPayloadContinuity;
    bool carriesDiscontinuity;
    std::size_t committedOffset = 0;
    struct Pending final {
        std::uint64_t revision;
        std::size_t endOffset;
        std::uint8_t nextPayload;
        bool clearsDiscontinuity;
    };
    std::optional<Pending> pending;
    bool continuityReserved = false;
};

struct MediaTsPacketCursorFactory final {
    static MediaTsPacketCursor create(
        std::unique_ptr<MediaTsPacketCursorState> state) noexcept
    {
        return MediaTsPacketCursor(std::move(state));
    }
};

namespace {

std::size_t continuityIndex(CursorPidKind kind) noexcept
{
    return static_cast<std::size_t>(
        kind == CursorPidKind::Pcr ? CursorPidKind::Video : kind);
}

ContinuityState* continuity(MediaTsPacketizerControlState& state,
                            CursorPidKind kind) noexcept
{
    const std::size_t index = continuityIndex(kind);
    if (auto* video = std::get_if<VideoContinuityState>(&state.continuity)) {
        return index < video->next.size() ? &video->next[index] : nullptr;
    }
    auto* av = std::get_if<AudioVideoContinuityState>(&state.continuity);
    return av && index < av->next.size() ? &av->next[index] : nullptr;
}

bool* discontinuityPending(
    MediaTsPacketizerControlState& state,
    CursorPidKind kind) noexcept
{
    const std::size_t index = continuityIndex(kind);
    if (auto* video = std::get_if<VideoContinuityState>(&state.continuity)) {
        return index < video->discontinuityPending.size()
            ? &video->discontinuityPending[index] : nullptr;
    }
    auto* av = std::get_if<AudioVideoContinuityState>(&state.continuity);
    return av && index < av->discontinuityPending.size()
        ? &av->discontinuityPending[index] : nullptr;
}

std::optional<std::uint64_t>* activeCursor(
    MediaTsPacketizerControlState& state,
    CursorPidKind kind) noexcept
{
    const std::size_t index = continuityIndex(kind);
    return index < state.activeCursors.size()
        ? &state.activeCursors[index]
        : nullptr;
}

const std::optional<std::uint64_t>* activeCursor(
    const MediaTsPacketizerControlState& state,
    CursorPidKind kind) noexcept
{
    const std::size_t index = continuityIndex(kind);
    return index < state.activeCursors.size()
        ? &state.activeCursors[index]
        : nullptr;
}

std::deque<MediaTsPacketizerControlState::ContinuityReservation>*
reservationLedger(MediaTsPacketizerControlState& state,
                  CursorPidKind kind) noexcept
{
    const std::size_t index = continuityIndex(kind);
    return index < state.reservations.size() ? &state.reservations[index]
                                             : nullptr;
}

std::uint8_t reservedNextPayload(MediaTsPacketizerControlState& state,
                                 CursorPidKind kind) noexcept
{
    auto* ledger = reservationLedger(state, kind);
    ContinuityState* committed = continuity(state, kind);
    return ledger && !ledger->empty() ? ledger->back().nextPayload
                                      : committed->nextPayload;
}

bool reservedDiscontinuityPending(MediaTsPacketizerControlState& state,
                                  CursorPidKind kind) noexcept
{
    auto* ledger = reservationLedger(state, kind);
    bool* committed = discontinuityPending(state, kind);
    return ledger && !ledger->empty() && ledger->back().clearsDiscontinuity
        ? false
        : *committed;
}

bool cursorFinishedLocked(const MediaTsPacketCursorState& state) noexcept
{
    return state.packets && !state.pending &&
           state.committedOffset == state.packets->size();
}

std::size_t cursorRemainingPacketCountLocked(
    const MediaTsPacketCursorState& state) noexcept
{
    if (!state.packets || state.committedOffset >= state.packets->size()) {
        return 0;
    }
    return state.packets->size() - state.committedOffset;
}

::media::Result<MediaTsPacketCursor> beginPayloadLocked(
    const std::shared_ptr<MediaTsPacketizerControlState>& state,
    CursorPidKind kind,
    std::uint16_t pid,
    std::span<const std::span<const std::uint8_t>> segments,
    bool randomAccess)
{
    if (!state || state->poisoned) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer was moved from"));
    }
    auto* active = activeCursor(*state, kind);
    if (!active || *active) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer PID already has an active cursor"));
    }
    if (state->nextCursorIdentity == 0 ||
        state->nextCursorIdentity == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS cursor identity space is exhausted"));
    }
    ContinuityState* nextContinuity = continuity(*state, kind);
    bool* pendingDiscontinuity = discontinuityPending(*state, kind);
    if (!nextContinuity || !pendingDiscontinuity) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS stream is absent from the typed program plan"));
    }
    const std::uint8_t initialContinuity = reservedNextPayload(*state, kind);
    const bool discontinuity = reservedDiscontinuityPending(*state, kind);
    auto packets = MediaTsTransportPacketBuilder::payload(
        pid, initialContinuity, segments, randomAccess,
        discontinuity, std::move(state->packetWorkspace));
    if (!packets) {
        return ::media::Result<MediaTsPacketCursor>::failure(packets.error());
    }
    const std::uint64_t identity = state->nextCursorIdentity;
    auto cursor = std::make_unique<MediaTsPacketCursorState>();
    cursor->owner = state;
    cursor->identity = identity;
    cursor->pidKind = kind;
    cursor->initialPayloadContinuity = initialContinuity;
    cursor->advancesPayloadContinuity = true;
    cursor->carriesDiscontinuity = discontinuity;
    cursor->packets = std::make_shared<std::vector<std::array<std::uint8_t, PacketSize>>>(
        std::move(packets).value());
    ++state->nextCursorIdentity;
    *active = identity;
    return ::media::Result<MediaTsPacketCursor>::success(
        MediaTsPacketCursorFactory::create(std::move(cursor)));
}

} // namespace

MediaTsPacketCommitToken::MediaTsPacketCommitToken(
    std::weak_ptr<MediaTsPacketizerControlState> owner,
    std::uint64_t cursorIdentity,
    std::uint64_t revision) noexcept
    : m_owner(std::move(owner)),
      m_cursorIdentity(cursorIdentity),
      m_revision(revision),
      m_valid(true)
{
}

MediaTsPacketCommitToken::MediaTsPacketCommitToken(
    MediaTsPacketCommitToken&& other) noexcept
    : m_owner(std::move(other.m_owner)),
      m_cursorIdentity(std::exchange(other.m_cursorIdentity, 0)),
      m_revision(std::exchange(other.m_revision, 0)),
      m_valid(std::exchange(other.m_valid, false))
{
}

MediaTsPacketCommitToken& MediaTsPacketCommitToken::operator=(
    MediaTsPacketCommitToken&& other) noexcept
{
    if (this != &other) {
        m_owner = std::move(other.m_owner);
        m_cursorIdentity = std::exchange(other.m_cursorIdentity, 0);
        m_revision = std::exchange(other.m_revision, 0);
        m_valid = std::exchange(other.m_valid, false);
    }
    return *this;
}

MediaTsPreparedPacketBatch::MediaTsPreparedPacketBatch(
    std::shared_ptr<const PacketStorage> storage,
    std::size_t begin,
    std::size_t count,
    MediaTsPacketCommitToken commitToken) noexcept
    : m_storage(std::move(storage)),
      m_begin(begin),
      m_count(count),
      m_commitToken(std::move(commitToken))
{
}

std::span<const std::array<std::uint8_t, 188>>
MediaTsPreparedPacketBatch::packets() const noexcept
{
    if (!m_storage) return {};
    return std::span<const std::array<std::uint8_t, 188>>(*m_storage).subspan(
        m_begin, m_count);
}

MediaTsPacketCommitToken MediaTsPreparedPacketBatch::takeCommitToken() noexcept
{
    return std::move(m_commitToken);
}

MediaTsPreparedPacketSeries::MediaTsPreparedPacketSeries(
    std::shared_ptr<const PacketStorage> storage,
    std::size_t begin,
    std::size_t count,
    MediaTsPacketCommitToken commitToken) noexcept
    : m_storage(std::move(storage)),
      m_begin(begin),
      m_count(count),
      m_commitToken(std::move(commitToken))
{
}

std::span<const std::array<std::uint8_t, 188>>
MediaTsPreparedPacketSeries::packets() const noexcept
{
    if (!m_storage) return {};
    return std::span<const std::array<std::uint8_t, 188>>(*m_storage).subspan(
        m_begin, m_count);
}

MediaTsPacketCommitToken
MediaTsPreparedPacketSeries::takeCommitToken() noexcept
{
    return std::move(m_commitToken);
}

MediaTsPacketCursor::MediaTsPacketCursor(
    std::unique_ptr<MediaTsPacketCursorState> state) noexcept
    : m_state(std::move(state))
{
}

MediaTsPacketCursor::~MediaTsPacketCursor()
{
    cancel();
}

MediaTsPacketCursor::MediaTsPacketCursor(MediaTsPacketCursor&& other) noexcept
    : m_state(std::move(other.m_state))
{
}

MediaTsPacketCursor& MediaTsPacketCursor::operator=(
    MediaTsPacketCursor&& other) noexcept
{
    if (this != &other) {
        cancel();
        m_state = std::move(other.m_state);
    }
    return *this;
}

void MediaTsPacketCursor::cancel() noexcept
{
    if (m_state && m_state->owner) {
        const auto owner = m_state->owner;
        const std::lock_guard lock(owner->mutex);
        if (m_state->continuityReserved && m_state->pending) {
            owner->poisoned = true;
        }
        if (m_state->packets && m_state->packets.use_count() == 1) {
            owner->packetWorkspace =
                std::move(*m_state->packets);
        }
        auto* active = activeCursor(*owner, m_state->pidKind);
        if (active && *active == m_state->identity) {
            active->reset();
        }
    }
    m_state.reset();
}

::media::Result<MediaTsPreparedPacketBatch> MediaTsPacketCursor::prepare(
    std::size_t maximumPackets)
{
    if (!m_state || !m_state->owner) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet cursor cannot prepare from poisoned state"));
    }
    const auto owner = m_state->owner;
    const std::lock_guard lock(owner->mutex);
    if (owner->poisoned) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet cursor cannot prepare from poisoned state"));
    }
    if (maximumPackets < 1 ||
        maximumPackets > owner->plan.parameters().maximumPacketsPerDatagram) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet batch limit is outside the mux plan"));
    }
    if (m_state->pending) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet cursor already has a prepared batch"));
    }
    if (cursorFinishedLocked(*m_state)) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet cursor is finished"));
    }
    if (m_state->nextRevision == 0 ||
        m_state->nextRevision == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet revision space is exhausted"));
    }
    const std::size_t count = std::min(
        maximumPackets, m_state->packets->size() - m_state->committedOffset);
    const std::size_t end = m_state->committedOffset + count;
    const std::uint64_t revision = m_state->nextRevision++;
    const std::uint8_t nextPayload = m_state->advancesPayloadContinuity
        ? static_cast<std::uint8_t>(
              (m_state->initialPayloadContinuity + end) & 0x0F)
        : m_state->initialPayloadContinuity;
    m_state->pending = MediaTsPacketCursorState::Pending{
        revision, end, nextPayload,
        m_state->carriesDiscontinuity &&
            m_state->committedOffset == 0};
    return ::media::Result<MediaTsPreparedPacketBatch>::success(
        MediaTsPreparedPacketBatch(
            m_state->packets, m_state->committedOffset, count,
            MediaTsPacketCommitToken(m_state->owner, m_state->identity, revision)));
}

::media::Result<MediaTsPreparedPacketSeries>
MediaTsPacketCursor::prepareRemaining()
{
    if (!m_state || !m_state->owner) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS packet cursor cannot reserve from poisoned state"));
    }
    const auto owner = m_state->owner;
    const std::lock_guard lock(owner->mutex);
    if (owner->poisoned) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS packet cursor cannot reserve from poisoned state"));
    }
    if (m_state->pending || cursorFinishedLocked(*m_state) ||
        m_state->nextRevision == 0 ||
        m_state->nextRevision == (std::numeric_limits<std::uint64_t>::max)()) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS packet cursor cannot reserve its remaining series"));
    }
    const std::size_t begin = m_state->committedOffset;
    const std::size_t end = m_state->packets->size();
    const std::uint64_t revision = m_state->nextRevision++;
    const std::uint8_t nextPayload = m_state->advancesPayloadContinuity
        ? static_cast<std::uint8_t>(
              (m_state->initialPayloadContinuity + end) & 0x0F)
        : m_state->initialPayloadContinuity;
    const bool clearsDiscontinuity =
        m_state->carriesDiscontinuity && begin == 0;
    auto* ledger = reservationLedger(*owner, m_state->pidKind);
    auto* active = activeCursor(*owner, m_state->pidKind);
    if (!ledger || !active || *active != m_state->identity) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS packet cursor lost its reservation authority"));
    }
    try {
        ledger->push_back(
            {m_state->identity, nextPayload, clearsDiscontinuity});
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MPEG-TS continuity reservation ledger"));
    }
    m_state->pending = MediaTsPacketCursorState::Pending{
        revision, end, nextPayload, clearsDiscontinuity};
    m_state->continuityReserved = true;
    active->reset();
    return ::media::Result<MediaTsPreparedPacketSeries>::success(
        MediaTsPreparedPacketSeries(
            m_state->packets, begin, end - begin,
            MediaTsPacketCommitToken(
                m_state->owner, m_state->identity, revision)));
}

void MediaTsPacketCursor::poison() noexcept
{
    if (m_state && m_state->owner) {
        const auto owner = m_state->owner;
        const std::lock_guard lock(owner->mutex);
        owner->poisoned = true;
    }
}

::media::Status MediaTsPacketCursor::commit(MediaTsPacketCommitToken token)
{
    if (!m_state || !m_state->owner) {
        return ::media::Status::failure(
            invalid("MPEG-TS packet commit token is not valid for a prepared batch"));
    }
    const auto owner = m_state->owner;
    auto tokenOwner = token.m_owner.lock();
    const std::lock_guard lock(owner->mutex);
    if (owner->poisoned || !m_state->pending || !token.m_valid) {
        return ::media::Status::failure(
            invalid("MPEG-TS packet commit token is not valid for a prepared batch"));
    }
    const auto* active = activeCursor(*owner, m_state->pidKind);
    auto* ledger = reservationLedger(*owner, m_state->pidKind);
    const bool reservationMatches = m_state->continuityReserved
        ? ledger && !ledger->empty() &&
            ledger->front().cursorIdentity == m_state->identity
        : active && *active == m_state->identity;
    if (!tokenOwner || tokenOwner.get() != owner.get() ||
        token.m_cursorIdentity != m_state->identity ||
        token.m_revision != m_state->pending->revision ||
        !reservationMatches) {
        owner->poisoned = true;
        return ::media::Status::failure(
            invalid("MPEG-TS packet commit token is stale, reordered, or has a continuity gap"));
    }
    ContinuityState* nextContinuity = continuity(*owner, m_state->pidKind);
    bool* pendingDiscontinuity = discontinuityPending(
        *owner, m_state->pidKind);
    if (!nextContinuity || !pendingDiscontinuity) {
        return ::media::Status::failure(
            invalid("MPEG-TS packet commit stream is absent from its plan"));
    }
    nextContinuity->nextPayload = m_state->pending->nextPayload;
    if (m_state->pending->clearsDiscontinuity) {
        *pendingDiscontinuity = false;
    }
    if (m_state->continuityReserved) {
        ledger->pop_front();
    }
    m_state->committedOffset = m_state->pending->endOffset;
    m_state->pending.reset();
    if (cursorFinishedLocked(*m_state) && !m_state->continuityReserved) {
        activeCursor(*owner, m_state->pidKind)->reset();
    }
    return ::media::Status::success();
}

bool MediaTsPacketCursor::finished() const noexcept
{
    if (!m_state || !m_state->owner) return false;
    const std::lock_guard lock(m_state->owner->mutex);
    return cursorFinishedLocked(*m_state);
}

std::size_t MediaTsPacketCursor::remainingPacketCount() const noexcept
{
    if (!m_state || !m_state->owner) return 0;
    const std::lock_guard lock(m_state->owner->mutex);
    return cursorRemainingPacketCountLocked(*m_state);
}

::media::Result<MediaTsTransportPacketizer> MediaTsTransportPacketizer::create(
    const MediaTsMuxPlan& plan,
    bool startsWithDiscontinuity)
{
    ProgramContinuityState continuityState;
    if (const auto* video = plan.videoOnlyProgram()) {
        continuityState.emplace<VideoContinuityState>(
            VideoContinuityState{
                {{{video->continuity.pat}, {video->continuity.pmt},
                  {video->continuity.video}}},
                {startsWithDiscontinuity, startsWithDiscontinuity,
                 startsWithDiscontinuity}});
    } else if (const auto* av = plan.audioVideoProgram()) {
        continuityState.emplace<AudioVideoContinuityState>(
            AudioVideoContinuityState{
                {{{av->continuity.pat}, {av->continuity.pmt},
                  {av->continuity.video}, {av->continuity.audio}}},
                {startsWithDiscontinuity, startsWithDiscontinuity,
                 startsWithDiscontinuity, startsWithDiscontinuity}});
    } else {
        return ::media::Result<MediaTsTransportPacketizer>::failure(
            invalid("MPEG-TS packetizer requires a typed program plan"));
    }
    auto state = std::make_shared<MediaTsPacketizerControlState>(
        plan, std::move(continuityState));
    return ::media::Result<MediaTsTransportPacketizer>::success(
        MediaTsTransportPacketizer(std::move(state)));
}

MediaTsTransportPacketizer::MediaTsTransportPacketizer(
    std::shared_ptr<MediaTsPacketizerControlState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Result<std::size_t> MediaTsTransportPacketizer::patPacketCount(
    const MediaTsPatSection& section) const
{
    if (!m_state) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PAT packet count cannot be inspected"));
    }
    const std::lock_guard lock(m_state->mutex);
    const auto* active = m_state
        ? activeCursor(*m_state, CursorPidKind::Pat)
        : nullptr;
    if (!m_state || !active || *active || !section.matches(m_state->plan)) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PAT packet count cannot be inspected"));
    }
    const bool* pendingDiscontinuity = discontinuityPending(
        *m_state, CursorPidKind::Pat);
    if (!pendingDiscontinuity) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PAT continuity state is absent"));
    }
    const std::array<std::uint8_t, 1> pointerField{0};
    const std::array<std::span<const std::uint8_t>, 2> segments{
        pointerField, section.bytes()};
    return MediaTsTransportPacketBuilder::payloadPacketCount(
        segments, false, *pendingDiscontinuity);
}

::media::Result<std::size_t> MediaTsTransportPacketizer::pmtPacketCount(
    const MediaTsPmtSection& section) const
{
    if (!m_state) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PMT packet count cannot be inspected"));
    }
    const std::lock_guard lock(m_state->mutex);
    const auto* active = m_state
        ? activeCursor(*m_state, CursorPidKind::Pmt)
        : nullptr;
    if (!m_state || !active || *active || !section.matches(m_state->plan)) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PMT packet count cannot be inspected"));
    }
    const bool* pendingDiscontinuity = discontinuityPending(
        *m_state, CursorPidKind::Pmt);
    if (!pendingDiscontinuity) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PMT continuity state is absent"));
    }
    const std::array<std::uint8_t, 1> pointerField{0};
    const std::array<std::span<const std::uint8_t>, 2> segments{
        pointerField, section.bytes()};
    return MediaTsTransportPacketBuilder::payloadPacketCount(
        segments, false, *pendingDiscontinuity);
}

::media::Result<std::size_t> MediaTsTransportPacketizer::pcrOnlyPacketCount(
    const MediaTsPcrClock& pcr) const
{
    if (!m_state) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PCR packet count cannot be inspected"));
    }
    const std::lock_guard lock(m_state->mutex);
    const auto* active = m_state
        ? activeCursor(*m_state, CursorPidKind::Pcr)
        : nullptr;
    if (!m_state || !active || *active) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PCR packet count cannot be inspected"));
    }
    ContinuityState* nextState = continuity(*m_state, CursorPidKind::Pcr);
    bool* pendingDiscontinuity = discontinuityPending(
        *m_state, CursorPidKind::Pcr);
    if (!nextState || !pendingDiscontinuity) {
        return ::media::Result<std::size_t>::failure(
            invalid("MPEG-TS PCR continuity state is absent"));
    }
    auto packet = MediaTsTransportPacketBuilder::pcrOnly(
        m_state->plan.pcrPid(), nextState->nextPayload,
        pcr.wire27Mhz, *pendingDiscontinuity);
    if (!packet) {
        return ::media::Result<std::size_t>::failure(packet.error());
    }
    return ::media::Result<std::size_t>::success(1);
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPat(
    const MediaTsPatSection& section)
{
    if (!m_state) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PAT section does not match the packetizer plan"));
    }
    const std::lock_guard lock(m_state->mutex);
    if (m_state->poisoned || !section.matches(m_state->plan)) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PAT section does not match the packetizer plan"));
    }
    const std::array<std::uint8_t, 1> pointerField{0};
    const std::array<std::span<const std::uint8_t>, 2> segments{
        pointerField, section.bytes()};
    return beginPayloadLocked(m_state, CursorPidKind::Pat,
                        m_state->plan.parameters().patPid,
                        segments, false);
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPmt(
    const MediaTsPmtSection& section)
{
    if (!m_state) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PMT section does not match the packetizer plan"));
    }
    const std::lock_guard lock(m_state->mutex);
    if (m_state->poisoned || !section.matches(m_state->plan)) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PMT section does not match the packetizer plan"));
    }
    const std::array<std::uint8_t, 1> pointerField{0};
    const std::array<std::span<const std::uint8_t>, 2> segments{
        pointerField, section.bytes()};
    return beginPayloadLocked(m_state, CursorPidKind::Pmt,
                        m_state->plan.parameters().programMapPid,
                        segments, false);
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPcrOnly(
    const MediaTsPcrClock& pcr)
{
    if (!m_state) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer was moved from"));
    }
    const std::lock_guard lock(m_state->mutex);
    if (m_state->poisoned) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer was moved from"));
    }
    auto* active = activeCursor(*m_state, CursorPidKind::Pcr);
    if (!active || *active) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PCR cursor cannot begin"));
    }
    if (m_state->nextCursorIdentity == 0 ||
        m_state->nextCursorIdentity == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS cursor identity space is exhausted"));
    }
    const auto kind = CursorPidKind::Pcr;
    const std::uint64_t identity = m_state->nextCursorIdentity;
    auto cursor = std::make_unique<MediaTsPacketCursorState>();
    cursor->owner = m_state;
    cursor->identity = identity;
    cursor->pidKind = kind;
    ContinuityState* nextState = continuity(*m_state, kind);
    bool* pendingDiscontinuity = discontinuityPending(*m_state, kind);
    if (!nextState || !pendingDiscontinuity) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PCR authority is absent from the program"));
    }
    const auto next = reservedNextPayload(*m_state, kind);
    cursor->carriesDiscontinuity =
        reservedDiscontinuityPending(*m_state, kind);
    auto packet = MediaTsTransportPacketBuilder::pcrOnly(
        m_state->plan.pcrPid(), next, pcr.wire27Mhz,
        cursor->carriesDiscontinuity);
    if (!packet) {
        return ::media::Result<MediaTsPacketCursor>::failure(packet.error());
    }
    m_state->packetWorkspace.clear();
    m_state->packetWorkspace.push_back(std::move(packet).value());
    cursor->initialPayloadContinuity = next;
    cursor->advancesPayloadContinuity = false;
    cursor->packets = std::make_shared<
        std::vector<std::array<std::uint8_t, PacketSize>>>(
            std::move(m_state->packetWorkspace));
    ++m_state->nextCursorIdentity;
    *active = identity;
    return ::media::Result<MediaTsPacketCursor>::success(
        MediaTsPacketCursor(std::move(cursor)));
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPes(
    MediaScheduledStream stream,
    const MediaTsPesHeader& header,
    std::span<const std::uint8_t> payload,
    bool randomAccess)
{
    if (!m_state) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer is poisoned"));
    }
    const std::lock_guard lock(m_state->mutex);
    if (m_state->poisoned) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer is poisoned"));
    }
    CursorPidKind kind;
    std::uint16_t pid = 0;
    switch (stream) {
    case MediaScheduledStream::Video:
        kind = CursorPidKind::Video;
        if (m_state) pid = m_state->plan.videoPid();
        break;
    case MediaScheduledStream::Audio:
        if (randomAccess) {
            return ::media::Result<MediaTsPacketCursor>::failure(
                invalid("MPEG-TS audio PES cannot be random access"));
        }
        kind = CursorPidKind::Audio;
        if (!m_state || !m_state->plan.audioVideoProgram()) {
            return ::media::Result<MediaTsPacketCursor>::failure(
                invalid("MPEG-TS audio PES is absent from the typed program"));
        }
        pid = m_state->plan.audioVideoProgram()->audioPid;
        break;
    default:
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PES stream is invalid"));
    }
    if (header.stream() != stream) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PES header stream does not match payload stream"));
    }
    if (payload.size() != header.framedPayloadBytes()) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PES payload size does not match its serialized header"));
    }
    const std::array<std::span<const std::uint8_t>, 2> segments{
        header.bytes(), payload};
    return beginPayloadLocked(m_state, kind, pid, segments, randomAccess);
}

} // namespace media::ffmpeg::graph
