#include "internal/graph/planner/realtime/MediaWireProducerStoragePlanner.h"
#include "internal/graph/nodes/output/MediaWireCommitPreparation.h"
#include <algorithm>
#include <initializer_list>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Result<std::size_t> sum(std::initializer_list<std::size_t> values)
{
    std::size_t total = 0;
    for (const auto value : values) {
        if (value > (std::numeric_limits<std::size_t>::max)() - total)
            return ::media::Result<std::size_t>::failure(::media::ErrorInfo::invalidArgument(
                "Wire producer storage footprint overflows"));
        total += value;
    }
    return ::media::Result<std::size_t>::success(total);
}

::media::Result<MediaWireProducerStoragePlan> phase(
    std::size_t entries, std::size_t payload, std::size_t workspace,
    std::size_t collection, const MediaDatagramCommitStoragePlan& commit,
    const MediaDatagramBatchPlan& batch)
{
    using Result = ::media::Result<MediaWireProducerStoragePlan>;
    auto transaction = MediaWireCommitPreparation::storageFootprint(commit, entries);
    auto overhead = MediaWireDatagramBatchPartitionBuilder::storageOverheadContribution(entries, commit.batchGeometry);
    if (!transaction || !overhead) return Result::failure(!transaction ? transaction.error() : overhead.error());
    auto wire = sum({payload, overhead.value(), workspace, transaction.value()});
    if (!wire) return Result::failure(wire.error());
    auto reservation = sum({collection, wire.value()});
    auto published = MediaWireDatagramBatchPartitionBuilder::publishedStorageUpperBound(
        entries, payload, commit.batchGeometry);
    auto tail = MediaWireDatagramBatchPartitionBuilder::partitionStorageFootprint(
        (std::min)(entries, static_cast<std::size_t>(batch.maximumDatagrams)),
        (std::min)(payload, static_cast<std::size_t>(batch.maximumBytes)), commit.batchGeometry);
    auto publication = MediaWireDatagramBatchPartitionBuilder::publicationCollectionStorageFootprint(entries);
    if (!reservation || !published || !tail || !publication)
        return Result::failure(!reservation ? reservation.error() : !published ? published.error() :
            !tail ? tail.error() : publication.error());
    auto slot = sum({publication.value(), tail.value()});
    auto active = sum({published.value(), transaction.value()});
    auto handoff = sum({tail.value(), transaction.value()});
    if (!slot || !active || !handoff)
        return Result::failure(!slot ? slot.error() : !active ? active.error() : handoff.error());
    return Result::success({wire.value(), reservation.value(),
        (std::max)(reservation.value(), slot.value()), active.value(), handoff.value()});
}
} // namespace

::media::Result<MediaWireProducerStoragePlan> MediaWireProducerStoragePlanner::plan(
    const MediaWireProducerStorageFacts& facts,
    const MediaDatagramCommitStoragePlan& commit, const MediaDatagramBatchPlan& batch)
{
    using Result = ::media::Result<MediaWireProducerStoragePlan>;
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    if (!facts.maximumEntries || facts.maximumPayloadBytes < facts.maximumEntries ||
        !batch.maximumDatagrams || batch.maximumDatagrams > maximum ||
        !batch.maximumBytes || batch.maximumBytes > maximum ||
        (facts.terminalPayloadBytes && (!*facts.terminalPayloadBytes ||
            *facts.terminalPayloadBytes > batch.maximumBytes ||
            commit.kind != MediaDatagramCommitStorageKind::Rtp)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Wire producer storage requires bounded allocation facts"));
    auto result = phase(facts.maximumEntries, facts.maximumPayloadBytes,
        facts.workspaceBytes, facts.collectionBytes, commit, batch);
    if (!result || !facts.terminalPayloadBytes) return result;
    // A terminal compound has one inline entry, no collection and no workspace.
    auto terminal = phase(1, *facts.terminalPayloadBytes, 0, 0, commit, batch);
    if (!terminal) return Result::failure(terminal.error());
    result.value().producerSlotBytes = (std::max)(result.value().producerSlotBytes, terminal.value().producerSlotBytes);
    result.value().activeTransactionBytes = (std::max)(result.value().activeTransactionBytes, terminal.value().activeTransactionBytes);
    result.value().senderHandoffBytes = (std::max)(result.value().senderHandoffBytes, terminal.value().senderHandoffBytes);
    return result;
}
} // namespace media::ffmpeg::graph
