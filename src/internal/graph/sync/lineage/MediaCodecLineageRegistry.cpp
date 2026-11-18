#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <optional>
#include <utility>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace {

struct SerializedMediaFfmpegLineageToken final {
    std::uint64_t magic = 0;
    std::uint64_t identifier = 0;
    std::uint64_t generation = 0;
};

constexpr std::uint64_t CodecOpaqueMagic = 0x4d54434f50415131ULL;
constexpr std::size_t tokenBytes = sizeof(SerializedMediaFfmpegLineageToken);

struct MediaFfmpegCodecOpaqueOwner final {
    MediaLineageAllocationScope allocation;
    MediaFfmpegLineageToken lineage;
    SerializedMediaFfmpegLineageToken serialized;
};

static_assert(std::is_nothrow_move_constructible_v<MediaFfmpegCodecOpaqueOwner>);

void releaseOpaqueLease(void* opaque, std::uint8_t*) noexcept
{
    auto* owner = static_cast<MediaFfmpegCodecOpaqueOwner*>(opaque);
    // Keep the allocator owner alive after destruction of its last token.
    MediaLineageAllocator<MediaFfmpegCodecOpaqueOwner> allocator(owner->allocation);
    // Keep the slot occupied until the last engine opaque allocation is gone.
    auto slot = std::move(owner->lineage);
    std::destroy_at(owner);
    allocator.deallocate(owner, 1);
}

} // namespace

struct MediaCodecLineageRegistryState final {
    struct Entry final {
        MediaCodecLineage lineage;
        MediaCodecSendObservation sendObservation;
    };

    using EntryValue = std::pair<const std::uint64_t, Entry>;
    explicit MediaCodecLineageRegistryState(std::size_t requestedCapacity, MediaLineageAllocationScope scope)
        : allocation(std::move(scope)), entries(std::less<std::uint64_t>{},
              MediaLineageAllocator<EntryValue>{allocation}), capacity(requestedCapacity) {}

    void release(std::uint64_t identifier) noexcept
    {
        std::lock_guard lock(mutex);
        entries.erase(identifier);
    }

    MediaLineageAllocationScope allocation;
    std::mutex mutex;
    std::map<std::uint64_t, Entry, std::less<std::uint64_t>,
        MediaLineageAllocator<EntryValue>> entries;
    std::size_t capacity;
    std::uint64_t nextIdentifier = 1;
    std::uint64_t currentGeneration = 0;
    std::uint64_t lastTransitionSequence = 0;
};

namespace {
struct RegistryStorage final {
    std::size_t capacity;
    std::uint64_t stateBytes;
    std::uint64_t treeBytes;
    std::uint64_t opaqueBytes;
    std::uint64_t totalBytes;
};
template<class Charge>
::media::Result<RegistryStorage> registryStorage(std::size_t capacity)
{
    using R = ::media::Result<RegistryStorage>;
    using State = MediaCodecLineageRegistryState;
    if (!capacity) return R::failure(::media::ErrorInfo::invalidArgument("Registry storage requires positive capacity"));
    auto state = probeMediaSharedAllocationGeometry<State>(std::type_identity<MediaLineageAllocator<State>>{});
    auto tree = MediaContainerGeometry<Charge>::template tree<State::EntryValue>(capacity);
    if (!state) return R::failure(state.error());
    if (!tree) return R::failure(tree.error());
    auto stateBytes = Charge::allocationFootprint(state.value().bytes, state.value().alignment);
    auto opaqueUnit = Charge::allocationFootprint(sizeof(MediaFfmpegCodecOpaqueOwner), alignof(MediaFfmpegCodecOpaqueOwner));
    if (!stateBytes) return R::failure(stateBytes.error());
    if (!opaqueUnit) return R::failure(opaqueUnit.error());
    // A slot remains occupied through deallocation of its engine opaque owner.
    // Its inlined token does not allocate a separate lease/control block.
    auto opaque = MediaCheckedArithmetic::multiply(capacity, opaqueUnit.value(), "registry opaque storage");
    if (!opaque) return R::failure(opaque.error());
    RegistryStorage result{capacity, stateBytes.value(),
        tree.value(), opaque.value(), 0};
    for (const auto bytes : {result.stateBytes, result.treeBytes, result.opaqueBytes}) {
        auto total = MediaCheckedArithmetic::add(result.totalBytes, bytes, "registry runtime storage");
        if (!total) return R::failure(total.error());
        result.totalBytes = total.value();
    }
    return R::success(result);
}
} // namespace

