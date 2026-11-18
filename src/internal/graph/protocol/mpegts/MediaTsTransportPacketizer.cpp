#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketBuilder.h"

#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"

#include <algorithm>

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
        std::uint64_t cursorRevision;
        // Reservation publication order across all PIDs, not wire delivery.
        std::uint64_t publicationIdentity;
        MediaTsPacketReservationWork work;
        std::uint8_t nextPayload;
        bool clearsDiscontinuity;
        std::optional<MediaTsPreparedWireWork> preparedWire;
        std::optional<MediaTsBoundWireWork> boundWire;
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
    std::array<std::optional<std::uint64_t>, 4> latestPayloadCursors;
    std::array<MediaFixedCapacityStorage<ContinuityReservation>, 4> reservations;
    MediaFixedCapacityStorage<MediaTsAcceptedReservationObservation> observationScratch;
    bool observationBorrowed = false;
    MediaTsSeriesStoragePlan storagePlan;
    std::shared_ptr<MediaNetworkMemoryResource> resource;
    std::uint64_t nextCursorIdentity = 1;
    std::uint64_t nextPublicationIdentity = 1;
    std::size_t liveSeries = 0;
    std::shared_ptr<MediaFixedCapacityStorage<std::array<std::uint8_t, PacketSize>>> packetWorkspace;
    bool poisoned = false;
};

struct MediaTsPacketCursorState final {
    std::shared_ptr<MediaTsPacketizerControlState> owner;
    std::uint64_t identity;
    std::uint64_t nextRevision = 1;
    CursorPidKind pidKind;
    std::shared_ptr<MediaFixedCapacityStorage<std::array<std::uint8_t, PacketSize>>> packets;
    std::optional<MediaTsSeriesStoragePreparation> preparedStorage;
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
    bool counted = false;
};

MediaTsAcceptedWorkRead::MediaTsAcceptedWorkRead(
    std::shared_ptr<MediaTsPacketizerControlState> owner, std::unique_lock<std::mutex> lock) noexcept
    : m_owner(std::move(owner)), m_lock(std::move(lock)) {}

MediaTsAcceptedWorkRead::~MediaTsAcceptedWorkRead()
{
    if (!m_owner) return;
    if (!m_lock.owns_lock()) m_lock.lock();
    m_owner->observationBorrowed = false;
}

std::span<MediaTsAcceptedReservationObservation> MediaTsAcceptedWorkRead::unlockForPlanning()
{
    auto records = m_owner->observationScratch.view();
    m_lock.unlock();
    return records;
}

::media::Result<MediaTsAcceptedWorkRead> MediaTsTransportPacketizer::lockAcceptedWork()
{
    using Result = ::media::Result<MediaTsAcceptedWorkRead>;
    if (!m_state) return Result::failure(invalid("TS accepted-work observation has no owner"));
    std::unique_lock lock(m_state->mutex);
    if (m_state->poisoned || m_state->observationBorrowed)
        return Result::failure(invalid("TS accepted-work observation requires its live exclusive owner"));
    auto& scratch = m_state->observationScratch;
    scratch.clear();
    for (const auto& ledger : m_state->reservations) {
        for (const auto& record : ledger) {
            if (scratch.size() == scratch.capacity())
                return Result::failure(invalid("TS accepted-work observation exceeds live cursor capacity"));
            scratch.push_back({{record.cursorIdentity, record.cursorRevision, record.publicationIdentity},
                record.work, record.preparedWire, record.boundWire});
        }
    }
    m_state->observationBorrowed = true;
    return Result::success(MediaTsAcceptedWorkRead(m_state, std::move(lock)));
}

MediaSharedAllocationGeometry MediaTsTransportPacketizer::acceptedObservationGeometry() noexcept
{
    return {1, sizeof(MediaTsAcceptedReservationObservation), alignof(MediaTsAcceptedReservationObservation)};
}

void MediaTsPacketCursorDeleter::operator()(MediaTsPacketCursorState* state) const noexcept
{
    if (!state) return;
    std::destroy_at(state);
    resource->deallocate(state, sizeof(MediaTsPacketCursorState), alignof(MediaTsPacketCursorState));
}

