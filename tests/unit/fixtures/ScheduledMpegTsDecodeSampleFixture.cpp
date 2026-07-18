#include "unit/fixtures/ScheduledMpegTsDecodeSampleFixture.h"

#include "unit/fixtures/MediaTsSampleStreamConfigFixture.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace media_transcode::test {
namespace {

using namespace ::media::ffmpeg::graph;

constexpr std::int64_t CaptureDurationNs = 1'500'000'000;
constexpr std::int64_t PresentationMarginNs = 250'000'000;
constexpr std::size_t MaximumDemuxPackets = 512;

struct CapturedUnit final {
    MediaScheduledStream stream;
    ::media::ffmpeg::PacketPtr packet;
    std::int64_t presentationNs;
    std::int64_t dispatchNs;
};

::media::ErrorInfo ffmpegFailure(const char* operation, int code)
{
    return ::media::ErrorInfo::ffmpegFailure(operation, code);
}

::media::Result<std::int64_t> runningNanoseconds(
    std::int64_t ticks,
    AVRational timeBase)
{
    if (ticks == AV_NOPTS_VALUE) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled MPEG-TS sample requires PTS and DTS"));
    }
    auto converted = MediaRunningTime::checkedFromTicks(
        ticks, timeBase.num, timeBase.den);
    return converted
        ? ::media::Result<std::int64_t>::success(
              converted.value().nanoseconds())
        : ::media::Result<std::int64_t>::failure(converted.error());
}

::media::Result<::media::ffmpeg::CodecContextPtr> makeCodecContext(
    const AVStream& stream,
    MediaScheduledStream kind,
    const MediaTsMuxPlan& plan)
{
    auto context = ::media::ffmpeg::makeCodecContext(nullptr);
    if (!context) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::allocationFailed(
                "scheduled MPEG-TS sample codec context"));
    }
    const int copied = avcodec_parameters_to_context(
        context.get(), stream.codecpar);
    if (copied < 0) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ffmpegFailure(
                "avcodec_parameters_to_context(scheduled MPEG-TS sample)",
                copied));
    }
    context->time_base = stream.time_base;
    if (kind == MediaScheduledStream::Audio) {
        context->frame_size = plan.parameters().maximumAudioAccessUnitSamples;
    }
    return ::media::Result<::media::ffmpeg::CodecContextPtr>::success(
        std::move(context));
}

::media::Result<CapturedUnit> capture(
    MediaScheduledStream stream,
    const AVPacket& source,
    AVRational timeBase)
{
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Result<CapturedUnit>::failure(
            ::media::ErrorInfo::allocationFailed(
                "scheduled MPEG-TS sample packet"));
    }
    const int referenced = av_packet_ref(packet.get(), &source);
    if (referenced < 0) {
        return ::media::Result<CapturedUnit>::failure(
            ffmpegFailure(
                "av_packet_ref(scheduled MPEG-TS sample)", referenced));
    }
    auto presentation = runningNanoseconds(packet->pts, timeBase);
    auto dispatch = runningNanoseconds(packet->dts, timeBase);
    if (!presentation || !dispatch) {
        return ::media::Result<CapturedUnit>::failure(
            !presentation ? presentation.error() : dispatch.error());
    }
    return ::media::Result<CapturedUnit>::success(CapturedUnit{
        stream, std::move(packet), presentation.value(), dispatch.value()});
}

bool validLengthPrefixedAccessUnit(
    const AVPacket& packet,
    std::uint8_t nalLengthBytes) noexcept
{
    if (!packet.data || packet.size <= 0 || nalLengthBytes == 0 ||
        nalLengthBytes > 4) {
        return false;
    }
    const auto bytes = std::span<const std::uint8_t>(
        packet.data, static_cast<std::size_t>(packet.size));
    std::size_t offset = 0;
    std::size_t nalCount = 0;
    while (offset < bytes.size()) {
        if (bytes.size() - offset < nalLengthBytes) return false;
        std::size_t nalSize = 0;
        for (std::size_t index = 0; index < nalLengthBytes; ++index) {
            nalSize = (nalSize << 8) | bytes[offset + index];
        }
        offset += nalLengthBytes;
        if (nalSize == 0 || nalSize > bytes.size() - offset ||
            (bytes[offset] & 0x80) != 0) {
            return false;
        }
        offset += nalSize;
        ++nalCount;
    }
    return nalCount != 0;
}

