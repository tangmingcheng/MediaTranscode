#include "internal/graph/planner/realtime/MediaRawRtpStoragePlanner.h"
#include "internal/graph/nodes/output/MediaRtpAccessUnitCollection.h"
#include "internal/graph/nodes/output/MediaRtpWirePreparationWorkspace.h"
#include "internal/graph/planner/realtime/MediaWireProducerStoragePlanner.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"
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
                "Raw RTP storage footprint overflows"));
        total += value;
    }
    return ::media::Result<std::size_t>::success(total);
}
} // namespace

::media::Result<MediaRawRtpStoragePlan> MediaRawRtpStoragePlanner::plan(
    const MediaRawRtpProducerPlan& producer,
    const MediaDatagramCommitStoragePlan& commit, const MediaDatagramBatchPlan& batch)
{
    using Result = ::media::Result<MediaRawRtpStoragePlan>;
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    const auto count = producer.emission.maximumDatagramsPerAccessUnit();
    const auto bytes = producer.emission.maximumDatagramBytes();
    if (!count || count >= maximum || !bytes || count > maximum / bytes ||
        !producer.maximumRtcpCompoundBytes || producer.maximumRtcpCompoundBytes > bytes ||
        commit.kind != MediaDatagramCommitStorageKind::Rtp ||
        !batch.maximumDatagrams || batch.maximumDatagrams > maximum ||
        batch.maximumBytes < bytes || batch.maximumBytes > maximum ||
        (producer.role != MediaDatagramProtocolEndpointRole::VideoRtp &&
         producer.role != MediaDatagramProtocolEndpointRole::AudioRtp))
        return Result::failure(::media::ErrorInfo::invalidArgument("Raw RTP storage requires complete bounded producer facts"));
    auto terminalPayload = sum({static_cast<std::size_t>(producer.maximumRtcpCompoundBytes),
        static_cast<std::size_t>(MediaRtcpWireGeometry::byePayloadBytes())});
    if (!terminalPayload) return Result::failure(terminalPayload.error());
    if (terminalPayload.value() > bytes || terminalPayload.value() > batch.maximumBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTCP terminal compound exceeds the planned datagram or batch bound"));
    const auto media = static_cast<std::size_t>(count);
    const auto entries = media + 1;
    auto payload = sum({media * bytes, static_cast<std::size_t>(producer.maximumRtcpCompoundBytes)});
    auto collection = MediaRtpAccessUnitCollection::storageFootprint(media, bytes);
    auto workspace = MediaRtpWirePreparationWorkspace::storageFootprint(media, entries);
    if (!payload || !collection || !workspace)
        return Result::failure(!payload ? payload.error() : !collection ? collection.error() : workspace.error());
    auto storage = MediaWireProducerStoragePlanner::plan(
        {entries, payload.value(), workspace.value(), collection.value(), terminalPayload.value()}, commit, batch);
    if (!storage) return Result::failure(storage.error());
    return Result::success({producer.role, media, bytes, collection.value(),
        storage.value().wirePreparationBytes, storage.value().reservationBytes,
        storage.value().producerSlotBytes, storage.value().activeTransactionBytes,
        storage.value().senderHandoffBytes});
}

::media::Result<std::size_t> MediaRawRtpStoragePlanner::ownedStorageUpperBound(
    std::span<const MediaRawRtpStoragePlan> producers)
{
    std::size_t slots = 0;
    std::size_t active = 0;
    std::size_t handoff = 0;
    for (const auto& producer : producers) {
        auto next = sum({slots, producer.producerSlotBytes});
        if (!next) return next;
        slots = next.value();
        active = (std::max)(active, producer.activeTransactionBytes);
        handoff = (std::max)(handoff, producer.senderHandoffBytes);
    }
    return sum({slots, active, handoff});
}
} // namespace media::ffmpeg::graph
