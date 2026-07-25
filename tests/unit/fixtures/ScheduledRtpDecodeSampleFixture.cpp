#include "unit/fixtures/ScheduledRtpDecodeSampleFixture.h"

#include "unit/fixtures/MediaTsSampleStreamConfigFixture.h"

extern "C" {
#include <libavcodec/bsf.h>
#include <libavcodec/codec_par.h>
#include <libavutil/error.h>
}

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace media_transcode::test {
namespace {

constexpr std::int64_t kCaptureDurationNs = 1'500'000'000;
constexpr std::int64_t kPresentationMarginNs = 250'000'000;
constexpr std::size_t kMaximumDemuxPackets = 512;

struct BsfDeleter final {
    void operator()(AVBSFContext* context) const noexcept
    {
        if (context) av_bsf_free(&context);
    }
};

using BsfPtr = std::unique_ptr<AVBSFContext, BsfDeleter>;

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
                "scheduled RTP decode sample requires PTS and DTS"));
    }
    auto converted = MediaRunningTime::checkedFromTicks(
        ticks, timeBase.num, timeBase.den);
    if (!converted) {
        return ::media::Result<std::int64_t>::failure(converted.error());
    }
    return ::media::Result<std::int64_t>::success(
        converted.value().nanoseconds());
}

::media::Result<::media::ffmpeg::CodecContextPtr> makeCodecContext(
    const AVStream& stream,
    const MediaScheduledRtpPacketizationPlan& packetization)
{
    auto context = ::media::ffmpeg::makeCodecContext(nullptr);
    if (!context) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ::media::ErrorInfo::allocationFailed(
                "scheduled RTP decode codec context"));
    }
    const int copied = avcodec_parameters_to_context(
        context.get(), stream.codecpar);
    if (copied < 0) {
        return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
            ffmpegFailure(
                "avcodec_parameters_to_context(scheduled RTP decode)",
                copied));
    }
    context->time_base = AVRational{
        packetization.streamTimeBaseNumerator(),
        packetization.streamTimeBaseDenominator()};
    if (packetization.streamKind() == MediaStreamKind::Audio) {
        if (!packetization.maximumAccessUnitSamples()) {
            return ::media::Result<::media::ffmpeg::CodecContextPtr>::failure(
                ::media::ErrorInfo::notInitialized(
                    "scheduled RTP audio decode fixture needs planned AU samples"));
        }
        context->frame_size = *packetization.maximumAccessUnitSamples();
    }
    return ::media::Result<::media::ffmpeg::CodecContextPtr>::success(
        std::move(context));
}

::media::Result<BsfPtr> makeH264AnnexBFilter(const AVStream& stream)
{
    const AVBitStreamFilter* filter = av_bsf_get_by_name(
        "h264_mp4toannexb");
    if (!filter) {
        return ::media::Result<BsfPtr>::failure(
            ::media::ErrorInfo::unsupported(
                "FFmpeg h264_mp4toannexb filter is unavailable"));
    }
    AVBSFContext* raw = nullptr;
    const int allocated = av_bsf_alloc(filter, &raw);
    BsfPtr context(raw);
    if (allocated < 0 || !context) {
        return ::media::Result<BsfPtr>::failure(
            allocated < 0
                ? ffmpegFailure(
                      "av_bsf_alloc(h264_mp4toannexb)", allocated)
                : ::media::ErrorInfo::allocationFailed(
                      "h264_mp4toannexb context"));
    }
    const int copied = avcodec_parameters_copy(
        context->par_in, stream.codecpar);
    if (copied < 0) {
        return ::media::Result<BsfPtr>::failure(
            ffmpegFailure(
                "avcodec_parameters_copy(h264_mp4toannexb)", copied));
    }
    context->time_base_in = stream.time_base;
    const int initialized = av_bsf_init(context.get());
    if (initialized < 0) {
        return ::media::Result<BsfPtr>::failure(
            ffmpegFailure(
                "av_bsf_init(h264_mp4toannexb)", initialized));
    }
    return ::media::Result<BsfPtr>::success(std::move(context));
}

::media::Result<CapturedUnit> capture(
    MediaScheduledStream stream,
    ::media::ffmpeg::PacketPtr packet,
    AVRational timeBase)
{
    auto presentation = runningNanoseconds(packet->pts, timeBase);
    auto dispatch = runningNanoseconds(packet->dts, timeBase);
    if (!presentation) {
        return ::media::Result<CapturedUnit>::failure(presentation.error());
    }
    if (!dispatch) {
        return ::media::Result<CapturedUnit>::failure(dispatch.error());
    }
    return ::media::Result<CapturedUnit>::success(CapturedUnit{
        stream, std::move(packet), presentation.value(), dispatch.value()});
}

