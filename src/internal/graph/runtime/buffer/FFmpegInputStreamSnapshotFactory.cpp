#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshotFactory.h"

#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

namespace media::ffmpeg::graph {
namespace {

MediaStreamKind streamKindFromCodecType(AVMediaType type) noexcept
{
    if (type == AVMEDIA_TYPE_VIDEO) return MediaStreamKind::Video;
    if (type == AVMEDIA_TYPE_AUDIO) return MediaStreamKind::Audio;
    return MediaStreamKind::Unknown;
}

AVRational resolvedSourceFrameRate(const AVFormatContext& context,
                                   const AVStream& stream) noexcept
{
    AVRational frameRate = av_guess_frame_rate(
        const_cast<AVFormatContext*>(&context), const_cast<AVStream*>(&stream), nullptr);
    if (frameRate.num > 0 && frameRate.den > 0) return frameRate;
    if (stream.avg_frame_rate.num > 0 && stream.avg_frame_rate.den > 0) return stream.avg_frame_rate;
    if (stream.r_frame_rate.num > 0 && stream.r_frame_rate.den > 0) return stream.r_frame_rate;
    return AVRational{0, 1};
}

MediaTimeDescriptor snapshotTime(const AVFormatContext& context, const AVStream& stream)
{
    MediaTimeDescriptor time;
    time.timeBase = FFmpegDescriptorMapper::toRational(stream.time_base);
    time.frameRate = FFmpegDescriptorMapper::toRational(resolvedSourceFrameRate(context, stream));
    time.startTime = stream.start_time;
    time.duration = stream.duration;
    return time;
}

} // namespace

::media::Result<std::vector<FFmpegInputStreamSnapshot>>
FFmpegInputStreamSnapshotFactory::fromFormatContext(const AVFormatContext& context)
{
    std::vector<FFmpegInputStreamSnapshot> snapshots;
    snapshots.reserve(context.nb_streams);
    for (unsigned index = 0; index < context.nb_streams; ++index) {
        const AVStream* stream = context.streams[index];
        if (!stream) {
            return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "input snapshot stream " + std::to_string(index) + " is null"));
        }
        if (!stream->codecpar) {
            return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "input snapshot stream " + std::to_string(index) +
                    " codec parameters are null"));
        }
        auto parameters = ::media::ffmpeg::makeCodecParameters();
        if (!parameters) {
            return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::failure(
                ::media::ErrorInfo::allocationFailed("input snapshot codec allocation failed"));
        }
        const int copied = avcodec_parameters_copy(parameters.get(), stream->codecpar);
        if (copied < 0) {
            return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::failure(
                FFmpegGraphError::fromCode(copied, "input snapshot codec copy"));
        }
        auto codec = FFmpegCodecParametersSnapshot::takeOwnership(std::move(parameters));
        if (!codec) {
            return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::failure(codec.error());
        }
        FFmpegInputStreamSnapshot snapshot;
        snapshot.index = static_cast<int>(index);
        snapshot.streamKind = streamKindFromCodecType(stream->codecpar->codec_type);
        snapshot.codec = std::move(codec.value());
        snapshot.format = FFmpegDescriptorMapper::fromStream(stream);
        snapshot.time = snapshotTime(context, *stream);
        snapshots.push_back(std::move(snapshot));
    }
    return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::success(std::move(snapshots));
}

} // namespace media::ffmpeg::graph