struct MediaTsPacketCursorFactory final {
    using CursorOwner = std::unique_ptr<MediaTsPacketCursorState, MediaTsPacketCursorDeleter>;
    static MediaTsPacketCursor create(CursorOwner state) noexcept
    { return MediaTsPacketCursor(std::move(state)); }

    static ::media::Result<CursorOwner> prepare(
        const std::shared_ptr<MediaTsPacketizerControlState>& state, std::size_t packets,
        std::optional<MediaTsSeriesStoragePreparation> groupStorage)
    {
        using Result = ::media::Result<CursorOwner>;
        using PacketStorage = MediaFixedCapacityStorage<std::array<std::uint8_t, PacketSize>>;
        const auto& plan = state->storagePlan;
        if (state->liveSeries >= plan.maximumLiveSeries)
            return Result::failure(invalid("TS live series exceed the final holder contract"));
        if (!packets || packets > plan.maximumPackets)
            return Result::failure(invalid("TS packet series exceeds its storage contract"));
        // Reserve the descriptor maximum: packetization may precede timing binding.
        auto shape = MediaTsSeriesStoragePreparation::geometry(plan.packetizerGeometry,
            plan.protocolGeometry, packets, (std::min)(packets, plan.maximumDatagrams));
        if (!shape) return Result::failure(shape.error());
        const bool reuse = !groupStorage && state->packetWorkspace && state->packetWorkspace.use_count() == 1 &&
            state->packetWorkspace->capacity() == packets;
        auto storage = groupStorage
            ? ::media::Result<MediaTsSeriesStoragePreparation>::success(std::move(*groupStorage))
            : MediaTsSeriesStoragePreparation::prepare(state->resource, shape.value(), reuse);
        if (!storage) return Result::failure(storage.error());
        auto prepared = std::move(storage).value();
        for (std::size_t index = 0; index < shape.value().size(); ++index) {
            const bool reused = reuse && (index == static_cast<std::size_t>(MediaTsSeriesStorageRole::Packets) ||
                index == static_cast<std::size_t>(MediaTsSeriesStorageRole::PacketOwner));
            const auto& block = prepared.blocks[index];
            const auto& expected = shape.value()[index];
            if (reused ? block.has_value() : (!block || !*block ||
                block->resource() != state->resource.get() || block->bytes() != expected.bytes ||
                block->alignment() != expected.alignment))
                return Result::failure(invalid("TS series prepared storage differs from its exact role geometry"));
        }
        std::shared_ptr<PacketStorage> packetOwner;
        if (reuse) {
            packetOwner = state->packetWorkspace;
        } else {
            auto backing = PacketStorage::fromAllocation(prepared.take(MediaTsSeriesStorageRole::Packets));
            if (!backing) return Result::failure(backing.error());
            auto control = prepared.take(MediaTsSeriesStorageRole::PacketOwner);
            packetOwner = std::allocate_shared<PacketStorage>(
                MediaSharedAllocationAllocator<PacketStorage>{MediaSharedAllocationAdmitted{
                    state->resource, plan.packetizerGeometry.packetOwner, &control}},
                std::move(backing).value());
        }
        auto allocation = prepared.take(MediaTsSeriesStorageRole::Cursor);
        auto* pointer = std::construct_at(static_cast<MediaTsPacketCursorState*>(allocation.data()));
        CursorOwner cursor(pointer, MediaTsPacketCursorDeleter{allocation.resource()});
        (void)allocation.release();
        cursor->owner = state;
        cursor->packets = std::move(packetOwner);
        cursor->preparedStorage.emplace(std::move(prepared));
        // No failing allocation remains before ownership is transferred.
        state->packetWorkspace.reset();
        cursor->packets->clear();
        for (std::size_t index = 0; index < packets; ++index) cursor->packets->push_back({});
        return Result::success(std::move(cursor));
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

MediaFixedCapacityStorage<MediaTsPacketizerControlState::ContinuityReservation>*
reservationLedger(MediaTsPacketizerControlState& state,
                  CursorPidKind kind) noexcept
{
    const std::size_t index = continuityIndex(kind);
    return index < state.reservations.size() ? &state.reservations[index]
                                             : nullptr;
}

MediaTsPacketizerControlState::ContinuityReservation* findReservation(
    MediaTsPacketCursorState& cursor, const MediaTsPacketReservationIdentity& identity) noexcept
{
    if (cursor.identity != identity.cursorIdentity || !cursor.pending ||
        cursor.pending->revision != identity.cursorRevision) return nullptr;
    auto* ledger = reservationLedger(*cursor.owner, cursor.pidKind);
    if (!ledger) return nullptr;
    for (auto& record : *ledger) {
        if (record.cursorIdentity == identity.cursorIdentity &&
            record.cursorRevision == identity.cursorRevision &&
            record.publicationIdentity == identity.publicationIdentity) return &record;
    }
    return nullptr;
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
    if (!*committed) return false;
    // Entries record local clear events, not a cumulative projected value.
    // A later no-op cannot restore a flag consumed by an earlier reservation.
    if (ledger && std::any_of(ledger->begin(), ledger->end(),
            [](const auto& reservation) { return reservation.clearsDiscontinuity; }))
        return false;
    return *committed;
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
    bool randomAccess, std::optional<MediaTsSeriesStoragePreparation> groupStorage) try
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
    auto packetCount = MediaTsTransportPacketBuilder::payloadPacketCount(
        segments, randomAccess, discontinuity);
    if (!packetCount) {
        return ::media::Result<MediaTsPacketCursor>::failure(packetCount.error());
    }
    auto preparedCursor = MediaTsPacketCursorFactory::prepare(state, packetCount.value(), std::move(groupStorage));
    if (!preparedCursor) return ::media::Result<MediaTsPacketCursor>::failure(preparedCursor.error());
    auto cursor = std::move(preparedCursor).value();
    auto packets = MediaTsTransportPacketBuilder::payload(
        pid, initialContinuity, segments, randomAccess,
        discontinuity, cursor->packets->view());
    if (!packets) return ::media::Result<MediaTsPacketCursor>::failure(packets.error());
    const std::uint64_t identity = state->nextCursorIdentity;
    cursor->identity = identity;
    cursor->pidKind = kind;
    cursor->initialPayloadContinuity = initialContinuity;
    cursor->advancesPayloadContinuity = true;
    cursor->carriesDiscontinuity = discontinuity;
    ++state->nextCursorIdentity;
    ++state->liveSeries;
    cursor->counted = true;
    *active = identity;
    state->latestPayloadCursors[continuityIndex(kind)] = identity;
    return ::media::Result<MediaTsPacketCursor>::success(
        MediaTsPacketCursorFactory::create(std::move(cursor)));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaTsPacketCursor>::failure(
        ::media::ErrorInfo::allocationFailed("MPEG-TS payload cursor preparation"));
}

} // namespace