::media::Result<MediaCodecLineageRuntimeStorage> MediaCodecLineageRegistry::runtimeStorage(std::size_t capacity)
{
    using R = ::media::Result<MediaCodecLineageRuntimeStorage>;
    auto internal = registryStorage<MediaStandardAllocationRequest>(capacity);
    auto registry = probeMediaMakeSharedAllocationGeometry<MediaCodecLineageRegistry>();
    if (!internal) return R::failure(internal.error());
    if (!registry) return R::failure(registry.error());
    auto total = MediaCheckedArithmetic::add(internal.value().totalBytes, registry.value().bytes, "registry runtime storage");
    if (!total) return R::failure(total.error());
    return R::success({capacity, registry.value().bytes, internal.value().stateBytes,
        internal.value().treeBytes, internal.value().opaqueBytes, total.value()});
}

::media::Result<MediaCodecLineagePreparedStorage> MediaCodecLineageRegistry::preparedStorage(std::size_t capacity)
{
    using R = ::media::Result<MediaCodecLineagePreparedStorage>;
    auto result = registryStorage<MediaPreparationMemoryResource>(capacity);
    if (!result) return R::failure(result.error());
    return R::success({capacity, result.value().stateBytes, result.value().treeBytes,
        result.value().opaqueBytes, result.value().totalBytes});
}

MediaFfmpegLineageToken::MediaFfmpegLineageToken(
    std::uint64_t tokenIdentifier,
    std::uint64_t tokenGeneration,
    std::weak_ptr<MediaCodecLineageRegistryState> state,
    MediaLineageAllocationScope allocation, bool ownsSlot) noexcept
    : identifier(tokenIdentifier), generation(tokenGeneration),
      m_state(std::move(state)), m_allocation(std::move(allocation)),
      m_slotIdentifier(ownsSlot ? tokenIdentifier : 0)
{
}

MediaFfmpegLineageToken::MediaFfmpegLineageToken(MediaFfmpegLineageToken&& other) noexcept
    : identifier(std::exchange(other.identifier, 0)), generation(std::exchange(other.generation, 0)),
      m_state(std::move(other.m_state)), m_allocation(std::move(other.m_allocation)),
      m_slotIdentifier(std::exchange(other.m_slotIdentifier, 0)) {}

MediaFfmpegLineageToken& MediaFfmpegLineageToken::operator=(MediaFfmpegLineageToken&& other) noexcept
{
    if (this == &other) return *this;
    releaseSlot();
    identifier = std::exchange(other.identifier, 0);
    generation = std::exchange(other.generation, 0);
    m_state = std::move(other.m_state);
    m_allocation = std::move(other.m_allocation);
    m_slotIdentifier = std::exchange(other.m_slotIdentifier, 0);
    return *this;
}

MediaFfmpegLineageToken::~MediaFfmpegLineageToken() { releaseSlot(); }

void MediaFfmpegLineageToken::releaseSlot() noexcept
{
    const auto slot = std::exchange(m_slotIdentifier, 0);
    if (slot) if (auto state = m_state.lock()) state->release(slot);
}

namespace {

::media::Result<AVBufferRef*> makeCodecOpaque(
    MediaFfmpegLineageToken token, MediaLineageAllocationScope allocation)
{
    if (!token.identifier || !token.generation) {
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::invalidArgument("FFmpeg codec opaque requires lineage ownership"));
    }
    MediaLineageAllocator<MediaFfmpegCodecOpaqueOwner> allocator(allocation);
    MediaFfmpegCodecOpaqueOwner* owner = nullptr;
    try {
        owner = allocator.allocate(1);
    } catch (const std::bad_alloc&) {
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::allocationFailed("Failed to allocate FFmpeg lineage opaque owner"));
    }
    const SerializedMediaFfmpegLineageToken serialized{
        CodecOpaqueMagic, token.identifier, token.generation};
    std::construct_at(owner, MediaFfmpegCodecOpaqueOwner{
        std::move(allocation), std::move(token), serialized});
    // This allocation covers only the engine-owned token and callback owner.
    // FFmpeg's AVBuffer and cloned AVBufferRef headers have a separate contract.
    AVBufferRef* opaque = av_buffer_create(
        reinterpret_cast<std::uint8_t*>(&owner->serialized), tokenBytes,
        &releaseOpaqueLease, owner, 0);
    if (!opaque) {
        releaseOpaqueLease(owner, nullptr);
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::allocationFailed("Failed to create FFmpeg lineage opaque lease"));
    }
    return ::media::Result<AVBufferRef*>::success(opaque);
}

} // namespace

