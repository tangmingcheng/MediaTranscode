#include "internal/graph/planner/realtime/MediaSourceLineageStoragePlanner.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <new>

namespace media::ffmpeg::graph {
::media::Result<MediaSourceLineageStoragePlan> MediaSourceLineageStoragePlanner::plan(
    const MediaCompositionGraphTopology& topology,
    const MediaCompositionReferenceResidencePlan& reference,
    std::size_t sourceIndex, std::uint64_t registryCapacity, std::uint64_t preparedPrefixCapacity,
    MediaHardwareTransferDirection transferDirection)
try {
    using R = ::media::Result<MediaSourceLineageStoragePlan>;
    using Family = MediaSourceLineageAllocationFamily;
    using Role = MediaSourceLineageRootRole;
    const auto invalid = [](const char* message) { return R::failure(::media::ErrorInfo::invalidArgument(message)); };
    if (!registryCapacity || !preparedPrefixCapacity)
        return invalid("Source lineage requires planned registry and compressed prefix capacities");
    if (transferDirection != MediaHardwareTransferDirection::None)
        return R::failure(::media::ErrorInfo::unsupported("Source lineage requires a proven passthrough transfer contract"));
    std::optional<std::size_t> packet, decoder, filter, transfer;
    for (const auto& binding : topology.bindings) {
        if (binding.role == compositionSourceRole(sourceIndex,
                {MediaSourceTopologyDomain::Rtp, static_cast<std::size_t>(MediaRtpClockRouteRole::VideoBinder)})) {
            if (packet) return invalid("Source lineage has repeated video binder identity");
            packet = binding.position;
        }
    }
    for (std::size_t i = 0; i < topology.positions.size(); ++i) {
        const auto& position = topology.positions[i];
        if (position.source != sourceIndex) continue;
        if (position.kind == MediaNodeKind::VideoDecode) {
            if (decoder) return invalid("Source lineage has repeated decoder identity");
            decoder = i;
        } else if (position.kind == MediaNodeKind::VideoFilter) {
            if (filter) return invalid("Source lineage has repeated filter identity");
            filter = i;
        } else if (position.kind == MediaNodeKind::HardwareTransfer) {
            if (transfer) return invalid("Source lineage has repeated transfer identity");
            transfer = i;
        } else if (position.kind == MediaNodeKind::VideoFrameRate || position.kind == MediaNodeKind::VideoOutputFanout) {
            return invalid("Source lineage does not admit additional frame clone creators");
        }
    }
    const auto transferRole = compositionSourceRole(sourceIndex,
        {MediaSourceTopologyDomain::VideoFrames, static_cast<std::size_t>(MediaVideoBranchRole::Transfer)});
    std::size_t transferBindings = 0;
    for (const auto& binding : topology.bindings) {
        if (binding.role != transferRole) continue;
        if (!transfer || binding.position != *transfer)
            return invalid("Source lineage transfer role differs from its source node");
        ++transferBindings;
    }
    if (!transfer || transferBindings != 1)
        return invalid("Source lineage requires one declared passthrough transfer");
    if (!packet || !decoder || *packet >= topology.positions.size() ||
        topology.positions[*packet].source != sourceIndex ||
        topology.positions[*packet].kind != MediaNodeKind::RtpPacketClockBinder)
        return invalid("Source lineage requires exact source binder and decoder identities");
    MediaSourceLineageStoragePlan result{sourceIndex, {}, {{{Family::AccessUnit, {}, 0, 0},
        {Family::Display, {}, 0, 0}, {Family::PacketWrapper, {}, 0, 0}, {Family::FrameWrapper, {}, 0, 0}}}, 0};
    auto& accessUnit = result.allocations[0];
    auto& display = result.allocations[1];
    auto& packetWrappers = result.allocations[2];
    auto& frameWrappers = result.allocations[3];
    const auto appendRoot = [&](MediaSourceLineageAllocationCost& target, MediaSourceLineageAllocationRoot root) {
        const auto index = result.roots.size();
        result.roots.push_back(std::move(root));
        target.rootIndices.push_back(index);
    };
    const auto project = [&](std::size_t position, MediaPayloadKind payload,
                             MediaSourceLineageAllocationCost& target) -> ::media::Status {
        auto residence = MediaCompositionPayloadResidencePlanner::plan(topology, reference, position,
            MediaStreamKind::Video, payload, MediaPayloadResidenceSelection::ReachableOwners);
        if (!residence) return ::media::Status::failure(residence.error());
        for (auto& owner : residence.value().contributions) {
            const auto count = owner.maximumReferences;
            appendRoot(target, {position, payload == MediaPayloadKind::Packet ? Role::PacketHolder : Role::FrameHolder,
                std::move(owner), count});
        }
        // Each actual creator has one serial construction, not one per AU output.
        appendRoot(target, {position, Role::Construction, std::nullopt, 1});
        return ::media::Status::success();
    };
    if (auto status = project(*packet, MediaPayloadKind::Packet, packetWrappers); !status) return R::failure(status.error());
    // The entire bound packet prefix is moved into the runtime binder. Decoded
    // startup frame units and the acquisition queue cannot bound this owner.
    appendRoot(packetWrappers, {*packet, Role::PreparedPrefix, std::nullopt, preparedPrefixCapacity});
    if (auto status = project(*decoder, MediaPayloadKind::Frame, frameWrappers); !status) return R::failure(status.error());
    if (filter) if (auto status = project(*filter, MediaPayloadKind::Frame, frameWrappers); !status) return R::failure(status.error());
    // A frame wrapper retains one display. Converted wrappers alias that display;
    // summing their roots is conservative, not a claim of distinct displays.
    display.rootIndices = frameWrappers.rootIndices;
    const auto lineageRoots = [&](MediaSourceLineageAllocationCost& target, std::size_t position) {
        appendRoot(target, {position, Role::Registry, std::nullopt, registryCapacity});
        appendRoot(target, {position, Role::PendingLineage, std::nullopt, 1});
        appendRoot(target, {position, Role::ResolvedLineage, std::nullopt, 1});
    };
    if (filter) lineageRoots(display, *filter);
    accessUnit.rootIndices = packetWrappers.rootIndices;
    lineageRoots(accessUnit, *decoder);
    // display.accessUnit and the frame wrapper's direct AU are the same owner.
    accessUnit.rootIndices.insert(accessUnit.rootIndices.end(), display.rootIndices.begin(), display.rootIndices.end());
    if (auto status = rebind(result, topology, reference, registryCapacity, preparedPrefixCapacity); !status) return R::failure(status.error());
    return R::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaSourceLineageStoragePlan>::failure(
        ::media::ErrorInfo::allocationFailed("source lineage storage planning"));
}

::media::Status MediaSourceLineageStoragePlanner::rebind(MediaSourceLineageStoragePlan& plan,
    const MediaCompositionGraphTopology& topology, const MediaCompositionReferenceResidencePlan& reference,
    std::uint64_t registryCapacity, std::uint64_t preparedPrefixCapacity)
{
    using Role = MediaSourceLineageRootRole;
    const auto invalid = [] { return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Source lineage catalog differs from its fixed source creators")); };
    if (!registryCapacity || !preparedPrefixCapacity) return invalid();
    for (auto& root : plan.roots) {
        if (root.creatorPosition >= topology.positions.size()) return invalid();
        const auto& creator = topology.positions[root.creatorPosition];
        const bool binder = creator.kind == MediaNodeKind::RtpPacketClockBinder;
        const bool frame = creator.kind == MediaNodeKind::VideoDecode || creator.kind == MediaNodeKind::VideoFilter;
        if (creator.source != plan.sourceIndex || (!binder && !frame)) return invalid();
        const bool holder = root.role == Role::PacketHolder || root.role == Role::FrameHolder;
        if (holder != root.holder.has_value()) return invalid();
        switch (root.role) {
        case Role::PacketHolder:
        case Role::FrameHolder: {
            if ((root.role == Role::PacketHolder) != binder) return invalid();
            auto status = MediaCompositionPayloadResidencePlanner::rebindOwner(topology, reference,
                MediaStreamKind::Video, binder ? MediaPayloadKind::Packet : MediaPayloadKind::Frame, *root.holder);
            if (!status) return status;
            root.maximumReferences = root.holder->maximumReferences;
            break;
        }
        case Role::PreparedPrefix:
            if (!binder) return invalid();
            root.maximumReferences = preparedPrefixCapacity;
            break;
        case Role::Registry:
            if (!frame) return invalid();
            root.maximumReferences = registryCapacity;
            break;
        case Role::PendingLineage:
        case Role::ResolvedLineage:
            if (!frame || root.maximumReferences != 1) return invalid();
            break;
        case Role::Construction:
            if (root.maximumReferences != 1) return invalid();
            break;
        default:
            return invalid();
        }
    }
    return evaluate(plan);
}

::media::Status MediaSourceLineageStoragePlanner::evaluate(MediaSourceLineageStoragePlan& plan)
{
    using Family = MediaSourceLineageAllocationFamily;
    auto geometry = MediaVideoDecodeBuffer::allocationGeometry();
    if (!geometry) return ::media::Status::failure(geometry.error());
    const std::array geometries{geometry.value().accessUnit, geometry.value().display,
        geometry.value().wrapper, geometry.value().wrapper};
    constexpr std::array families{Family::AccessUnit, Family::Display, Family::PacketWrapper, Family::FrameWrapper};
    std::array<std::uint64_t, families.size()> counts{}, charges{};
    std::uint64_t totalBytes = 0;
    for (std::size_t i = 0; i < plan.allocations.size(); ++i) {
        const auto& allocation = plan.allocations[i];
        if (allocation.family != families[i])
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source lineage allocation family order is invalid"));
        for (const auto index : allocation.rootIndices) {
            if (index >= plan.roots.size())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source lineage allocation root index is absent"));
            const auto& root = plan.roots[index];
            if (root.holder && root.holder->maximumReferences != root.maximumReferences)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source lineage holder count differs from its root"));
            auto sum = MediaCheckedArithmetic::add(counts[i], root.maximumReferences,
                "source lineage allocation roots");
            if (!sum) return ::media::Status::failure(sum.error());
            counts[i] = sum.value();
        }
        auto unit = MediaPreparationMemoryResource::allocationFootprint(geometries[i].bytes, geometries[i].alignment);
        if (!unit) return ::media::Status::failure(unit.error());
        auto bytes = MediaCheckedArithmetic::multiply(unit.value(), counts[i],
            "source lineage preparation allocation bytes");
        if (!bytes) return ::media::Status::failure(bytes.error());
        charges[i] = bytes.value();
        auto total = MediaCheckedArithmetic::add(totalBytes, bytes.value(), "source lineage preparation storage");
        if (!total) return ::media::Status::failure(total.error());
        totalBytes = total.value();
    }
    for (std::size_t i = 0; i < plan.allocations.size(); ++i) {
        plan.allocations[i].maximumAllocations = counts[i];
        plan.allocations[i].preparationBytes = charges[i];
    }
    plan.preparationBytes = totalBytes;
    return ::media::Status::success();
}
}
