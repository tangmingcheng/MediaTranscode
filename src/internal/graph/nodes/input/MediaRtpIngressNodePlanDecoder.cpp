#include "internal/graph/nodes/input/MediaRtpIngressNodePlanDecoder.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::size_t> requiredSize(
    const MediaNodeOptions* options,
    const char* key)
{
    auto value = requiredPositiveInt64NodeOption(
        options, "RawRtpInputNode", key);
    if (!value) return ::media::Result<std::size_t>::failure(value.error());
    if (static_cast<std::uint64_t>(value.value()) >
        (std::numeric_limits<std::size_t>::max)()) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("RawRtpInputNode option exceeds size range: ") + key));
    }
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(value.value()));
}

} // namespace

::media::Result<MediaRtpIngressPlan>
MediaRtpIngressNodePlanDecoder::decode(const MediaNodeOptions* options)
{
    auto adapter = requiredNonNegativeIntNodeOption(
        options, "RawRtpInputNode", "rtp.ingress.adapter_kind");
    auto socketCapacity = requiredSize(
        options, "rtp.ingress.socket_receive_capacity_bytes");
    auto maximumDatagram = requiredSize(
        options, "rtp.ingress.maximum_datagram_bytes");
    auto batchBytes = requiredSize(
        options, "rtp.ingress.batch_byte_capacity");
    auto descriptorCapacity = requiredSize(
        options, "rtp.ingress.descriptor_capacity");
    auto alignment = requiredSize(
        options, "rtp.ingress.required_buffer_alignment_bytes");
    auto reorderWindow = requiredSize(
        options, "rtp.ingress.reorder_window_packets");
    auto reorderDelay = requiredPositiveInt64NodeOption(
        options, "RawRtpInputNode",
        "rtp.ingress.maximum_reorder_delay_ns");
    auto storage = requiredNonNegativeIntNodeOption(
        options, "RawRtpInputNode", "rtp.ingress.storage_ownership");
    auto cancellation = requiredNonNegativeIntNodeOption(
        options, "RawRtpInputNode", "rtp.ingress.cancellation_contract");
    auto completion = requiredNonNegativeIntNodeOption(
        options, "RawRtpInputNode", "rtp.ingress.completion_evidence");
    if (!adapter || !socketCapacity || !maximumDatagram || !batchBytes ||
        !descriptorCapacity || !alignment || !reorderWindow ||
        !reorderDelay || !storage || !cancellation || !completion) {
        const ::media::ErrorInfo* error = nullptr;
        if (!adapter) error = &adapter.error();
        else if (!socketCapacity) error = &socketCapacity.error();
        else if (!maximumDatagram) error = &maximumDatagram.error();
        else if (!batchBytes) error = &batchBytes.error();
        else if (!descriptorCapacity) error = &descriptorCapacity.error();
        else if (!alignment) error = &alignment.error();
        else if (!reorderWindow) error = &reorderWindow.error();
        else if (!reorderDelay) error = &reorderDelay.error();
        else if (!storage) error = &storage.error();
        else if (!cancellation) error = &cancellation.error();
        else error = &completion.error();
        return ::media::Result<MediaRtpIngressPlan>::failure(*error);
    }
    return MediaRtpIngressPlan::fromFacts({
        static_cast<MediaRtpIngressAdapterKind>(adapter.value()),
        socketCapacity.value(), maximumDatagram.value(), batchBytes.value(),
        descriptorCapacity.value(), alignment.value(), reorderWindow.value(),
        reorderDelay.value(),
        static_cast<MediaRtpIngressStorageOwnership>(storage.value()),
        static_cast<MediaRtpIngressCancellationContract>(cancellation.value()),
        static_cast<MediaRtpIngressCompletionEvidence>(completion.value())});
}

} // namespace media::ffmpeg::graph
