#pragma once
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/runtime/storage/MediaAvAggregateStorage.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include <span>

namespace media::ffmpeg::graph {
class MediaAvAggregateMetadataBudget final {
public:
    static ::media::Status validate(const MediaAvContinuousAggregateTopology& plan,
        std::span<const MediaAvAggregateSourceStorage> sources,
        const MediaFixedCapacityStorage<std::shared_ptr<MediaBoundCanonicalAudioBuffer>>& audioCandidates,
        const MediaBufferRef& pending)
    {
    std::uint64_t remaining = plan.maximumMetadataBytes;
    const auto consume = [&](std::uint64_t bytes) {
        if (bytes > remaining) return false;
        remaining -= bytes;
        return true;
    };
    const auto lineageBytes = [&](const MediaCanonicalLineage& lineage) {
        if (!consume(sizeof(lineage))) return false;
        const auto& identity = lineage.identity;
        const auto stringBytes = std::holds_alternative<MediaSourceAccessUnitIdentity>(identity)
            ? std::get<MediaSourceAccessUnitIdentity>(identity).sourceIdentity.storageBytes()
            : std::get<MediaOutputAccessUnitIdentity>(identity).outputIdentity.storageBytes();
        if (!consume(stringBytes)) return false;
        if (!consume(lineage.videoContributions.storageBytes())) return false;
        for (const auto& contribution : lineage.videoContributions) {
            if (const auto* source = std::get_if<MediaCanonicalSourceStamp>(&contribution.origin))
                if (!consume(source->identity.sourceIdentity.storageBytes())) return false;
            if (const auto* display = std::get_if<MediaVideoDisplaySourceStamp>(&contribution.origin))
                if (!consume(display->identity.sourceIdentity.storageBytes())) return false;
        }
        return true;
    };
    const auto audioBytes = [&](const MediaBoundCanonicalAudioBuffer& audio) {
        if (!consume(sizeof(audio)) || !consume(sizeof(MediaCanonicalAudioSamplesBuffer))) return false;
        if (!consume(audio.media()->fragments().size_bytes())) return false;
        for (const auto& fragment : audio.media()->fragments()) {
            // Counting a shared lineage for every fragment is conservative and
            // keeps the charge independent of allocator/address identities.
            if (!lineageBytes(*fragment.lineage)) return false;
            if (const auto* real = std::get_if<MediaCanonicalAudioRealSource>(&fragment.contribution.origin))
                if (!consume(real->source.identity.sourceIdentity.storageBytes())) return false;
        }
        return true;
    };
    bool valid = consume(plan.storage.fixedStorageBytes);
    for (const auto& source : sources) {
        for (const auto& video : source.video) {
            if (const auto lineage = FFmpegFrameView::canonicalLineage(video))
                valid = valid && consume(sizeof(MediaCanonicalVideoFrameBuffer)) && lineageBytes(*lineage);
            else {
                const auto* decoded = dynamic_cast<const MediaVideoDecodeBuffer*>(video.get());
                const auto display = decoded ? decoded->displayLineage() : nullptr;
                valid = valid && display && display->accessUnit && consume(sizeof(MediaVideoDecodeBuffer)) &&
                    consume(sizeof(MediaVideoDisplayLineage)) && consume(sizeof(MediaVideoDecodeLineage)) &&
                    consume(display->accessUnit->identity.sourceIdentity.storageBytes());
            }
        }
    }
    for (const auto& audio : audioCandidates)
        valid = valid && audioBytes(*audio);
    if (pending) {
        if (const auto video = FFmpegFrameView::canonicalLineage(pending))
            valid = valid && consume(sizeof(MediaCanonicalVideoFrameBuffer)) && lineageBytes(*video);
        if (const auto* audio = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(pending.get()))
            valid = valid && audioBytes(*audio);
    }
    return valid ? ::media::Status::success() : ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Aggregate retained metadata exceeds its planner bound"));
    }
};
} // namespace media::ffmpeg::graph
