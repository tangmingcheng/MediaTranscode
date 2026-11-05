#pragma once

#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"
#include <span>
#include <variant>

namespace media::ffmpeg::graph {

// A projection for one stream/payload domain. Indices are topology positions,
// not fabricated graph nodes. An owner is charged once when any retaining
// position is reachable. Edge owners use their source position.
struct MediaPayloadResidenceConnection final {
    std::size_t from;
    std::size_t to;
};
enum class MediaPayloadResidenceOwnerKind {
    Node, Edge, StartupNode, StartupEdge, Shared
};
enum class MediaPayloadResidenceSelection { LiveOwners, ReachableOwners };
struct MediaPayloadResidenceOwner final {
    std::vector<std::size_t> retainingPositions;
    std::uint64_t maximumReferences;
    MediaPayloadResidenceOwnerKind kind;
    // Index in the complete topology, not the filtered media connections.
    std::optional<std::size_t> connection;
    // Set by projection: index in the original view, not contributions.
    // Meaningful only with that fixed topology, stream/payload and producer.
    std::optional<std::size_t> projectionSlot;
};
struct MediaPayloadResidenceTopology final {
    std::size_t nodeCount;
    std::vector<MediaPayloadResidenceConnection> connections;
    std::vector<MediaPayloadResidenceOwner> owners;
};
// A single position is stored inline; shared owners borrow their position set.
struct MediaPayloadResidenceOwnerView final {
    std::variant<std::size_t, std::span<const std::size_t>> retainingPositions;
    std::uint64_t maximumReferences;
    MediaPayloadResidenceOwnerKind kind;
    std::optional<std::size_t> connection;
    std::size_t positionCount() const noexcept;
    std::size_t positionAt(std::size_t index) const noexcept;
};
// Synchronous read-only adapter. Slot identities remain stable even when a
// stream/payload projection excludes a connection or owner. Implementations
// must not allocate on successful reads or mutate during a planning call.
class MediaPayloadResidenceTopologyView {
public:
    virtual ~MediaPayloadResidenceTopologyView() = default;
    virtual std::size_t nodeCount() const noexcept = 0;
    virtual std::size_t connectionSlots() const noexcept = 0;
    virtual std::size_t ownerSlots() const noexcept = 0;
    virtual ::media::Result<std::optional<MediaPayloadResidenceConnection>> connectionAt(std::size_t index) const = 0;
    virtual ::media::Result<std::optional<MediaPayloadResidenceOwnerView>> ownerAt(std::size_t index) const = 0;
};
struct MediaPayloadResidenceProjection final {
    std::vector<std::size_t> reachablePositions;
    std::uint64_t maximumRetainedObjects;
    std::vector<MediaPayloadResidenceOwner> contributions;
};

// Borrows only the caller's traversal workspace, never the topology storage.
// Keep that workspace alive and unchanged while inspecting this result.
struct MediaPayloadResidenceEvaluation final {
    std::span<const std::size_t> reachablePositions;
    std::uint64_t maximumRetainedObjects;
    bool retains(const MediaPayloadResidenceOwnerView& owner,
        MediaPayloadResidenceSelection selection) const noexcept;
};

struct MediaGraphPayloadResidenceContribution final {
    std::vector<MediaNodeId> retainingNodes;
    std::uint64_t maximumReferences;
    MediaPayloadResidenceOwnerKind kind;
    std::optional<MediaEdgeId> edge;
};
struct MediaGraphPayloadResidence final {
    std::vector<MediaNodeId> reachableNodes;
    std::uint64_t maximumRetainedObjects;
    std::vector<MediaGraphPayloadResidenceContribution> contributions;
};

struct MediaGraphPayloadResidenceFacts final {
    std::span<const MediaFinalGraphResourceLedgerEntry> storage;
    std::span<const MediaStartupEventMediaBound> startupEvents;
    std::span<const MediaStartupNodeMediaBound> startupNodes;
};

// Bounds references to one producer's allocation, not frame cadence or the
// decoder's internal surface pool. Startup wrappers use the validated final
// graph products; ordinary storage remains a conservative reference bound.
class MediaGraphPayloadResidencePlanner final {
public:
    // Successful evaluation performs no allocations. Workspace must contain
    // at least nodeCount entries and must not alias topology storage. All
    // entries used are initialized here; failure may leave them modified.
    static ::media::Result<MediaPayloadResidenceEvaluation> evaluate(
        const MediaPayloadResidenceTopologyView& topology, std::size_t producerPosition,
        std::span<std::size_t> workspace);
    static ::media::Result<MediaPayloadResidenceProjection> plan(
        const MediaPayloadResidenceTopologyView& topology, std::size_t producerPosition,
        MediaPayloadResidenceSelection selection);
    static ::media::Result<MediaPayloadResidenceProjection> plan(
        const MediaPayloadResidenceTopology& topology, std::size_t producerPosition);
    static ::media::Result<MediaGraphPayloadResidence> plan(
        const MediaGraph& graph,
        const MediaGraphPayloadProducerKey& producer,
        const MediaGraphPayloadResidenceFacts& facts);
};

} // namespace media::ffmpeg::graph
