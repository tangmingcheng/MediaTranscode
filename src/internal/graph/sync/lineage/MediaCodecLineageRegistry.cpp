#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <utility>

extern "C" {
#include <libavutil/mem.h>
}

namespace media::ffmpeg::graph {
namespace {

struct SerializedMediaFfmpegLineageToken final {
    std::uint64_t identifier = 0;
    std::uint64_t generation = 0;
};

constexpr std::size_t tokenBytes = sizeof(SerializedMediaFfmpegLineageToken);

void releaseOpaqueLease(void* opaque, std::uint8_t* data) noexcept
{
    delete static_cast<MediaFfmpegLineageToken*>(opaque);
    av_free(data);
}

} // namespace

struct MediaCodecLineageRegistry::State final {
    struct Entry final {
        std::shared_ptr<const MediaCanonicalLineage> lineage;
    };

    explicit State(std::size_t requestedCapacity) : capacity(requestedCapacity) {}

    void release(std::uint64_t identifier) noexcept
    {
        std::lock_guard lock(mutex);
        entries.erase(identifier);
    }

    std::mutex mutex;
    std::map<std::uint64_t, Entry> entries;
    std::size_t capacity;
    std::uint64_t nextIdentifier = 1;
    std::uint64_t currentGeneration = 0;
    std::uint64_t lastTransitionSequence = 0;
};

class MediaFfmpegLineageLeaseControl final {
public:
    MediaFfmpegLineageLeaseControl(
        std::weak_ptr<MediaCodecLineageRegistry::State> state,
        std::uint64_t identifier) noexcept
        : m_state(std::move(state)), m_identifier(identifier)
    {
    }

    ~MediaFfmpegLineageLeaseControl()
    {
        if (auto state = m_state.lock()) {
            state->release(m_identifier);
        }
    }

private:
    std::weak_ptr<MediaCodecLineageRegistry::State> m_state;
    std::uint64_t m_identifier;
};

MediaFfmpegLineageToken::MediaFfmpegLineageToken(
    std::uint64_t tokenIdentifier,
    std::uint64_t tokenGeneration,
    std::shared_ptr<MediaFfmpegLineageLeaseControl> lease) noexcept
    : identifier(tokenIdentifier), generation(tokenGeneration),
      m_lease(std::move(lease))
{
}

::media::Result<AVBufferRef*> makeMediaFfmpegLineageOpaque(
    MediaFfmpegLineageToken token)
{
    if (token.identifier == 0 || token.generation == 0 || !token.m_lease) {
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg lineage opaque requires one owned submission token"));
    }

    auto* data = static_cast<std::uint8_t*>(av_malloc(tokenBytes));
    if (!data) {
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Failed to allocate FFmpeg lineage opaque token data"));
    }
    const SerializedMediaFfmpegLineageToken serialized{
        token.identifier, token.generation};
    std::memcpy(data, &serialized, tokenBytes);

    auto* owner = new (std::nothrow) MediaFfmpegLineageToken(std::move(token));
    if (!owner) {
        av_free(data);
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Failed to allocate FFmpeg lineage opaque lease owner"));
    }
    AVBufferRef* opaque = av_buffer_create(
        data, tokenBytes, &releaseOpaqueLease, owner, 0);
    if (!opaque) {
        releaseOpaqueLease(owner, data);
        return ::media::Result<AVBufferRef*>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Failed to create FFmpeg lineage opaque lease"));
    }
    return ::media::Result<AVBufferRef*>::success(opaque);
}

::media::Result<MediaFfmpegLineageToken> mediaFfmpegLineageToken(
    const AVBufferRef* opaque)
{
    if (!opaque || opaque->size != tokenBytes) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg output has missing or invalid lineage opaque token"));
    }
    SerializedMediaFfmpegLineageToken serialized;
    std::memcpy(&serialized, opaque->data, tokenBytes);
    if (serialized.identifier == 0 || serialized.generation == 0) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg output lineage token is empty"));
    }
    return ::media::Result<MediaFfmpegLineageToken>::success(
        MediaFfmpegLineageToken(
            serialized.identifier, serialized.generation, nullptr));
}