::media::Status appendFilteredVideo(
    AVBSFContext& filter,
    const AVPacket& source,
    AVRational timeBase,
    std::vector<CapturedUnit>& output)
{
    auto submitted = ::media::ffmpeg::makePacket();
    if (!submitted) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "scheduled RTP decode video packet"));
    }
    const int referenced = av_packet_ref(submitted.get(), &source);
    if (referenced < 0) {
        return ::media::Status::failure(
            ffmpegFailure(
                "av_packet_ref(scheduled RTP decode video)", referenced));
    }
    const int sent = av_bsf_send_packet(&filter, submitted.get());
    if (sent < 0) {
        return ::media::Status::failure(
            ffmpegFailure(
                "av_bsf_send_packet(h264_mp4toannexb)", sent));
    }
    for (;;) {
        auto filtered = ::media::ffmpeg::makePacket();
        if (!filtered) {
            return ::media::Status::failure(
                ::media::ErrorInfo::allocationFailed(
                    "scheduled RTP decode filtered video packet"));
        }
        const int received = av_bsf_receive_packet(&filter, filtered.get());
        if (received == AVERROR(EAGAIN) || received == AVERROR_EOF) {
            return ::media::Status::success();
        }
        if (received < 0) {
            return ::media::Status::failure(
                ffmpegFailure(
                    "av_bsf_receive_packet(h264_mp4toannexb)", received));
        }
        auto unit = capture(
            MediaScheduledStream::Video, std::move(filtered), timeBase);
        if (!unit) return ::media::Status::failure(unit.error());
        output.push_back(std::move(unit).value());
    }
}

::media::Status appendAudio(
    const AVPacket& source,
    AVRational timeBase,
    std::vector<CapturedUnit>& output)
{
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "scheduled RTP decode audio packet"));
    }
    const int referenced = av_packet_ref(packet.get(), &source);
    if (referenced < 0) {
        return ::media::Status::failure(
            ffmpegFailure(
                "av_packet_ref(scheduled RTP decode audio)", referenced));
    }
    auto unit = capture(
        MediaScheduledStream::Audio, std::move(packet), timeBase);
    if (!unit) return ::media::Status::failure(unit.error());
    output.push_back(std::move(unit).value());
    return ::media::Status::success();
}

bool beginsWithAnnexB(const AVPacket& packet) noexcept
{
    if (!packet.data || packet.size < 4) return false;
    return (packet.data[0] == 0 && packet.data[1] == 0 &&
            packet.data[2] == 1) ||
           (packet.size >= 5 && packet.data[0] == 0 &&
            packet.data[1] == 0 && packet.data[2] == 0 &&
            packet.data[3] == 1);
}

} // namespace

ScheduledRtpDecodeSampleFixture::ScheduledRtpDecodeSampleFixture(
    ::media::ffmpeg::CodecContextPtr videoCodec,
    ::media::ffmpeg::CodecContextPtr audioCodec,
    std::vector<ScheduledRtpDecodeAccessUnit> accessUnits) noexcept
    : m_videoCodec(std::move(videoCodec)),
      m_audioCodec(std::move(audioCodec)),
      m_accessUnits(std::move(accessUnits))
{
}

