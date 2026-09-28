#include "internal/graph/runtime/buffer/MediaPreparedVideoDecoder.h"
#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"
#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"

#include <cstring>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool sameSideData(const AVPacketSideData* a, int aCount,
    const AVPacketSideData* b, int bCount)
{
    if (aCount < 0 || aCount != bCount || (aCount && (!a || !b))) return false;
    for (int i = 0; i < aCount; ++i) {
        if (a[i].type != b[i].type || a[i].size != b[i].size ||
            (a[i].size && (!a[i].data || !b[i].data ||
             std::memcmp(a[i].data, b[i].data, a[i].size) != 0))) return false;
    }
    return true;
}

bool sameVideoParameters(const AVCodecParameters& a, const AVCodecParameters& b)
{
    return a.codec_type == AVMEDIA_TYPE_VIDEO && b.codec_type == a.codec_type &&
        a.codec_id == b.codec_id && a.codec_tag == b.codec_tag && a.format == b.format &&
        a.bit_rate == b.bit_rate && a.bits_per_coded_sample == b.bits_per_coded_sample &&
        a.bits_per_raw_sample == b.bits_per_raw_sample &&
        a.profile == b.profile && a.level == b.level &&
        a.framerate.num == b.framerate.num && a.framerate.den == b.framerate.den &&
        a.width == b.width && a.height == b.height &&
        a.sample_aspect_ratio.num == b.sample_aspect_ratio.num &&
        a.sample_aspect_ratio.den == b.sample_aspect_ratio.den &&
        a.field_order == b.field_order && a.color_range == b.color_range &&
        a.color_primaries == b.color_primaries && a.color_trc == b.color_trc &&
        a.color_space == b.color_space && a.chroma_location == b.chroma_location &&
        a.video_delay == b.video_delay &&
        sameSideData(a.coded_side_data, a.nb_coded_side_data, b.coded_side_data, b.nb_coded_side_data) &&
        a.extradata_size == b.extradata_size &&
        a.extradata_size >= 0 && (a.extradata_size == 0 ||
            (a.extradata && b.extradata &&
             std::memcmp(a.extradata, b.extradata, a.extradata_size) == 0));
}

} // namespace

MediaPreparedVideoDecoder::MediaPreparedVideoDecoder(
    CodecResolverDecoderContextBuildResult decoder,
    ::media::ffmpeg::CodecParametersPtr parameters,
    int streamIndex, MediaTimeDescriptor time, MediaHardwareDescriptor frameContract)
    : m_decoder(std::move(decoder)), m_parameters(std::move(parameters)),
      m_streamIndex(streamIndex), m_time(time), m_frameContract(std::move(frameContract))
{
}