MediaCodecLineageRegistry::MediaCodecLineageRegistry(
    std::unique_ptr<State> state) : m_state(std::move(state)) {}
MediaCodecLineageRegistry::MediaCodecLineageRegistry(
    MediaCodecLineageRegistry&&) noexcept = default;
MediaCodecLineageRegistry& MediaCodecLineageRegistry::operator=(
    MediaCodecLineageRegistry&&) noexcept = default;
MediaCodecLineageRegistry::~MediaCodecLineageRegistry() = default;

::media::Result<MediaCodecLineageRegistry>
MediaCodecLineageRegistry::create(std::size_t capacity)
{
    if (capacity == 0) {
        return ::media::Result<MediaCodecLineageRegistry>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage registry capacity must be positive"));
    }
    return ::media::Result<MediaCodecLineageRegistry>::success(
        MediaCodecLineageRegistry(std::make_unique<State>(capacity)));
}

::media::Result<MediaFfmpegLineageToken>
MediaCodecLineageRegistry::submit(
    std::shared_ptr<const MediaCanonicalLineage> lineage)
{
    if (!lineage) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage registry rejects null lineage"));
    }
    if (auto status = validateMediaCanonicalLineage(*lineage); !status) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(status.error());
    }

    std::lock_guard lock(m_state->mutex);
    if (m_state->entries.size() >= m_state->capacity ||
        m_state->nextIdentifier == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::wouldBlock(
                "Codec lineage registry capacity exhausted"));
    }
    const std::uint64_t identifier = m_state->nextIdentifier++;
    m_state->entries.emplace(identifier, State::Entry{lineage});
    try {
        auto lease = std::make_shared<MediaFfmpegLineageLeaseControl>(
            m_state, identifier);
        return ::media::Result<MediaFfmpegLineageToken>::success(
            MediaFfmpegLineageToken(
                identifier, lineage->generation, std::move(lease)));
    } catch (const std::bad_alloc&) {
        m_state->entries.erase(identifier);
        return ::media::Result<MediaFfmpegLineageToken>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Failed to allocate FFmpeg lineage lease"));
    }
}

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
MediaCodecLineageRegistry::resolve(const MediaFfmpegLineageToken& token,
                                   std::uint64_t generation)
{
    std::lock_guard lock(m_state->mutex);
    const auto entry = m_state->entries.find(token.identifier);
    if (generation == 0 || token.generation != generation ||
        entry == m_state->entries.end() ||
        (m_state->currentGeneration != 0 && generation != m_state->currentGeneration) ||
        entry->second.lineage->generation != generation) {
        return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage token is missing or belongs to another generation"));
    }
    return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::success(
        entry->second.lineage);
}

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
MediaCodecLineageRegistry::resolve(const AVBufferRef* opaque,
                                   std::uint64_t generation)
{
    auto token = mediaFfmpegLineageToken(opaque);
    return token ? resolve(token.value(), generation)
                 : ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
                       token.error());
}

::media::Result<std::optional<std::shared_ptr<const MediaCanonicalLineage>>>
MediaCodecLineageRegistry::resolveOutput(const AVBufferRef* opaque)
{
    auto token = mediaFfmpegLineageToken(opaque);
    if (!token) {
        return ::media::Result<std::optional<std::shared_ptr<const MediaCanonicalLineage>>>::failure(
            token.error());
    }
    std::lock_guard lock(m_state->mutex);
    const auto entry = m_state->entries.find(token.value().identifier);
    if (entry == m_state->entries.end() ||
        entry->second.lineage->generation != token.value().generation) {
        return ::media::Result<std::optional<std::shared_ptr<const MediaCanonicalLineage>>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec output lineage token is missing or invalid"));
    }
    if (m_state->currentGeneration != 0 &&
        entry->second.lineage->generation != m_state->currentGeneration) {
        return ::media::Result<std::optional<std::shared_ptr<const MediaCanonicalLineage>>>::success(
            std::nullopt);
    }
    return ::media::Result<std::optional<std::shared_ptr<const MediaCanonicalLineage>>>::success(
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
        if (entry.lineage->generation == generation) {
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
