#include "internal/graph/builder/codec/MediaDecoderInputRetentionOptionCodec.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"
#include <limits>
extern "C" {
#include <libavcodec/avcodec.h>
}
namespace media::ffmpeg::graph {
::media::Result<MediaNodeOptions> MediaDecoderInputRetentionOptionCodec::encode(const MediaDecoderInputRetention& retention)
{
    using Result = ::media::Result<MediaNodeOptions>;
    MediaNodeOptions options;
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (retention.threadCount <= 0 || retention.threadType < 0 || retention.authority.empty() ||
        retention.mainHandoffPackets > maximum ||
        retention.frameWorkerPackets > maximum - retention.mainHandoffPackets ||
        retention.serialPrivatePackets > maximum - retention.mainHandoffPackets - retention.frameWorkerPackets ||
        retention.maximumInternalPackets() == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoder options require valid prepared input retention facts"));
    }
    options.set("decoder.pipeline.input_retention.thread_count", std::to_string(retention.threadCount));
    options.set("decoder.pipeline.input_retention.thread_type", std::to_string(retention.threadType));
    options.set("decoder.pipeline.input_retention.main_handoff_packets", std::to_string(retention.mainHandoffPackets));
    options.set("decoder.pipeline.input_retention.frame_worker_packets", std::to_string(retention.frameWorkerPackets));
    options.set("decoder.pipeline.input_retention.serial_private_packets", std::to_string(retention.serialPrivatePackets));
    options.set("decoder.pipeline.input_retention.maximum_internal_packets", std::to_string(retention.maximumInternalPackets()));
    options.set("decoder.pipeline.input_retention.authority", retention.authority);
    return Result::success(std::move(options));
}

::media::Status MediaDecoderInputRetentionOptionCodec::validateOpened(
    const AVCodecContext& context, const MediaNodeOptions& options)
{
    if (!options.has("decoder.pipeline.input_retention.maximum_internal_packets"))
        return ::media::Status::success();
    auto count = requiredPositiveIntNodeOption(&options, "CodecResolverNode",
        "decoder.pipeline.input_retention.thread_count");
    auto type = requiredNonNegativeIntNodeOption(&options, "CodecResolverNode",
        "decoder.pipeline.input_retention.thread_type");
    auto planned = requiredPositiveInt64NodeOption(&options, "CodecResolverNode",
        "decoder.pipeline.input_retention.maximum_internal_packets");
    if (!count || !type || !planned) return ::media::Status::failure(
        !count ? count.error() : !type ? type.error() : planned.error());
    auto observed = MediaDecoderInputRetentionAdapter::readAfterOpen(
        context, options.value("pipeline.hwaccel"));
    if (!observed || context.thread_count != count.value() ||
        context.thread_type != type.value() ||
        observed->maximumInternalPackets() > static_cast<std::uint64_t>(planned.value())) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "opened decoder threads or input retention differ from the prepared allocation contract"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