::media::Status validateMuxContract(
    const MediaTsMuxPlan& plan,
    const AVStream& video)
{
    const auto& parameters = plan.parameters();
    if (parameters.h264InputLayout !=
        MediaTsH264InputLayout::LengthPrefixed) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "scheduled MPEG-TS sample extractor requires planner-declared length-prefixed H.264"));
    }
    if (parameters.h264NalLengthBytes == 0 ||
        parameters.h264NalLengthBytes > 4 || !video.codecpar->extradata ||
        video.codecpar->extradata_size <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled MPEG-TS sample extractor received an incomplete H.264 layout contract"));
    }
    auto avcc = MediaTsSampleStreamConfigFixture::parseAvcc(
        std::span<const std::uint8_t>(
            video.codecpar->extradata,
            static_cast<std::size_t>(video.codecpar->extradata_size)));
    if (!avcc) return ::media::Status::failure(avcc.error());
    if (avcc.value().nalLengthBytes != parameters.h264NalLengthBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled MPEG-TS sample AVCC NAL length differs from the planner mux plan"));
    }
    return ::media::Status::success();
}

} // namespace

ScheduledMpegTsDecodeSampleFixture::ScheduledMpegTsDecodeSampleFixture(
    ::media::ffmpeg::CodecContextPtr videoCodec,
    ::media::ffmpeg::CodecContextPtr audioCodec,
    std::vector<ScheduledMpegTsDecodeAccessUnit> accessUnits) noexcept
    : m_videoCodec(std::move(videoCodec)),
      m_audioCodec(std::move(audioCodec)),
      m_accessUnits(std::move(accessUnits))
{
}