MediaTsPacketCommitToken::MediaTsPacketCommitToken(
    std::weak_ptr<MediaTsPacketizerControlState> owner,
    std::uint64_t cursorIdentity,
    std::uint64_t revision,
    std::optional<std::uint64_t> publicationIdentity) noexcept
    : m_owner(std::move(owner)),
      m_cursorIdentity(cursorIdentity),
      m_revision(revision),
      m_publicationIdentity(publicationIdentity),
      m_valid(true)
{
}

MediaTsPacketCommitToken::MediaTsPacketCommitToken(
    MediaTsPacketCommitToken&& other) noexcept
    : m_owner(std::move(other.m_owner)),
      m_cursorIdentity(std::exchange(other.m_cursorIdentity, 0)),
      m_revision(std::exchange(other.m_revision, 0)),
      m_publicationIdentity(std::exchange(other.m_publicationIdentity, std::nullopt)),
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
        m_publicationIdentity = std::exchange(other.m_publicationIdentity, std::nullopt);
        m_valid = std::exchange(other.m_valid, false);
    }
    return *this;
}

std::optional<MediaTsPacketReservationIdentity>
MediaTsPacketCommitToken::reservationIdentity() const noexcept
{
    if (!m_valid || !m_publicationIdentity) return std::nullopt;
    return MediaTsPacketReservationIdentity{
        m_cursorIdentity, m_revision, *m_publicationIdentity};
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
    return m_storage->view().subspan(
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
    return m_storage->view().subspan(
        m_begin, m_count);
}

MediaTsPacketCommitToken
MediaTsPreparedPacketSeries::takeCommitToken() noexcept
{
    return std::move(m_commitToken);
}

MediaTsPacketCursor::MediaTsPacketCursor(
    std::unique_ptr<MediaTsPacketCursorState, MediaTsPacketCursorDeleter> state) noexcept
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
        if (m_state->counted) {
            --owner->liveSeries;
            m_state->counted = false;
        }
        if ((m_state->continuityReserved && m_state->pending) ||
            (m_state->committedOffset != 0 && !cursorFinishedLocked(*m_state))) {
            owner->poisoned = true;
        }
        if (m_state->packets && m_state->packets.use_count() == 1) {
            owner->packetWorkspace = std::move(m_state->packets);
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
    const std::uint64_t revision = m_state->nextRevision;
    const std::uint8_t nextPayload = m_state->advancesPayloadContinuity
        ? static_cast<std::uint8_t>(
              (m_state->initialPayloadContinuity + end) & 0x0F)
        : m_state->initialPayloadContinuity;
    m_state->pending = MediaTsPacketCursorState::Pending{
        revision, end, nextPayload,
        m_state->carriesDiscontinuity &&
            m_state->committedOffset == 0};
    ++m_state->nextRevision;
    return ::media::Result<MediaTsPreparedPacketBatch>::success(
        MediaTsPreparedPacketBatch(
            m_state->packets, m_state->committedOffset, count,
            MediaTsPacketCommitToken(m_state->owner, m_state->identity, revision, std::nullopt)));
}

::media::Result<MediaTsPreparedPacketSeries>
MediaTsPacketCursor::prepareSeries(const MediaTsPacketReservationWork& work)
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
        m_state->nextRevision == (std::numeric_limits<std::uint64_t>::max)() ||
        owner->nextPublicationIdentity == 0 ||
        owner->nextPublicationIdentity == (std::numeric_limits<std::uint64_t>::max)()) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS packet cursor cannot reserve its remaining series"));
    }
    const std::size_t begin = m_state->committedOffset;
    if (!work.generation || work.release.nanoseconds() < 0 ||
        work.release >= work.completionLimit || work.completionLimit > work.deadline ||
        !work.packetCount || !work.datagramCount || !work.maximumPacketsPerDatagram ||
        work.maximumPacketsPerDatagram != owner->plan.parameters().maximumPacketsPerDatagram ||
        work.packetCount > m_state->packets->size() - begin ||
        work.datagramCount > (std::numeric_limits<std::size_t>::max)() - work.protocolBegin ||
        work.packetCount / work.maximumPacketsPerDatagram +
            (work.packetCount % work.maximumPacketsPerDatagram != 0) != work.datagramCount) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS reservation work differs from its exact packet geometry or timing"));
    }
    const std::size_t end = begin + work.packetCount;
    const std::uint64_t revision = m_state->nextRevision;
    const std::uint8_t nextPayload = m_state->advancesPayloadContinuity
        ? static_cast<std::uint8_t>(
              (m_state->initialPayloadContinuity + end) & 0x0F)
        : m_state->initialPayloadContinuity;
    bool clearsDiscontinuity =
        m_state->carriesDiscontinuity && begin == 0;
    auto* ledger = reservationLedger(*owner, m_state->pidKind);
    auto* active = activeCursor(*owner, m_state->pidKind);
    // A committed bounded prefix releases active ownership. Resume only if
    // no other cursor owns construction and no payload reservation overtook
    // this cursor. Adaptation-only PCR reservations do not advance this value.
    const bool resuming = active && !*active &&
        m_state->advancesPayloadContinuity &&
        owner->latestPayloadCursors[continuityIndex(m_state->pidKind)] == m_state->identity;
    const auto expectedNext = static_cast<std::uint8_t>(
        (m_state->initialPayloadContinuity + begin) & 0x0F);
    if (!ledger || !active ||
        (*active != m_state->identity && !resuming) ||
        (resuming && reservedNextPayload(*owner, m_state->pidKind) != expectedNext)) {
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS packet cursor lost its reservation authority"));
    }
    if (ledger->size() == ledger->capacity())
        return ::media::Result<MediaTsPreparedPacketSeries>::failure(
            invalid("MPEG-TS continuity storage violates its live series contract"));
    if (clearsDiscontinuity && !reservedDiscontinuityPending(*owner, m_state->pidKind)) {
        // Only the first, never-published payload can be adjusted. A PCR
        // reserved during deferred construction now owns the first flag.
        auto cleared = MediaTsTransportPacketBuilder::clearPreparedDiscontinuity(
            m_state->packets->view().front());
        if (!cleared) return ::media::Result<MediaTsPreparedPacketSeries>::failure(cleared.error());
        m_state->carriesDiscontinuity = false;
        clearsDiscontinuity = false;
    }
    const auto publicationIdentity = owner->nextPublicationIdentity;
    ledger->push_back({m_state->identity, revision, publicationIdentity, work,
                       nextPayload, clearsDiscontinuity});
    ++owner->nextPublicationIdentity;
    ++m_state->nextRevision;
    m_state->pending = MediaTsPacketCursorState::Pending{
        revision, end, nextPayload, clearsDiscontinuity};
    m_state->continuityReserved = true;
    active->reset();
    return ::media::Result<MediaTsPreparedPacketSeries>::success(
        MediaTsPreparedPacketSeries(
            m_state->packets, begin, end - begin,
            MediaTsPacketCommitToken(
                m_state->owner, m_state->identity, revision, publicationIdentity)));
}