::media::Result<AVBufferRef*> makeMediaFfmpegCodecOpaque(
    MediaFfmpegLineageToken token)
{
    if (!token.m_slotIdentifier || token.identifier != token.m_slotIdentifier) {
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg codec opaque lineage has no owned lease"));
    }
    auto allocation = token.m_allocation;
    return makeCodecOpaque(std::move(token), std::move(allocation));
}

::media::Result<MediaFfmpegLineageToken> mediaFfmpegLineageToken(
    const AVBufferRef* opaque)
{
    if (!opaque || opaque->size != tokenBytes || !opaque->data) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg output has missing or invalid lineage opaque token"));
    }
    SerializedMediaFfmpegLineageToken serialized;
    std::memcpy(&serialized, opaque->data, tokenBytes);
    if (serialized.magic != CodecOpaqueMagic ||
        serialized.identifier == 0 || serialized.generation == 0) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg output lineage token is empty"));
    }
    return ::media::Result<MediaFfmpegLineageToken>::success(
        MediaFfmpegLineageToken(
            serialized.identifier, serialized.generation, {}, MediaRuntimeLineageAllocation{}, false));
}

MediaCodecLineageRegistry::MediaCodecLineageRegistry(
    std::shared_ptr<State> state) : m_state(std::move(state)) {}
MediaCodecLineageRegistry::MediaCodecLineageRegistry(
    MediaCodecLineageRegistry&&) noexcept = default;
MediaCodecLineageRegistry& MediaCodecLineageRegistry::operator=(
    MediaCodecLineageRegistry&&) noexcept = default;
MediaCodecLineageRegistry::~MediaCodecLineageRegistry() = default;

::media::Result<MediaCodecLineageRegistry>
MediaCodecLineageRegistry::create(std::size_t capacity, MediaLineageAllocationScope allocation)
{
    const auto* prepared = std::get_if<MediaPreparedLineageAllocation>(&allocation);
    if (capacity == 0 || (prepared && !prepared->resource)) {
        return ::media::Result<MediaCodecLineageRegistry>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage registry requires positive capacity and an explicit allocation owner"));
    }
    try {
        auto state = std::allocate_shared<State>(MediaLineageAllocator<State>{allocation},
            capacity, allocation);
        return ::media::Result<MediaCodecLineageRegistry>::success(
            MediaCodecLineageRegistry(std::move(state)));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaCodecLineageRegistry>::failure(
            ::media::ErrorInfo::allocationFailed("Failed to allocate codec lineage registry"));
    }
}

::media::Result<MediaFfmpegLineageToken>
MediaCodecLineageRegistry::submit(MediaCodecLineage lineage)
{
    if (auto status = validateMediaCodecLineage(lineage); !status) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(status.error());
    }
    const auto generation = mediaCodecLineageGeneration(lineage);

    std::lock_guard lock(m_state->mutex);
    if (m_state->entries.size() >= m_state->capacity ||
        m_state->nextIdentifier == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::wouldBlock(
                "Codec lineage registry capacity exhausted"));
    }
    const std::uint64_t identifier = m_state->nextIdentifier;
    try {
        m_state->entries.emplace(identifier, State::Entry{std::move(lineage)});
        ++m_state->nextIdentifier;
        return ::media::Result<MediaFfmpegLineageToken>::success(
            MediaFfmpegLineageToken(
                identifier, generation, m_state, m_state->allocation, true));
    } catch (const std::bad_alloc&) {
        m_state->entries.erase(identifier);
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Failed to allocate FFmpeg lineage lease"));
    }
}

