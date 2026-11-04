#include "internal/graph/planner/realtime/MediaPreparedPacketWrapperStoragePlanner.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <new>
namespace media::ffmpeg::graph {
::media::Result<MediaPreparedPacketWrapperStoragePlan> MediaPreparedPacketWrapperStoragePlanner::plan(
    const MediaCompositionGraphTopology& topology,
    const MediaCompositionReferenceResidencePlan& reference,
    std::size_t sourceIndex, MediaStreamKind stream,
    const MediaPreparedRtpContainerBounds& containers, std::uint64_t maximumAtomicCompletion)
try {
    using R = ::media::Result<MediaPreparedPacketWrapperStoragePlan>;
    using Kind = MediaPreparedPacketWrapperRootKind;
    const auto invalid = [](const char* message) { return R::failure(::media::ErrorInfo::invalidArgument(message)); };
    if ((stream != MediaStreamKind::Video && stream != MediaStreamKind::Audio) ||
        !containers.prefixObjects || !containers.acquiringObjects || !containers.outputObjects || !maximumAtomicCompletion)
        return invalid("Prepared packet wrappers require complete stream container and completion bounds");
    const bool video = stream == MediaStreamKind::Video;
    const auto rawRole = compositionSourceRole(sourceIndex, {MediaSourceTopologyDomain::Rtp,
        static_cast<std::size_t>(video ? MediaRtpClockRouteRole::VideoInput : MediaRtpClockRouteRole::AudioInput)});
    const auto clockRole = compositionSourceRole(sourceIndex, {MediaSourceTopologyDomain::Rtp,
        static_cast<std::size_t>(video ? MediaRtpClockRouteRole::VideoBinder : MediaRtpClockRouteRole::AudioBinder)});
    std::optional<std::size_t> raw, clock;
    for (const auto& binding : topology.bindings) {
        auto* target = binding.role == rawRole ? &raw : binding.role == clockRole ? &clock : nullptr;
        if (!target) continue;
        if (*target) return invalid("Prepared packet wrapper creator identity is repeated");
        *target = binding.position;
    }
    if (!raw || !clock || *raw >= topology.positions.size() || *clock >= topology.positions.size() ||
        topology.positions[*raw].source != sourceIndex || topology.positions[*clock].source != sourceIndex ||
        topology.positions[*raw].kind != MediaNodeKind::RawRtpInput ||
        topology.positions[*clock].kind != MediaNodeKind::RtpPacketClockBinder)
        return invalid("Prepared packet wrappers require their actual raw input and clock binder");
    MediaPreparedPacketWrapperStoragePlan result{sourceIndex, stream, {}, 0, 0};
    for (const auto creator : {*raw, *clock}) {
        auto residence = MediaCompositionPayloadResidencePlanner::plan(topology, reference,
            creator, stream, MediaPayloadKind::Packet);
        if (!residence) return R::failure(residence.error());
        for (auto& owner : residence.value().contributions) {
            const auto count = owner.maximumReferences;
            result.roots.push_back({creator, Kind::GraphHolder, std::move(owner), count});
        }
        // Each creator executes serially. PacketClock can keep its old input
        // wrapper alive while constructing the replacement raw wrapper.
        result.roots.push_back({creator, Kind::Construction, std::nullopt, 1});
        result.roots.push_back({creator, Kind::CreditWait, std::nullopt, 1});
    }
    result.roots.push_back({*clock, Kind::Prefix, std::nullopt, containers.prefixObjects});
    result.roots.push_back({*raw, Kind::Acquiring, std::nullopt, containers.acquiringObjects});
    result.roots.push_back({*raw, Kind::Outputs, std::nullopt, containers.outputObjects});
    result.roots.push_back({*raw, Kind::AtomicCompletion, std::nullopt, maximumAtomicCompletion});
    // Snapshot references alias this same monotonic prepared prefix. Graph
    // contributions may overlap; their sum is a conservative allocation bound.
    for (const auto& root : result.roots) {
        auto sum = MediaCheckedArithmetic::add(result.maximumWrappers, root.maximumWrappers,
            "prepared raw packet wrapper roots");
        if (!sum) return R::failure(sum.error());
        result.maximumWrappers = sum.value();
    }
    auto geometry = FFmpegBufferFactory::packetAllocationGeometry();
    if (!geometry) return R::failure(geometry.error());
    auto unit = MediaPreparationMemoryResource::allocationFootprint(geometry.value().bytes, geometry.value().alignment);
    if (!unit) return R::failure(unit.error());
    auto bytes = MediaCheckedArithmetic::multiply(unit.value(), result.maximumWrappers,
        "prepared raw packet wrapper storage");
    if (!bytes) return R::failure(bytes.error());
    result.preparationBytes = bytes.value();
    return R::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaPreparedPacketWrapperStoragePlan>::failure(
        ::media::ErrorInfo::allocationFailed("prepared packet wrapper storage planning"));
}
}