::media::Status MediaTsPacketCursor::registerPreparedWire(
    const MediaTsPacketReservationIdentity& identity, const MediaTsPreparedWireWork& work)
{
    if (!m_state || !m_state->owner)
        return ::media::Status::failure(invalid("MPEG-TS prepared wire work has no owner"));
    const std::lock_guard lock(m_state->owner->mutex);
    auto* record = findReservation(*m_state, identity);
    if (m_state->owner->poisoned || !record || record->preparedWire)
        return ::media::Status::failure(invalid("MPEG-TS prepared wire work lost exclusive reservation ownership"));
    const auto& media = work.media;
    const auto expectedBegin = record->boundWire
        ? record->boundWire->range.protocolEnd : record->work.protocolBegin;
    const auto end = record->work.protocolBegin + record->work.datagramCount;
    if (work.protocolBegin != expectedBegin || work.protocolBegin > end ||
        !media.count || media.count > end - work.protocolBegin ||
        !media.endpointId || !media.regularPayloadBytes || !media.lastPayloadBytes ||
        media.lastPayloadBytes > media.regularPayloadBytes ||
        media.release != record->work.release || media.deadline < media.release ||
        media.deadline > record->work.completionLimit ||
        (work.leadingReport && (!work.leadingReport->endpointId || !work.leadingReport->payloadBytes ||
            work.leadingReport->release.nanoseconds() < 0 ||
            work.leadingReport->deadline < work.leadingReport->release)))
        return ::media::Status::failure(invalid("MPEG-TS prepared wire geometry differs from its reservation"));
    record->preparedWire = work;
    return ::media::Status::success();
}