::media::Result<ScheduledRtpDecodeSampleFixture>
ScheduledRtpDecodeSampleFixture::load(
    const std::filesystem::path& path,
    const MediaScheduledRtpPacketizationPlan& videoPacketization,
    const MediaScheduledRtpPacketizationPlan& audioPacketization)
{
    using FixtureResult =
        ::media::Result<ScheduledRtpDecodeSampleFixture>;
    if (!std::filesystem::exists(path)) {
        return FixtureResult::failure(
            ::media::ErrorInfo::unsupported(
                "scheduled RTP decode sample is unavailable"));
    }
    AVFormatContext* raw = nullptr;
    const int opened = avformat_open_input(
        &raw, path.string().c_str(), nullptr, nullptr);
    ::media::ffmpeg::InputFormatContextPtr input(raw);
    if (opened < 0 || !input) {
        return FixtureResult::failure(
            ffmpegFailure(
                "avformat_open_input(scheduled RTP decode sample)", opened));
    }
    const int probed = avformat_find_stream_info(input.get(), nullptr);
    if (probed < 0) {
        return FixtureResult::failure(
            ffmpegFailure(
                "avformat_find_stream_info(scheduled RTP decode sample)",
                probed));
    }
    const int videoIndex = av_find_best_stream(
        input.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int audioIndex = av_find_best_stream(
        input.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (videoIndex < 0 || audioIndex < 0) {
        return FixtureResult::failure(
            ::media::ErrorInfo::unsupported(
                "scheduled RTP decode sample needs H264 and AAC streams"));
    }
    const AVStream& video = *input->streams[videoIndex];
    const AVStream& audio = *input->streams[audioIndex];
    if (video.codecpar->codec_id != AV_CODEC_ID_H264 ||
        audio.codecpar->codec_id != AV_CODEC_ID_AAC ||
        !video.codecpar->extradata || video.codecpar->extradata_size <= 0) {
        return FixtureResult::failure(
            ::media::ErrorInfo::unsupported(
                "scheduled RTP decode sample codecs are unsupported"));
    }
    auto avcc = MediaTsSampleStreamConfigFixture::parseAvcc(
        std::span<const std::uint8_t>(
            video.codecpar->extradata,
            static_cast<std::size_t>(video.codecpar->extradata_size)));
    if (!avcc) return FixtureResult::failure(avcc.error());
    auto videoCodec = makeCodecContext(video, videoPacketization);
    auto audioCodec = makeCodecContext(audio, audioPacketization);
    auto annexB = makeH264AnnexBFilter(video);
    if (!videoCodec) return FixtureResult::failure(videoCodec.error());
    if (!audioCodec) return FixtureResult::failure(audioCodec.error());
    if (!annexB) return FixtureResult::failure(annexB.error());

    std::vector<CapturedUnit> captured;
    captured.reserve(256);
    std::optional<std::int64_t> firstVideoKeyDispatch;
    AVPacket packet{};
    int readResult = 0;
    std::size_t demuxed = 0;
    while (demuxed < kMaximumDemuxPackets &&
           (readResult = av_read_frame(input.get(), &packet)) >= 0) {
        ++demuxed;
        ::media::Status appended = ::media::Status::success();
        if (packet.stream_index == videoIndex && packet.size > 0) {
            appended = appendFilteredVideo(
                *annexB.value(), packet, video.time_base, captured);
        } else if (packet.stream_index == audioIndex && packet.size > 0) {
            appended = appendAudio(packet, audio.time_base, captured);
        }
        av_packet_unref(&packet);
        if (!appended) return FixtureResult::failure(appended.error());
        for (const auto& unit : captured) {
            if (unit.stream == MediaScheduledStream::Video &&
                (unit.packet->flags & AV_PKT_FLAG_KEY) != 0) {
                firstVideoKeyDispatch = unit.dispatchNs;
                break;
            }
        }
        if (firstVideoKeyDispatch && !captured.empty() &&
            captured.back().dispatchNs >=
                *firstVideoKeyDispatch + kCaptureDurationNs + 250'000'000) {
            break;
        }
    }
    av_packet_unref(&packet);
    if (readResult < 0 && readResult != AVERROR_EOF) {
        return FixtureResult::failure(
            ffmpegFailure(
                "av_read_frame(scheduled RTP decode sample)", readResult));
    }
    if (!firstVideoKeyDispatch) {
        return FixtureResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP decode sample has no H264 keyframe"));
    }
    const std::int64_t captureEnd =
        *firstVideoKeyDispatch + kCaptureDurationNs;
    std::erase_if(captured, [&](const CapturedUnit& unit) {
        return unit.dispatchNs < *firstVideoKeyDispatch ||
               unit.dispatchNs > captureEnd;
    });
    if (captured.empty()) {
        return FixtureResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP decode sample produced no access units"));
    }
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
        !beginsWithAnnexB(*firstVideo->packet)) {
        return FixtureResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP decode sample is not decodable from its first AU"));
    }
    std::int64_t minimumPresentation =
        (std::numeric_limits<std::int64_t>::max)();
    for (const auto& unit : captured) {
        minimumPresentation = (std::min)(
            minimumPresentation, unit.presentationNs);
    }
    const std::int64_t firstDispatch = captured.front().dispatchNs;
    std::vector<ScheduledRtpDecodeAccessUnit> accessUnits;
    accessUnits.reserve(captured.size());
    for (auto& unit : captured) {
        if (unit.dispatchNs < firstDispatch ||
            unit.presentationNs < minimumPresentation) {
            return FixtureResult::failure(
                ::media::ErrorInfo::invalidArgument(
                    "scheduled RTP decode sample timing is inconsistent"));
        }
        accessUnits.push_back(ScheduledRtpDecodeAccessUnit{
            unit.stream,
            std::move(unit.packet),
            MediaRunningTime::fromNanoseconds(
                unit.dispatchNs - firstDispatch),
            MediaRunningTime::fromNanoseconds(
                kPresentationMarginNs +
                unit.presentationNs - minimumPresentation)});
    }
    return FixtureResult::success(ScheduledRtpDecodeSampleFixture(
        std::move(videoCodec).value(), std::move(audioCodec).value(),
        std::move(accessUnits)));
}

} // namespace media_transcode::test
