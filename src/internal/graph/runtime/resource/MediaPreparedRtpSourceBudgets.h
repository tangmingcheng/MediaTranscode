#pragma once
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include <array>

namespace media::ffmpeg::graph {
// Roles have stable owners from bootstrap through final packet release.
struct MediaPreparedRtpSourceBudgets final {
    std::shared_ptr<MediaPreparationStorageBudget> root;
    std::shared_ptr<MediaPreparationStorageBudget> metadata;
    std::shared_ptr<MediaPreparationStorageBudget> videoPayload;
    std::shared_ptr<MediaPreparationStorageBudget> audioPayload;

    bool operator==(const MediaPreparedRtpSourceBudgets&) const noexcept = default;

    ::media::Status validate(std::size_t capacity, bool audio) const
    {
        if (!root || !metadata || !videoPayload || static_cast<bool>(audioPayload) != audio)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source budget roles are incomplete"));
        const std::array children{metadata, videoPayload, audioPayload};
        std::array<MediaPreparationStorageSnapshot, 3> snapshots{};
        const std::size_t count = audio ? 3 : 2;
        auto partition = MediaPreparationStorageBudget::snapshotPartition(
            std::span{children}.first(count), std::span{snapshots}.first(count));
        if (!partition) return ::media::Status::failure(partition.error());
        if (partition.value().root != root || partition.value().budget.capacity != capacity)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source budget roles changed their deployment root"));
        for (std::size_t i = 0; i < count; ++i)
            if (snapshots[i].sealed || snapshots[i].capacity != capacity)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source roles require original unsealed capacity"));
        return ::media::Status::success();
    }

    static ::media::Result<MediaPreparedRtpSourceBudgets> create(
        std::size_t capacity, const MediaPreparationStorageScope& scope, bool audio)
    {
        using R = ::media::Result<MediaPreparedRtpSourceBudgets>;
        std::shared_ptr<MediaPreparationStorageBudget> root;
        if (const auto* shared = std::get_if<MediaSharedPreparationStorage>(&scope)) {
            root = shared->root;
            if (!root || root->snapshot().capacity != capacity)
                return R::failure(::media::ErrorInfo::invalidArgument("Source budgets require matching root capacity"));
        } else {
            auto created = MediaPreparationStorageBudget::create(capacity,
                MediaStandalonePreparationStorage{}, MediaStorageReleaseUnobserved{});
            if (!created) return R::failure(created.error());
            root = std::move(created).value();
        }
        auto metadata = MediaPreparationStorageBudget::create(capacity,
            MediaSharedPreparationStorage{root}, MediaStorageReleaseUnobserved{});
        if (!metadata) return R::failure(metadata.error());
        auto video = MediaPreparationStorageBudget::create(capacity,
            MediaSharedPreparationStorage{root}, MediaStorageReleaseUnobserved{});
        if (!video) return R::failure(video.error());
        std::shared_ptr<MediaPreparationStorageBudget> audioOwner;
        if (audio) {
            auto value = MediaPreparationStorageBudget::create(capacity,
                MediaSharedPreparationStorage{root}, MediaStorageReleaseUnobserved{});
            if (!value) return R::failure(value.error());
            audioOwner = std::move(value).value();
        }
        return R::success({std::move(root), std::move(metadata).value(),
            std::move(video).value(), std::move(audioOwner)});
    }
};
} // namespace media::ffmpeg::graph
