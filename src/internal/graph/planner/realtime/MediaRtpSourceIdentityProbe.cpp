#include "internal/graph/runtime/ffmpeg/MediaDecodedVideoTimeBase.h"
#include "internal/graph/planner/realtime/MediaRtpSourceIdentityProbe.h"
#include <limits>
#include "internal/graph/sync/lineage/MediaDecodedVideoPresentation.h"
extern "C" {
#include <libavutil/mathematics.h>
}

namespace media::ffmpeg::graph {

MediaRtpSourceIdentityProbe::MediaRtpSourceIdentityProbe(MediaPreparationStorageLease headers,
    MediaCodecLineageRegistry registry, MediaCanonicalIdentity source,
    std::uint64_t generation, MediaRtpSourceIdentityProbeStorage storage,
    std::shared_ptr<MediaPreparationStorageBudget> budget,
    std::shared_ptr<MediaPreparationStorageBudget> rootBudget, MediaSourceIdentityProbeRequirement requirement)
    : m_headers(std::move(headers)), m_registry(std::move(registry)),
      m_source(std::move(source)), m_generation(generation), m_storage(std::move(storage)),
      m_budget(std::move(budget)), m_rootBudget(std::move(rootBudget)), m_requirement(requirement) {}

::media::Result<std::shared_ptr<MediaRtpSourceIdentityProbe>> MediaRtpSourceIdentityProbe::create(
    std::size_t count, const MediaSourceIdentityProbeGoal& goal, std::size_t softwareFrameBytes)
try {
    using R = ::media::Result<std::shared_ptr<MediaRtpSourceIdentityProbe>>;
    if (!count || !goal.budget || goal.source.empty() || !goal.generation ||
        (goal.requirement != MediaSourceIdentityProbeRequirement::MappedFrame &&
         goal.requirement != MediaSourceIdentityProbeRequirement::ReorderedFrames))
        return R::failure(::media::ErrorInfo::invalidArgument("Identity probe requires bounded submissions and owned source storage"));
    auto storage = MediaRtpSourceIdentityProbeStoragePlanner::plan(count, softwareFrameBytes);
    if (!storage) return R::failure(storage.error());
    auto reservation = goal.budget->reserve(storage.value().capacityBytes, MediaStorageAdmission::RequiredAllocation);
    if (!reservation) return R::failure(reservation.error());
    auto budget = MediaPreparationStorageBudget::fromReservation(std::move(reservation).value(), MediaStorageReleaseUnobserved{});
    if (!budget) return R::failure(budget.error());
    auto headers = budget.value()->reserve(storage.value().publicHeaderBytes, MediaStorageAdmission::RequiredAllocation);
    if (!headers) return R::failure(headers.error());
    auto resource = MediaPreparationMemoryResource::create(budget.value());
    if (!resource) return R::failure(resource.error());
    const MediaPreparedLineageAllocation allocation{resource.value()};
    auto registry = MediaCodecLineageRegistry::create(storage.value().registry.capacity, allocation);
    if (!registry) return R::failure(registry.error());
    return R::success(std::allocate_shared<MediaRtpSourceIdentityProbe>(
        MediaLineageAllocator<MediaRtpSourceIdentityProbe>{allocation},
        std::move(headers).value(), std::move(registry).value(), goal.source, goal.generation,
        std::move(storage).value(), budget.value(), goal.budget, goal.requirement));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<MediaRtpSourceIdentityProbe>>::failure(
        ::media::ErrorInfo::allocationFailed("Identity probe storage allocation failed"));
}

::media::Result<::media::ffmpeg::PacketPtr> MediaRtpSourceIdentityProbe::prepare(
    const AVPacket& packet, std::shared_ptr<const MediaVideoDecodeLineage> lineage)
{
    using R = ::media::Result<::media::ffmpeg::PacketPtr>;
    if (!lineage || lineage->identity.sourceIdentity != m_source || lineage->generation != m_generation ||
        lineage->decode || lineage->duration || packet.dts != AV_NOPTS_VALUE || packet.duration != 0 ||
        !packet.buf || packet.side_data_elems || packet.opaque || packet.opaque_ref ||
        packet.pts != lineage->packetPresentation || m_evidence.submitted >= m_storage.registry.capacity ||
        (m_previousInput && lineage->identity.sourceSequence.value() <= m_previousInput->sourceSequence))
        return R::failure(::media::ErrorInfo::invalidArgument("Identity probe AU violates source presentation contract"));
    auto token = m_registry.submit(std::move(lineage));
    if (!token) return R::failure(token.error());
    ::media::ffmpeg::PacketPtr clone(av_packet_clone(&packet));
    if (!clone) return R::failure(::media::ErrorInfo::allocationFailed("Identity probe packet header clone failed"));
    auto opaque = makeMediaFfmpegCodecOpaque(std::move(token).value());
    if (!opaque) return R::failure(opaque.error());
    clone->opaque_ref = opaque.value();
    return R::success(std::move(clone));
}

::media::Status MediaRtpSourceIdentityProbe::submitted(const MediaVideoDecodeLineage& lineage)
{
    const MediaRtpProbeIdentityPoint point{lineage.identity.sourceSequence.value(),
        lineage.packetPresentation, lineage.packetTimeBase};
    if (m_previousInput && av_compare_ts(point.pts, {point.timeBase.num, point.timeBase.den},
            m_previousInput->pts, {m_previousInput->timeBase.num, m_previousInput->timeBase.den}) < 0 &&
        !m_evidence.inputPtsReversal)
        m_evidence.inputPtsReversal = MediaRtpProbeIdentityReversal{*m_previousInput, point};
    m_previousInput = point;
    ++m_evidence.submitted;
    return ::media::Status::success();
}

::media::Status MediaRtpSourceIdentityProbe::received(AVFrame& frame, const AVCodecContext& decoder)
{
    auto lineage = m_registry.resolve<MediaVideoDecodeLineage>(frame.opaque_ref, m_generation);
    if (!lineage) return ::media::Status::failure(lineage.error());
    auto timeBase = resolveMediaDecodedVideoTimeBase(&decoder, frame, true);
    if (!timeBase) return ::media::Status::failure(timeBase.error());
    if (lineage.value()->identity.sourceIdentity != m_source || frame.pts == AV_NOPTS_VALUE ||
        m_evidence.decoded == (std::numeric_limits<std::uint64_t>::max)())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Identity probe decoded frame has invalid source or timestamp"));
    auto presentation = projectMediaDecodedVideoPresentation(*lineage.value(), frame.pts,
        {timeBase.value().num, timeBase.value().den});
    if (!presentation) return ::media::Status::failure(presentation.error());
    frame.time_base = timeBase.value();
    const MediaRtpProbeIdentityPoint point{lineage.value()->identity.sourceSequence.value(),
        frame.pts, {timeBase.value().num, timeBase.value().den}};
    if (m_evidence.lastOutput && point.sourceSequence < m_evidence.lastOutput->sourceSequence &&
        !m_evidence.outputIdentityReversal)
        m_evidence.outputIdentityReversal = MediaRtpProbeIdentityReversal{*m_evidence.lastOutput, point};
    m_evidence.lastOutput = point;
    m_evidence.lastPresentation = presentation.value();
    ++m_evidence.decoded;
    return ::media::Status::success();
}

bool MediaRtpSourceIdentityProbe::complete() const noexcept
{ return m_evidence.decoded && (m_requirement == MediaSourceIdentityProbeRequirement::MappedFrame ||
    (m_evidence.inputPtsReversal && m_evidence.outputIdentityReversal)); }

} // namespace media::ffmpeg::graph
