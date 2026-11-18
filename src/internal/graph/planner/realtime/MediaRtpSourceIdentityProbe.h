#pragma once

#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"
#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"
#include "internal/graph/planner/realtime/MediaRtpSourceIdentityProbeStoragePlanner.h"

namespace media::ffmpeg::graph {

struct MediaFirstFrameProbeGoal final {};
enum class MediaSourceIdentityProbeRequirement { MappedFrame, ReorderedFrames };
struct MediaSourceIdentityProbeGoal final {
    MediaSourceIdentityProbeRequirement requirement;
    MediaCanonicalIdentity source;
    std::uint64_t generation;
    std::shared_ptr<MediaPreparationStorageBudget> budget;
};
using MediaRtpSourceProbeGoal = std::variant<MediaFirstFrameProbeGoal, MediaSourceIdentityProbeGoal>;

struct MediaRtpProbeIdentityPoint final {
    std::uint64_t sourceSequence;
    std::int64_t pts;
    MediaRational timeBase;
};
struct MediaRtpProbeIdentityReversal final {
    MediaRtpProbeIdentityPoint previous;
    MediaRtpProbeIdentityPoint current;
};
struct MediaRtpProbeIdentityEvidence final {
    std::uint64_t submitted = 0;
    std::uint64_t decoded = 0;
    std::optional<MediaRtpProbeIdentityReversal> inputPtsReversal;
    std::optional<MediaRtpProbeIdentityReversal> outputIdentityReversal;
    std::optional<MediaRtpProbeIdentityPoint> lastOutput;
    std::optional<MediaRunningTime> lastPresentation;
};

// Must outlive the decoder and every probe frame carrying its opaque tokens.
class MediaRtpSourceIdentityProbe final {
public:
    static ::media::Result<std::shared_ptr<MediaRtpSourceIdentityProbe>> create(
        std::size_t maximumSubmissions, const MediaSourceIdentityProbeGoal& goal,
        std::size_t additionalSoftwareFrameBytes);
    MediaRtpSourceIdentityProbe(MediaPreparationStorageLease headers,
        MediaCodecLineageRegistry registry, MediaCanonicalIdentity source,
        std::uint64_t generation, MediaRtpSourceIdentityProbeStorage storage,
        std::shared_ptr<MediaPreparationStorageBudget> budget,
        std::shared_ptr<MediaPreparationStorageBudget> rootBudget,
        MediaSourceIdentityProbeRequirement requirement);
    ::media::Result<::media::ffmpeg::PacketPtr> prepare(
        const AVPacket& packet, std::shared_ptr<const MediaVideoDecodeLineage> lineage);
    ::media::Status submitted(const MediaVideoDecodeLineage& lineage);
    ::media::Status received(AVFrame& frame, const AVCodecContext& decoder);
    bool complete() const noexcept;
    const MediaRtpProbeIdentityEvidence& evidence() const noexcept { return m_evidence; }
    const MediaRtpSourceIdentityProbeStorage& storage() const noexcept { return m_storage; }
    const std::shared_ptr<MediaPreparationStorageBudget>& budget() const noexcept { return m_budget; }
    const std::shared_ptr<MediaPreparationStorageBudget>& rootBudget() const noexcept { return m_rootBudget; }
private:
    MediaPreparationStorageLease m_headers;
    MediaCodecLineageRegistry m_registry;
    MediaCanonicalIdentity m_source;
    std::uint64_t m_generation;
    MediaRtpSourceIdentityProbeStorage m_storage;
    std::shared_ptr<MediaPreparationStorageBudget> m_budget;
    std::shared_ptr<MediaPreparationStorageBudget> m_rootBudget;
    MediaSourceIdentityProbeRequirement m_requirement;
    std::optional<MediaRtpProbeIdentityPoint> m_previousInput;
    MediaRtpProbeIdentityEvidence m_evidence;
};

} // namespace media::ffmpeg::graph