::media::Status MediaTsPacketCursor::bindPreparedWire(
    const MediaTsPacketReservationIdentity& identity, const MediaTsBoundWireRange& range)
{
    if (!m_state || !m_state->owner)
        return ::media::Status::failure(invalid("MPEG-TS wire binding has no owner"));
    const std::lock_guard lock(m_state->owner->mutex);
    auto* record = findReservation(*m_state, identity);
    if (m_state->owner->poisoned || !record || !record->preparedWire)
        return ::media::Status::failure(invalid("MPEG-TS wire binding has no prepared work"));
    const auto& work = *record->preparedWire;
    if (range.generation != record->work.generation || !range.identity ||
        range.protocolBegin != work.protocolBegin || range.protocolEnd < range.protocolBegin ||
        range.protocolEnd - range.protocolBegin != work.media.count ||
        range.firstMediaSequence < range.firstSequence ||
        range.firstMediaSequence - range.firstSequence != (work.leadingReport ? 1u : 0u) ||
        range.lastSequence < range.firstMediaSequence ||
        range.lastSequence - range.firstMediaSequence != work.media.count - 1)
        return ::media::Status::failure(invalid("MPEG-TS wire binding differs from its prepared geometry"));
    record->boundWire = MediaTsBoundWireWork{work, range};
    record->preparedWire.reset();
    return ::media::Status::success();
}