void MediaCodecLineageRegistry::observeAcceptedSend(const AVBufferRef* opaque,
    std::int64_t pts, int numerator, int denominator) noexcept
{
    try {
        auto token = mediaCodecTokenIdentity(opaque);
        if (!token) return;
        std::lock_guard lock(m_state->mutex);
        const auto entry = m_state->entries.find(token->identifier);
        if (entry == m_state->entries.end() ||
            mediaCodecLineageGeneration(entry->second.lineage) != token->generation) return;
        auto& send = entry->second.sendObservation;
        if (!send.acceptedCount) {
            send.inputPts = pts;
            send.timeBaseNumerator = numerator;
            send.timeBaseDenominator = denominator;
        } else {
            // Multiple accepted sends for one token cannot identify which input
            // contributed to a later packet. Preserve the first fact and mark it.
            send.incomplete = true;
        }
        if (send.acceptedCount == (std::numeric_limits<std::uint64_t>::max)()) send.incomplete = true;
        else ++send.acceptedCount;
    } catch (...) {
        // An optional observation must not replace a successful codec send.
    }
}

::media::Result<MediaCodecLineage>
MediaCodecLineageRegistry::resolveEntry(const MediaFfmpegLineageToken& token,
                                   std::uint64_t generation)
{
    std::lock_guard lock(m_state->mutex);
    const auto entry = m_state->entries.find(token.identifier);
    if (generation == 0 || token.generation != generation ||
        entry == m_state->entries.end() ||
        (m_state->currentGeneration != 0 && generation != m_state->currentGeneration) ||
        mediaCodecLineageGeneration(entry->second.lineage) != generation) {
        return ::media::Result<MediaCodecLineage>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage token is missing or belongs to another generation"));
    }
    return ::media::Result<MediaCodecLineage>::success(
        entry->second.lineage);
}

::media::Result<std::optional<MediaCodecLineage>>
MediaCodecLineageRegistry::resolveOutputEntry(const AVBufferRef* opaque, MediaCodecTokenObservation* observation)
{
    auto token = mediaFfmpegLineageToken(opaque);
    if (!token) {
        return ::media::Result<std::optional<MediaCodecLineage>>::failure(
            token.error());
    }
    std::lock_guard lock(m_state->mutex);
    const auto entry = m_state->entries.find(token.value().identifier);
    if (entry == m_state->entries.end() ||
        mediaCodecLineageGeneration(entry->second.lineage) != token.value().generation) {
        return ::media::Result<std::optional<MediaCodecLineage>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec output lineage token is missing or invalid"));
    }
    if (m_state->currentGeneration != 0 &&
        mediaCodecLineageGeneration(entry->second.lineage) != m_state->currentGeneration) {
        return ::media::Result<std::optional<MediaCodecLineage>>::success(
            std::nullopt);
    }
    if (observation) {
        auto& send = entry->second.sendObservation;
        if (send.outputCount == (std::numeric_limits<std::uint64_t>::max)()) send.incomplete = true;
        else ++send.outputCount;
        observation->selectedToken = MediaCodecTokenIdentity{token.value().identifier, token.value().generation};
        observation->submission = send;
        if (!send.acceptedCount || !send.inputPts) observation->submission.incomplete = true;
    }
    return ::media::Result<std::optional<MediaCodecLineage>>::success(
        entry->second.lineage);
}

bool MediaCodecLineageRegistry::retainedOutputIsCurrent(
    const MediaCanonicalLineage& lineage) const noexcept
{
    std::lock_guard lock(m_state->mutex);
    return m_state->currentGeneration == 0 ||
           lineage.generation == m_state->currentGeneration;
}

::media::Status MediaCodecLineageRegistry::finishGeneration(
    std::uint64_t generation)
{
    if (generation == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Codec lineage generation must be positive"));
    }
    std::lock_guard lock(m_state->mutex);
    for (const auto& [identifier, entry] : m_state->entries) {
        (void)identifier;
        if (mediaCodecLineageGeneration(entry.lineage) == generation) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Codec flush left live lineage leases"));
        }
    }
    return ::media::Status::success();
}

::media::Status MediaCodecLineageRegistry::purge(
    const MediaAvGenerationPurge& purge)
{
    if (purge.oldGeneration == 0 || purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Codec lineage purge requires a valid generation transition"));
    }
    std::lock_guard lock(m_state->mutex);
    if (purge.transitionSequence <= m_state->lastTransitionSequence ||
        (m_state->currentGeneration != 0 &&
         purge.oldGeneration != m_state->currentGeneration)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Codec lineage purge does not match the current generation"));
    }
    m_state->currentGeneration = purge.nextGeneration;
    m_state->lastTransitionSequence = purge.transitionSequence;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
