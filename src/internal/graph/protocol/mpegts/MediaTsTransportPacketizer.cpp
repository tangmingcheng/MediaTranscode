#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketBuilder.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t PacketSize = 188;

struct ContinuityState final {
    std::uint8_t nextPayload;
};

enum class CursorPidKind : std::uint8_t { Pat, Pmt, Video, Audio };

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

} // namespace

struct MediaTsPacketizerControlState final {
    MediaTsMuxPlan plan;
    std::array<ContinuityState, 4> continuity;
    std::optional<std::uint64_t> activeCursor;
    std::uint64_t nextCursorIdentity = 1;
    std::vector<std::array<std::uint8_t, PacketSize>> packetWorkspace;
};

struct MediaTsPacketCursorState final {
    std::shared_ptr<MediaTsPacketizerControlState> owner;
    std::uint64_t identity;
    std::uint64_t nextRevision = 1;
    CursorPidKind pidKind;
    std::shared_ptr<std::vector<std::array<std::uint8_t, PacketSize>>> packets;
    std::uint8_t initialPayloadContinuity;
    bool advancesPayloadContinuity;
    std::size_t committedOffset = 0;
    struct Pending final {
        std::uint64_t revision;
        std::size_t endOffset;
        std::uint8_t nextPayload;
    };
    std::optional<Pending> pending;
};

struct MediaTsPacketCursorFactory final {
    static MediaTsPacketCursor create(
        std::unique_ptr<MediaTsPacketCursorState> state) noexcept
    {
        return MediaTsPacketCursor(std::move(state));
    }
};

namespace {

ContinuityState& continuity(MediaTsPacketizerControlState& state,
                            CursorPidKind kind) noexcept
{
    return state.continuity[static_cast<std::size_t>(kind)];
}

::media::Result<MediaTsPacketCursor> beginPayload(
    const std::shared_ptr<MediaTsPacketizerControlState>& state,
    CursorPidKind kind,
    std::uint16_t pid,
    std::span<const std::span<const std::uint8_t>> segments,
    bool randomAccess)
{
    if (!state) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer was moved from"));
    }
    if (state->activeCursor) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer already has an active cursor"));
    }
    if (state->nextCursorIdentity == 0 ||
        state->nextCursorIdentity == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS cursor identity space is exhausted"));
    }
    auto packets = MediaTsTransportPacketBuilder::payload(
        pid, continuity(*state, kind).nextPayload, segments, randomAccess,
        std::move(state->packetWorkspace));
    if (!packets) {
        return ::media::Result<MediaTsPacketCursor>::failure(packets.error());
    }
    const std::uint64_t identity = state->nextCursorIdentity;
    auto cursor = std::make_unique<MediaTsPacketCursorState>();
    cursor->owner = state;
    cursor->identity = identity;
    cursor->pidKind = kind;
    cursor->initialPayloadContinuity = continuity(*state, kind).nextPayload;
    cursor->advancesPayloadContinuity = true;
    cursor->packets = std::make_shared<std::vector<std::array<std::uint8_t, PacketSize>>>(
        std::move(packets).value());
    ++state->nextCursorIdentity;
    state->activeCursor = identity;
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
        if (m_state->packets && m_state->packets.use_count() == 1) {
            m_state->owner->packetWorkspace =
                std::move(*m_state->packets);
        }
        if (m_state->owner->activeCursor == m_state->identity) {
            m_state->owner->activeCursor.reset();
        }
    }
    m_state.reset();
}

::media::Result<MediaTsPreparedPacketBatch> MediaTsPacketCursor::prepare(
    std::size_t maximumPackets)
{
    if (!m_state || !m_state->owner) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet cursor was moved from"));
    }
    if (maximumPackets < 1 ||
        maximumPackets > m_state->owner->plan.parameters().maximumPacketsPerDatagram) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet batch limit is outside the mux plan"));
    }
    if (m_state->pending) {
        return ::media::Result<MediaTsPreparedPacketBatch>::failure(
            invalid("MPEG-TS packet cursor already has a prepared batch"));
    }
    if (finished()) {
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
        revision, end, nextPayload};
    return ::media::Result<MediaTsPreparedPacketBatch>::success(
        MediaTsPreparedPacketBatch(
            m_state->packets, m_state->committedOffset, count,
            MediaTsPacketCommitToken(m_state->owner, m_state->identity, revision)));
}