::media::Status MediaTsPacketCursor::deferFirstReservation()
{
    if (!m_state || !m_state->owner)
        return ::media::Status::failure(invalid("MPEG-TS cursor has no construction owner"));
    auto& owner = *m_state->owner;
    const std::lock_guard lock(owner.mutex);
    auto* active = activeCursor(owner, m_state->pidKind);
    if (owner.poisoned || !active || *active != m_state->identity ||
        !m_state->advancesPayloadContinuity || m_state->continuityReserved ||
        m_state->pending || m_state->committedOffset != 0 ||
        owner.latestPayloadCursors[continuityIndex(m_state->pidKind)] != m_state->identity)
        return ::media::Status::failure(invalid(
            "MPEG-TS cursor cannot defer its first reservation"));
    active->reset();
    return ::media::Status::success();
}

MediaTsSeriesStoragePreparation& MediaTsPacketCursor::seriesStorage() noexcept
{ return *m_state->preparedStorage; }
::media::Result<std::span<const std::array<std::uint8_t, 188>>>
MediaTsPacketCursor::remainingPackets() const
{
    using Result = ::media::Result<std::span<const std::array<std::uint8_t, 188>>>;
    if (!m_state || !m_state->owner)
        return Result::failure(invalid("MPEG-TS packet backing has no cursor owner"));
    const std::lock_guard lock(m_state->owner->mutex);
    if (m_state->owner->poisoned || !m_state->packets || m_state->pending ||
        cursorFinishedLocked(*m_state))
        return Result::failure(invalid("MPEG-TS packet backing is not available for preparation"));
    return Result::success(m_state->packets->view().subspan(m_state->committedOffset));
}
const MediaTsSeriesStoragePlan& MediaTsPacketCursor::storagePlan() const noexcept
{ return m_state->owner->storagePlan; }
const std::shared_ptr<MediaNetworkMemoryResource>& MediaTsPacketCursor::storageResource() const noexcept
{ return m_state->owner->resource; }

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
            ledger->front().cursorIdentity == m_state->identity &&
            ledger->front().cursorRevision == m_state->pending->revision &&
            token.m_publicationIdentity == ledger->front().publicationIdentity
        : active && *active == m_state->identity && !token.m_publicationIdentity;
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

