#include "internal/graph/builder/codec/MediaDecoderOpaqueRetentionOptionCodec.h"
#include "internal/graph/planner/capability/MediaDecoderOpaqueRetentionAdapter.h"
#include "internal/graph/planner/realtime/MediaDecoderOpaqueRetentionPlanner.h"
namespace media::ffmpeg::graph {
::media::Result<MediaNodeOptions> MediaDecoderOpaqueRetentionOptionCodec::encode(const MediaDecoderOpaqueRetention& evidence)
{
    using Result = ::media::Result<MediaNodeOptions>;
    auto capacity = MediaDecoderOpaqueRetentionPlanner::registryCapacity(evidence);
    if (!capacity) return Result::failure(capacity.error());
    MediaNodeOptions options;
    options.set("decoder.opaque_retention.capacity", std::to_string(capacity.value()));
    if (const auto* backend = std::get_if<MediaBackendDecoderOpaqueRetention>(&evidence)) {
        options.set("decoder.opaque_retention.thread_count", std::to_string(backend->threadCount));
        options.set("decoder.opaque_retention.active_thread_type", std::to_string(backend->activeThreadType));
        options.set("decoder.opaque_retention.backend_version", std::to_string(backend->contractVersion));
        options.set("decoder.opaque_retention.backend_retained_packets", std::to_string(backend->retainedPackets));
    } else {
        const auto& facts = std::get<MediaNativeDecoderOpaqueRetention>(evidence);
        options.set("decoder.opaque_retention.thread_count", std::to_string(facts.threadCount));
        options.set("decoder.opaque_retention.active_thread_type", std::to_string(facts.activeThreadType));
        options.set("decoder.opaque_retention.private_frame_slots", std::to_string(facts.privateFrameSlots));
        options.set("decoder.opaque_retention.generic_slots", std::to_string(facts.genericSlots));
        options.set("decoder.opaque_retention.output_fifo", facts.outputFifo ? "1" : "0");
        options.set("decoder.opaque_retention.frame_workers", std::to_string(facts.frameWorkers));
    }
    return Result::success(std::move(options));
}
::media::Status MediaDecoderOpaqueRetentionOptionCodec::validateOpened(
    const AVCodecContext& context, const MediaNodeOptions& options)
{
    if (!options.has("decoder.opaque_retention.capacity")) {
        for (const auto& [key, value] : options.values()) {
            if (key.starts_with("decoder.opaque_retention."))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Decoder opaque retention contract is incomplete"));
        }
        return ::media::Status::success();
    }
    auto facts = MediaDecoderOpaqueRetentionAdapter::readAfterOpen(context, options.value("pipeline.hwaccel"));
    if (!facts) return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "Opened decoder has no matching opaque retention implementation"));
    auto observed = encode(*facts);
    if (!observed) return ::media::Status::failure(observed.error());
    for (const auto& [key, value] : observed.value().values()) {
        if (!options.has(key) || options.value(key) != value)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Opened decoder opaque retention differs from its prepared contract"));
    }
    for (const auto& [key, value] : options.values()) {
        if (key.starts_with("decoder.opaque_retention.") && !observed.value().has(key))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Opened decoder opaque retention contains an unknown or mixed contract field"));
    }
    return ::media::Status::success();
}
}
