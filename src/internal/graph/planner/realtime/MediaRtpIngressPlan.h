#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressCapability.h"
#include "internal/graph/planner/realtime/MediaRtpIngressObservation.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaRtpIngressPlanFacts final {
    MediaRtpIngressAdapterKind adapterKind;
    std::size_t socketReceiveCapacityBytes;
    std::size_t maximumDatagramBytes;
    std::size_t batchByteCapacity;
    std::size_t descriptorCapacity;
    std::size_t requiredBufferAlignmentBytes;
    std::size_t reorderWindowPackets;
    std::int64_t maximumReorderDelayNanoseconds;
    MediaRtpIngressStorageOwnership storageOwnership;
    MediaRtpIngressCancellationContract cancellationContract;
    MediaRtpIngressCompletionEvidence completionEvidence;

    bool operator==(const MediaRtpIngressPlanFacts&) const noexcept = default;
};

class MediaRtpIngressPlan final {
public:
    MediaRtpIngressPlan() = delete;

    static ::media::Result<MediaRtpIngressPlan> create(
        const MediaRtpIngressCapability& capability,
        const MediaRtpIngressObservation& observation,
        std::size_t preparedInputByteBudget);
    static ::media::Result<MediaRtpIngressPlan> fromFacts(
        MediaRtpIngressPlanFacts facts);

    MediaRtpIngressAdapterKind adapterKind() const noexcept;
    std::size_t socketReceiveCapacityBytes() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t batchByteCapacity() const noexcept;
    std::size_t descriptorCapacity() const noexcept;
    std::size_t requiredBufferAlignmentBytes() const noexcept;
    std::size_t reorderWindowPackets() const noexcept;
    std::int64_t maximumReorderDelayNanoseconds() const noexcept;
    MediaRtpIngressStorageOwnership storageOwnership() const noexcept;
    MediaRtpIngressCancellationContract cancellationContract() const noexcept;
    MediaRtpIngressCompletionEvidence completionEvidence() const noexcept;
    MediaRtpIngressPlanFacts facts() const noexcept;
    bool sameProduct(const MediaRtpIngressPlan& other) const noexcept;
    ::media::Status validateProduct() const;

private:
    MediaRtpIngressPlan(
        const MediaRtpIngressCapability& capability,
        std::size_t maximumDatagramBytes,
        std::size_t batchByteCapacity,
        std::size_t descriptorCapacity,
        std::size_t reorderWindowPackets,
        std::int64_t maximumReorderDelayNanoseconds) noexcept;
    explicit MediaRtpIngressPlan(MediaRtpIngressPlanFacts facts) noexcept;

    MediaRtpIngressAdapterKind m_adapterKind;
    std::size_t m_socketReceiveCapacityBytes;
    std::size_t m_maximumDatagramBytes;
    std::size_t m_batchByteCapacity;
    std::size_t m_descriptorCapacity;
    std::size_t m_requiredBufferAlignmentBytes;
    std::size_t m_reorderWindowPackets;
    std::int64_t m_maximumReorderDelayNanoseconds;
    MediaRtpIngressStorageOwnership m_storageOwnership;
    MediaRtpIngressCancellationContract m_cancellationContract;
    MediaRtpIngressCompletionEvidence m_completionEvidence;
};

} // namespace media::ffmpeg::graph