::media::Status MediaTsPacketCursor::commit(MediaTsPacketCommitToken token)
{
    if (!m_state || !m_state->owner || !m_state->pending || !token.m_valid) {
        return ::media::Status::failure(
            invalid("MPEG-TS packet commit token is not valid for a prepared batch"));
    }
    auto tokenOwner = token.m_owner.lock();
    if (!tokenOwner || tokenOwner.get() != m_state->owner.get() ||
        token.m_cursorIdentity != m_state->identity ||
        token.m_revision != m_state->pending->revision ||
        m_state->owner->activeCursor != m_state->identity) {
        return ::media::Status::failure(
            invalid("MPEG-TS packet commit token identity or revision does not match"));
    }
    continuity(*m_state->owner, m_state->pidKind).nextPayload =
        m_state->pending->nextPayload;
    m_state->committedOffset = m_state->pending->endOffset;
    m_state->pending.reset();
    if (finished()) m_state->owner->activeCursor.reset();
    return ::media::Status::success();
}

bool MediaTsPacketCursor::finished() const noexcept
{
    return m_state && !m_state->pending &&
           m_state->committedOffset == m_state->packets->size();
}

::media::Result<MediaTsTransportPacketizer> MediaTsTransportPacketizer::create(
    const MediaTsMuxPlan& plan)
{
    const auto& seeds = plan.parameters().continuity;
    auto state = std::make_shared<MediaTsPacketizerControlState>(
        MediaTsPacketizerControlState{
            plan, {{{seeds.pat}, {seeds.pmt}, {seeds.video}, {seeds.audio}}}});
    return ::media::Result<MediaTsTransportPacketizer>::success(
        MediaTsTransportPacketizer(std::move(state)));
}

MediaTsTransportPacketizer::MediaTsTransportPacketizer(
    std::shared_ptr<MediaTsPacketizerControlState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPat(
    const MediaTsPatSection& section)
{
    if (!m_state || !section.matches(m_state->plan)) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PAT section does not match the packetizer plan"));
    }
    const std::array<std::uint8_t, 1> pointerField{0};
    const std::array<std::span<const std::uint8_t>, 2> segments{
        pointerField, section.bytes()};
    return beginPayload(m_state, CursorPidKind::Pat,
                        m_state ? m_state->plan.parameters().patPid : 0,
                        segments, false);
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPmt(
    const MediaTsPmtSection& section)
{
    if (!m_state || !section.matches(m_state->plan)) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PMT section does not match the packetizer plan"));
    }
    const std::array<std::uint8_t, 1> pointerField{0};
    const std::array<std::span<const std::uint8_t>, 2> segments{
        pointerField, section.bytes()};
    return beginPayload(m_state, CursorPidKind::Pmt,
                        m_state ? m_state->plan.parameters().programMapPid : 0,
                        segments, false);
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPcrOnly(
    const MediaTsPcrClock& pcr)
{
    if (!m_state) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS packetizer was moved from"));
    }
    if (m_state->activeCursor) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS PCR cursor cannot begin"));
    }
    if (m_state->nextCursorIdentity == 0 ||
        m_state->nextCursorIdentity == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaTsPacketCursor>::failure(
            invalid("MPEG-TS cursor identity space is exhausted"));
    }
    const auto kind = m_state->plan.parameters().pcrPid ==
                              m_state->plan.parameters().videoPid
        ? CursorPidKind::Video
        : CursorPidKind::Audio;
    const std::uint64_t identity = m_state->nextCursorIdentity;
    auto cursor = std::make_unique<MediaTsPacketCursorState>();
    cursor->owner = m_state;
    cursor->identity = identity;
    cursor->pidKind = kind;
    const auto next = continuity(*m_state, kind).nextPayload;
    auto packet = MediaTsTransportPacketBuilder::pcrOnly(
        m_state->plan.parameters().pcrPid, next, pcr.wire27Mhz);
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
    m_state->activeCursor = identity;
    return ::media::Result<MediaTsPacketCursor>::success(
        MediaTsPacketCursor(std::move(cursor)));
}

::media::Result<MediaTsPacketCursor> MediaTsTransportPacketizer::beginPes(
    MediaScheduledStream stream,
    const MediaTsPesHeader& header,
    std::span<const std::uint8_t> payload,
    bool randomAccess)
{
    CursorPidKind kind;
    std::uint16_t pid = 0;
    switch (stream) {
    case MediaScheduledStream::Video:
        kind = CursorPidKind::Video;
        if (m_state) pid = m_state->plan.parameters().videoPid;
        break;
    case MediaScheduledStream::Audio:
        if (randomAccess) {
            return ::media::Result<MediaTsPacketCursor>::failure(
                invalid("MPEG-TS audio PES cannot be random access"));
        }
        kind = CursorPidKind::Audio;
        if (m_state) pid = m_state->plan.parameters().audioPid;
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
    return beginPayload(m_state, kind, pid, segments, randomAccess);
}

} // namespace media::ffmpeg::graph