::media::Result<ScheduledMpegTsDecodeSampleFixture>
ScheduledMpegTsDecodeSampleFixture::load(
    const std::filesystem::path& path,
    const MediaTsMuxPlan& muxPlan)
{
    using FixtureResult =
        ::media::Result<ScheduledMpegTsDecodeSampleFixture>;
    if (!std::filesystem::exists(path)) {
        return FixtureResult::failure(::media::ErrorInfo::unsupported(
            "scheduled MPEG-TS sample is unavailable"));
    }
    AVFormatContext* raw = nullptr;
    const int opened = avformat_open_input(
        &raw, path.string().c_str(), nullptr, nullptr);
    ::media::ffmpeg::InputFormatContextPtr input(raw);
    if (opened < 0 || !input) {
        return FixtureResult::failure(ffmpegFailure(
            "avformat_open_input(scheduled MPEG-TS sample)", opened));
    }
    const int probed = avformat_find_stream_info(input.get(), nullptr);
    if (probed < 0) {
        return FixtureResult::failure(ffmpegFailure(
            "avformat_find_stream_info(scheduled MPEG-TS sample)", probed));
    }
    const int videoIndex = av_find_best_stream(
        input.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int audioIndex = av_find_best_stream(
        input.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (videoIndex < 0 || audioIndex < 0) {
        return FixtureResult::failure(::media::ErrorInfo::unsupported(
            "scheduled MPEG-TS sample needs H.264 and AAC streams"));
    }
    const AVStream& video = *input->streams[videoIndex];
    const AVStream& audio = *input->streams[audioIndex];
    if (video.codecpar->codec_id != AV_CODEC_ID_H264 ||
        audio.codecpar->codec_id != AV_CODEC_ID_AAC) {
        return FixtureResult::failure(::media::ErrorInfo::unsupported(
            "scheduled MPEG-TS sample codecs are unsupported"));
    }
    if (auto contract = validateMuxContract(muxPlan, video); !contract) {
        return FixtureResult::failure(contract.error());
    }
    auto videoCodec = makeCodecContext(
        video, MediaScheduledStream::Video, muxPlan);
    auto audioCodec = makeCodecContext(
        audio, MediaScheduledStream::Audio, muxPlan);
    if (!videoCodec || !audioCodec) {
        return FixtureResult::failure(
            !videoCodec ? videoCodec.error() : audioCodec.error());
    }

    std::vector<CapturedUnit> captured;
    captured.reserve(256);
    std::optional<std::int64_t> firstVideoKeyDispatch;
    AVPacket packet{};
    int readResult = 0;
    std::size_t demuxed = 0;
    while (demuxed < MaximumDemuxPackets &&
           (readResult = av_read_frame(input.get(), &packet)) >= 0) {
        ++demuxed;
        std::optional<CapturedUnit> unit;
        if (packet.stream_index == videoIndex && packet.size > 0) {
            auto result = capture(
                MediaScheduledStream::Video, packet, video.time_base);
            if (!result) {
                av_packet_unref(&packet);
                return FixtureResult::failure(result.error());
            }
            unit.emplace(std::move(result).value());
        } else if (packet.stream_index == audioIndex && packet.size > 0) {
            auto result = capture(
                MediaScheduledStream::Audio, packet, audio.time_base);
            if (!result) {
                av_packet_unref(&packet);
                return FixtureResult::failure(result.error());
            }
            unit.emplace(std::move(result).value());
        }
        av_packet_unref(&packet);
        if (unit) {
            if (!firstVideoKeyDispatch &&
                unit->stream == MediaScheduledStream::Video &&
                (unit->packet->flags & AV_PKT_FLAG_KEY) != 0) {
                firstVideoKeyDispatch = unit->dispatchNs;
            }
            captured.push_back(std::move(*unit));
        }
        if (firstVideoKeyDispatch && !captured.empty() &&
            captured.back().dispatchNs >=
                *firstVideoKeyDispatch + CaptureDurationNs + 250'000'000) {
            break;
        }
    }
    av_packet_unref(&packet);
    if (readResult < 0 && readResult != AVERROR_EOF) {
        return FixtureResult::failure(ffmpegFailure(
            "av_read_frame(scheduled MPEG-TS sample)", readResult));
    }
    if (!firstVideoKeyDispatch) {
        return FixtureResult::failure(::media::ErrorInfo::invalidArgument(
            "scheduled MPEG-TS sample has no H.264 keyframe"));
    }
    const std::int64_t captureEnd =
        *firstVideoKeyDispatch + CaptureDurationNs;
    std::erase_if(captured, [&](const CapturedUnit& unit) {
        return unit.dispatchNs < *firstVideoKeyDispatch ||
               unit.dispatchNs > captureEnd;
    });
    std::stable_sort(
        captured.begin(), captured.end(),
        [](const CapturedUnit& left, const CapturedUnit& right) {
            if (left.dispatchNs != right.dispatchNs) {
                return left.dispatchNs < right.dispatchNs;
            }
            return left.stream == MediaScheduledStream::Video &&
                   right.stream == MediaScheduledStream::Audio;
        });
    const auto firstVideo = std::find_if(
        captured.begin(), captured.end(), [](const CapturedUnit& unit) {
            return unit.stream == MediaScheduledStream::Video;
        });
    const bool haveAudio = std::any_of(
        captured.begin(), captured.end(), [](const CapturedUnit& unit) {
            return unit.stream == MediaScheduledStream::Audio;
        });
    if (firstVideo == captured.end() || !haveAudio ||
        (firstVideo->packet->flags & AV_PKT_FLAG_KEY) == 0 ||
        !validLengthPrefixedAccessUnit(
            *firstVideo->packet,
            muxPlan.parameters().h264NalLengthBytes)) {
        return FixtureResult::failure(::media::ErrorInfo::invalidArgument(
            "scheduled MPEG-TS sample first keyframe violates the planner-declared length-prefixed layout"));
    }

    std::int64_t minimumPresentation =
        (std::numeric_limits<std::int64_t>::max)();
    for (const auto& unit : captured) {
        minimumPresentation = (std::min)(
            minimumPresentation, unit.presentationNs);
    }
    const std::int64_t firstDispatch = captured.front().dispatchNs;
    std::vector<ScheduledMpegTsDecodeAccessUnit> accessUnits;
    accessUnits.reserve(captured.size());
    for (auto& unit : captured) {
        if (unit.dispatchNs < firstDispatch ||
            unit.presentationNs < minimumPresentation) {
            return FixtureResult::failure(::media::ErrorInfo::invalidArgument(
                "scheduled MPEG-TS sample timing is inconsistent"));
        }
        accessUnits.push_back(ScheduledMpegTsDecodeAccessUnit{
            unit.stream,
            std::move(unit.packet),
            MediaRunningTime::fromNanoseconds(
                unit.dispatchNs - firstDispatch),
            MediaRunningTime::fromNanoseconds(
                PresentationMarginNs +
                unit.presentationNs - minimumPresentation)});
    }
    return FixtureResult::success(ScheduledMpegTsDecodeSampleFixture(
        std::move(videoCodec).value(), std::move(audioCodec).value(),
        std::move(accessUnits)));
}

} // namespace media_transcode::test