::media::Result<MediaTsMaintenanceStorage> MediaTsTransportPacketizer::prepareMaintenance(
    std::span<const std::size_t> packetCounts)
{
    using Result = ::media::Result<MediaTsMaintenanceStorage>;
    if (!m_state || packetCounts.empty() || packetCounts.size() > maintenancePreparationSlots())
        return Result::failure(invalid("TS maintenance preparation lacks bounded series geometry"));
    const std::lock_guard lock(m_state->mutex);
    std::array<MediaTsSeriesStoragePreparation::Geometry, maintenancePreparationSlots()> shapes{};
    std::array<std::size_t, maintenancePreparationSlots()> costs{};
    std::size_t total = 0;
    for (std::size_t index = 0; index < packetCounts.size(); ++index) {
        const auto packets = packetCounts[index];
        if (!packets || packets > m_state->storagePlan.maximumPackets)
            return Result::failure(invalid("TS maintenance exceeds its packet storage contract"));
        auto shape = MediaTsSeriesStoragePreparation::geometry(m_state->storagePlan.packetizerGeometry,
            m_state->storagePlan.protocolGeometry, packets,
            (std::min)(packets, m_state->storagePlan.maximumDatagrams));
        if (!shape) return Result::failure(shape.error());
        auto cost = MediaTsSeriesStoragePreparation::storageFootprint(shape.value());
        if (!cost) return Result::failure(cost.error());
        if (cost.value() > (std::numeric_limits<std::size_t>::max)() - total)
            return Result::failure(invalid("TS maintenance storage total overflows"));
        shapes[index] = shape.value(); costs[index] = cost.value(); total += cost.value();
    }
    auto admitted = m_state->resource->reserveStorage(total, MediaStorageAdmission::Backpressure);
    if (!admitted) return Result::failure(admitted.error());
    auto lease = std::move(admitted).value();
    MediaTsMaintenanceStorage prepared;
    for (std::size_t index = 0; index < packetCounts.size(); ++index) {
        auto portion = lease.split(costs[index]);
        if (!portion) return Result::failure(portion.error());
        auto storage = MediaTsSeriesStoragePreparation::fromReservation(m_state->resource,
            shapes[index], std::move(portion).value(), false);
        if (!storage) return Result::failure(storage.error());
        prepared[index].emplace(std::move(storage).value());
    }
    return Result::success(std::move(prepared));
}

::media::Result<MediaTsPacketizerSeriesStorageGeometry>
MediaTsTransportPacketizer::seriesStorageGeometry()
{
    using Result = ::media::Result<MediaTsPacketizerSeriesStorageGeometry>;
    auto packets = probeMediaSharedAllocationGeometry<
        MediaFixedCapacityStorage<std::array<std::uint8_t, PacketSize>>>();
    if (!packets) return Result::failure(packets.error());
    return Result::success({packets.value(),
        {1, sizeof(MediaTsPacketCursorState), alignof(MediaTsPacketCursorState)}});
}

::media::Result<MediaSharedAllocationGeometry> MediaTsTransportPacketizer::controlStorageGeometry()
{ return probeMediaSharedAllocationGeometry<MediaTsPacketizerControlState, MediaTsMuxPlan, ProgramContinuityState>(); }
MediaSharedAllocationGeometry MediaTsTransportPacketizer::continuityReservationGeometry() noexcept
{
    using Reservation = MediaTsPacketizerControlState::ContinuityReservation;
    return {1, sizeof(Reservation), alignof(Reservation)};
}
std::size_t MediaTsTransportPacketizer::continuityLedgerCount() noexcept
{ return std::tuple_size_v<decltype(MediaTsPacketizerControlState::reservations)>; }