::media::Result<std::shared_ptr<MediaPreparedVideoDecoder>>
MediaPreparedVideoDecoder::create(CodecResolverDecoderContextBuildResult decoderOwner,
    MediaPreparationStorageLease storageOwner, const FFmpegInputStreamSnapshot& source,
    MediaHardwareDescriptor frameContract)
{
    using Result = ::media::Result<std::shared_ptr<MediaPreparedVideoDecoder>>;
    struct Ownership final {
        MediaPreparationStorageLease storage;
        CodecResolverDecoderContextBuildResult decoder;
    } owner{std::move(storageOwner), std::move(decoderOwner)};
    auto& decoder = owner.decoder;
    auto& storage = owner.storage;
    if (!decoder.context || !avcodec_is_open(decoder.context.get()) ||
        !av_codec_is_decoder(decoder.context->codec) || !storage ||
        source.streamKind != MediaStreamKind::Video || !source.codecParametersComplete() ||
        source.time.timeBase.num <= 0 || source.time.timeBase.den <= 0 ||
        decoder.runtimeFacts.decoderName.empty() ||
        decoder.context.get_deleter().storageOwner) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared decoder requires its opened owner, storage, and authoritative source snapshot"));
    }
    const auto& p = *source.codec.parameters();
    const auto& context = *decoder.context;
    if (p.codec_type != AVMEDIA_TYPE_VIDEO || p.codec_id != context.codec_id ||
        p.width != context.width || p.height != context.height ||
        !sameSideData(p.coded_side_data, p.nb_coded_side_data,
            context.coded_side_data, context.nb_coded_side_data) ||
        context.pkt_timebase.num != source.time.timeBase.num ||
        context.pkt_timebase.den != source.time.timeBase.den ||
        p.extradata_size != context.extradata_size || p.extradata_size < 0 ||
        (p.extradata_size && (!p.extradata || !context.extradata ||
         std::memcmp(p.extradata, context.extradata, p.extradata_size) != 0))) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared decoder does not match its source codec parameters and time base"));
    }
    try {
        auto parameters = source.cloneCodecParameters();
        if (!parameters) return Result::failure(parameters.error());
        decoder.context.get_deleter().storageOwner =
            std::make_shared<MediaPreparationStorageLease>(std::move(storage));
        return Result::success(std::shared_ptr<MediaPreparedVideoDecoder>(
            new MediaPreparedVideoDecoder(std::move(decoder),
                std::move(parameters).value(), source.index, source.time, std::move(frameContract))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Prepared decoder handoff allocation failed"));
    }
}

::media::Result<CodecResolverDecoderContextBuildResult>
MediaPreparedVideoDecoder::claim(const FFmpegInputStreamSnapshot& source,
    const MediaNodeOptions& options)
{
    using Result = ::media::Result<CodecResolverDecoderContextBuildResult>;
    std::scoped_lock lock(m_mutex);
    if (!m_decoder) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Prepared decoder has already been claimed"));
    if (!options.has("decoder.pipeline.input_retention.maximum_internal_packets"))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared source publication requires planned decoder retention"));
    const auto& context = *m_decoder->context;
    if (auto retained = CodecResolverDecoderContextBuilder::validateInputRetention(context, options); !retained)
        return Result::failure(retained.error());
    auto frameContract = MediaVideoFrameContractValidator::contractFromOptions(
        &options, "decoder.pipeline.output", "prepared decoder claim");
    if (!frameContract) return Result::failure(frameContract.error());
    const auto& frame = frameContract.value();
    auto copyOpaque = parseMediaVideoLineageCopyOpaqueOption(
        &options, "video.lineage.decoder_copy_opaque");
    if (!copyOpaque) return Result::failure(copyOpaque.error());
    bool contextCopiesOpaque = false;
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
    contextCopiesOpaque = (context.flags & AV_CODEC_FLAG_COPY_OPAQUE) != 0;
#endif
    if (options.value("decoder") != m_decoder->runtimeFacts.decoderName ||
        frame.deviceKind != m_frameContract.deviceKind ||
        frame.frameKind != m_frameContract.frameKind ||
        frame.pixelFormat != m_frameContract.pixelFormat ||
        frame.surfacePixelFormat != m_frameContract.surfacePixelFormat ||
        frame.size != m_frameContract.size ||
        frame.requiresHardwareDeviceContext != m_frameContract.requiresHardwareDeviceContext ||
        frame.requiresHardwareFramesContext != m_frameContract.requiresHardwareFramesContext ||
        copyOpaque.value() != contextCopiesOpaque) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared decoder differs from the production planner codec contract"));
    }
    const auto* parameters = source.codec.parameters();
    if (source.streamKind != MediaStreamKind::Video || source.index != m_streamIndex ||
        source.time.timeBase != m_time.timeBase || source.time.frameRate != m_time.frameRate ||
        !parameters || !sameVideoParameters(*parameters, *m_parameters)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Production source snapshot differs from the prepared decoder source"));
    }
    auto decoder = std::move(*m_decoder);
    m_decoder.reset();
    m_parameters.reset();
    return Result::success(std::move(decoder));
}

::media::Status MediaPreparedVideoDecoder::validateHardwareDevice(const AVBufferRef* device) const
{
    std::scoped_lock lock(m_mutex);
    if (!m_decoder) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "Prepared decoder has already been claimed"));
    // DRM PRIME import validates the actual descriptor at the frame/copy
    // boundary; it does not require an FFmpeg AVHWDeviceContext identity.
    if (!m_frameContract.requiresHardwareDeviceContext)
        return ::media::Status::success();
    if (!device || !device->data || !m_decoder->hardwareDevice ||
        m_decoder->hardwareDevice->data != device->data) {
        return ::media::Status::failure(::media::ErrorInfo::hardwareUnavailable(
            "Prepared source decoder does not own the composition hardware device"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