::media::Result<MediaTsTransportPacketizer> MediaTsTransportPacketizer::create(
    const MediaTsMuxPlan& plan,
    bool startsWithDiscontinuity,
    const MediaTsSeriesStoragePlan& storagePlan,
    std::shared_ptr<MediaNetworkMemoryResource> resource)
{
    using Result = ::media::Result<MediaTsTransportPacketizer>;
    auto valid = MediaTsSeriesStoragePlanner::validate(storagePlan);
    if (!valid || !resource) return Result::failure(!valid ? valid.error() :
        invalid("TS packetizer requires its generation storage resource"));
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
    using Reservation = MediaTsPacketizerControlState::ContinuityReservation;
    const auto& controlShape = storagePlan.packetizerControlGeometry;
    auto controlCost = MediaNetworkMemoryResource::allocationFootprint(controlShape.bytes, controlShape.alignment);
    auto ledgerCost = MediaNetworkMemoryResource::allocationFootprint(
        storagePlan.maximumLiveSeries * sizeof(Reservation), alignof(Reservation));
    const auto observationShape = storagePlan.acceptedObservationGeometry;
    auto observationCost = MediaNetworkMemoryResource::allocationFootprint(
        storagePlan.maximumLiveSeries * observationShape.bytes, observationShape.alignment);
    if (!controlCost || !ledgerCost || !observationCost) return Result::failure(
        !controlCost ? controlCost.error() : !ledgerCost ? ledgerCost.error() : observationCost.error());
    // The planner validated overflow and the complete persistent footprint.
    auto admitted = resource->reserveStorage(controlCost.value() + continuityLedgerCount() * ledgerCost.value() + observationCost.value(),
        MediaStorageAdmission::RequiredAllocation);
    if (!admitted) return Result::failure(admitted.error());
    auto lease = std::move(admitted).value();
    auto portion = lease.split(controlCost.value());
    if (!portion) return Result::failure(portion.error());
    auto allocatedControl = resource->allocateFromReservation(controlShape.bytes, controlShape.alignment,
        std::move(portion).value());
    if (!allocatedControl) return Result::failure(allocatedControl.error());
    auto control = std::move(allocatedControl).value();
    decltype(MediaTsPacketizerControlState::reservations) ledgers;
    for (auto& ledger : ledgers) {
        auto credit = lease.split(ledgerCost.value());
        if (!credit) return Result::failure(credit.error());
        auto allocation = resource->allocateFromReservation(storagePlan.maximumLiveSeries * sizeof(Reservation),
            alignof(Reservation), std::move(credit).value());
        if (!allocation) return Result::failure(allocation.error());
        auto fixed = MediaFixedCapacityStorage<Reservation>::fromAllocation(std::move(allocation).value());
        if (!fixed) return Result::failure(fixed.error());
        ledger = std::move(fixed).value();
    }
    auto scratchCredit = lease.split(observationCost.value());
    if (!scratchCredit) return Result::failure(scratchCredit.error());
    auto scratchAllocation = resource->allocateFromReservation(storagePlan.maximumLiveSeries * observationShape.bytes,
        observationShape.alignment, std::move(scratchCredit).value());
    if (!scratchAllocation) return Result::failure(scratchAllocation.error());
    auto scratch = MediaFixedCapacityStorage<MediaTsAcceptedReservationObservation>::fromAllocation(
        std::move(scratchAllocation).value());
    if (!scratch) return Result::failure(scratch.error());
    auto state = std::allocate_shared<MediaTsPacketizerControlState>(
        MediaSharedAllocationAllocator<MediaTsPacketizerControlState>{MediaSharedAllocationAdmitted{
            resource, controlShape, &control}}, plan, std::move(continuityState));
    state->storagePlan = storagePlan;
    state->resource = std::move(resource);
    state->reservations = std::move(ledgers);
    state->observationScratch = std::move(scratch).value();
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
    return ::media::Result<std::size_t>::success(MediaTsTransportPacketBuilder::PcrOnlyPacketCount);
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPat(
    const MediaTsPatSection& section, MediaTsSeriesStoragePreparation storage)
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
                        segments, false, std::move(storage));
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPmt(
    const MediaTsPmtSection& section, MediaTsSeriesStoragePreparation storage)
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
                        segments, false, std::move(storage));
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPcrOnly(
    const MediaTsPcrClock& pcr, MediaTsSeriesStoragePreparation storage)
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
    auto preparedCursor = MediaTsPacketCursorFactory::prepare(m_state,
        MediaTsTransportPacketBuilder::PcrOnlyPacketCount, std::move(storage));
    if (!preparedCursor) return ::media::Result<MediaTsPacketCursor>::failure(preparedCursor.error());
    auto cursor = std::move(preparedCursor).value();
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
    (*cursor->packets)[0] = std::move(packet).value();
    cursor->initialPayloadContinuity = next;
    cursor->advancesPayloadContinuity = false;
    ++m_state->nextCursorIdentity;
    ++m_state->liveSeries;
    cursor->counted = true;
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
    return beginPayloadLocked(m_state, kind, pid, segments, randomAccess, std::nullopt);
}

} // namespace media::ffmpeg::graph
