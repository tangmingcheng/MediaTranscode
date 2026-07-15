#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"
#include "fixtures/MediaTsSampleStreamConfigFixture.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

using namespace media::ffmpeg::graph;

namespace {

struct FormatCloser { void operator()(AVFormatContext* value) const { avformat_close_input(&value); } };
struct CodecCloser { void operator()(AVCodecContext* value) const { avcodec_free_context(&value); } };
struct FrameCloser { void operator()(AVFrame* value) const { av_frame_free(&value); } };
using FormatPtr = std::unique_ptr<AVFormatContext, FormatCloser>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecCloser>;
using FramePtr = std::unique_ptr<AVFrame, FrameCloser>;

class FileByteSink final : public MediaOutputByteSink {
public:
    explicit FileByteSink(const std::filesystem::path& path)
        : m_file(path, std::ios::binary | std::ios::trunc) {}
    bool open() const noexcept { return m_file.is_open(); }
    ::media::Result<std::size_t> write(std::span<const std::uint8_t> bytes) override
    {
        m_file.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        return m_file ? ::media::Result<std::size_t>::success(bytes.size())
                      : ::media::Result<std::size_t>::failure(
                            ::media::ErrorInfo::ioFailure("TS integration file write failed"));
    }
    ::media::Status flush() override
    {
        m_file.flush();
        return m_file ? ::media::Status::success()
                      : ::media::Status::failure(
                            ::media::ErrorInfo::ioFailure("TS integration file flush failed"));
    }
    ::media::Status close() override
    {
        if (m_closed) return ::media::Status::success();
        m_file.close();
        m_closed = true;
        return m_file.fail() ? ::media::Status::failure(
            ::media::ErrorInfo::ioFailure("TS integration file close failed"))
            : ::media::Status::success();
    }
private:
    std::ofstream m_file;
    bool m_closed = false;
};

struct TemporaryFile final {
    std::filesystem::path path;
    ~TemporaryFile() { std::error_code ignored; std::filesystem::remove(path, ignored); }
};

struct InputUnit final {
    std::vector<std::uint8_t> bytes;
    MediaScheduledStream stream;
    std::int64_t ptsNs;
    std::int64_t dtsNs;
    bool randomAccess;
};

int firstStreamIndex(const AVFormatContext& input, AVMediaType type)
{
    for (unsigned int index = 0; index < input.nb_streams; ++index) {
        if (input.streams[index]->codecpar->codec_type == type) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

MediaTsMuxPlan makePlan(std::uint8_t nalLengthBytes, int frequencyIndex, int channels)
{
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x100, 0x101, 0x102, 0x101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, nalLengthBytes,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, static_cast<std::uint8_t>(frequencyIndex),
                           static_cast<std::uint8_t>(channels)},
        MediaTsOutputClockPolicy{MediaRunningTime::fromNanoseconds(20'000'000),
                                 MediaRunningTime::fromNanoseconds(100'000'000),
                                 MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024}).value();
}

struct DecoderOpenResult final {
    CodecPtr context;
    bool unsupported;
};

DecoderOpenResult openDecoder(const AVStream& stream)
{
    const AVCodec* codec = avcodec_find_decoder(stream.codecpar->codec_id);
    if (!codec) return {{}, true};
    CodecPtr context(avcodec_alloc_context3(codec));
    if (!context || avcodec_parameters_to_context(context.get(), stream.codecpar) < 0 ||
        avcodec_open2(context.get(), codec, nullptr) < 0) return {{}, false};
    return {std::move(context), false};
}

bool receiveDecodedFrames(AVCodecContext& decoder,
                          AVFrame& frame,
                          bool& decoded,
                          bool flushing)
{
    while (true) {
        const int received = avcodec_receive_frame(&decoder, &frame);
        if (received == AVERROR_EOF) return true;
        if (received == AVERROR(EAGAIN)) return !flushing;
        if (received < 0) return false;
        decoded = true;
        av_frame_unref(&frame);
    }
}

bool submitPacket(AVCodecContext& decoder,
                  const AVPacket& packet,
                  AVFrame& frame,
                  bool& decoded)
{
    int sent = avcodec_send_packet(&decoder, &packet);
    if (sent == AVERROR(EAGAIN)) {
        if (!receiveDecodedFrames(decoder, frame, decoded, false)) return false;
        sent = avcodec_send_packet(&decoder, &packet);
    }
    return sent >= 0 && receiveDecodedFrames(decoder, frame, decoded, false);
}

bool flushDecoder(AVCodecContext& decoder, AVFrame& frame, bool& decoded)
{
    const int sent = avcodec_send_packet(&decoder, nullptr);
    return (sent >= 0 || sent == AVERROR_EOF) &&
           receiveDecodedFrames(decoder, frame, decoded, true);
}

bool decodeBoth(const std::filesystem::path& path, bool& decoderUnavailable)
{
    AVFormatContext* raw = nullptr;
    if (avformat_open_input(&raw, path.string().c_str(), nullptr, nullptr) < 0) return false;
    FormatPtr input(raw);
    if (avformat_find_stream_info(input.get(), nullptr) < 0) return false;
    const int videoIndex = firstStreamIndex(*input, AVMEDIA_TYPE_VIDEO);
    const int audioIndex = firstStreamIndex(*input, AVMEDIA_TYPE_AUDIO);
    if (videoIndex < 0 || audioIndex < 0) return false;
    auto videoOpen = openDecoder(*input->streams[videoIndex]);
    auto audioOpen = openDecoder(*input->streams[audioIndex]);
    if (videoOpen.unsupported || audioOpen.unsupported) {
        decoderUnavailable = true;
        return false;
    }
    if (!videoOpen.context || !audioOpen.context) return false;
    auto video = std::move(videoOpen.context);
    auto audio = std::move(audioOpen.context);
    FramePtr frame(av_frame_alloc());
    if (!frame) return false;
    bool videoFrame = false;
    bool audioFrame = false;
    AVPacket packet{};
    int readResult = 0;
    while ((readResult = av_read_frame(input.get(), &packet)) >= 0) {
        AVCodecContext* decoder = packet.stream_index == videoIndex ? video.get()
            : packet.stream_index == audioIndex ? audio.get() : nullptr;
        if (decoder) {
            bool& decoded = packet.stream_index == videoIndex ? videoFrame : audioFrame;
            if (!submitPacket(*decoder, packet, *frame, decoded)) {
                av_packet_unref(&packet);
                return false;
            }
        }
        av_packet_unref(&packet);
    }
    if (readResult != AVERROR_EOF) return false;
    if (!flushDecoder(*video, *frame, videoFrame) ||
        !flushDecoder(*audio, *frame, audioFrame)) return false;
    return videoFrame && audioFrame;
}

} // namespace

int main()
{
    const std::filesystem::path sample =
        std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
        "tests/samples/sample_h264_aac_2560x1440.mp4";
    if (!std::filesystem::exists(sample)) return 77;
    AVFormatContext* raw = nullptr;
    if (avformat_open_input(&raw, sample.string().c_str(), nullptr, nullptr) < 0) return 1;
    FormatPtr input(raw);
    if (avformat_find_stream_info(input.get(), nullptr) < 0) return 1;
    const int videoIndex = firstStreamIndex(*input, AVMEDIA_TYPE_VIDEO);
    const int audioIndex = firstStreamIndex(*input, AVMEDIA_TYPE_AUDIO);
    if (videoIndex < 0 || audioIndex < 0) return 1;
    const auto& videoParameters = *input->streams[videoIndex]->codecpar;
    const auto& audioParameters = *input->streams[audioIndex]->codecpar;
    if (videoParameters.codec_id != AV_CODEC_ID_H264 ||
        audioParameters.codec_id != AV_CODEC_ID_AAC) return 1;
    auto avccConfig = media_transcode::test::MediaTsSampleStreamConfigFixture::parseAvcc(
        std::span<const std::uint8_t>(
            videoParameters.extradata,
            static_cast<std::size_t>(videoParameters.extradata_size)));
    auto frequencyIndex =
        media_transcode::test::MediaTsSampleStreamConfigFixture::aacSamplingFrequencyIndex(
            audioParameters.sample_rate);
#if LIBAVUTIL_VERSION_MAJOR >= 57
    const int channels = audioParameters.ch_layout.nb_channels;
#else
    const int channels = audioParameters.channels;
#endif
    if (!avccConfig || !frequencyIndex || channels < 1 || channels > 7) return 1;

    std::vector<InputUnit> units;
    AVPacket packet{};
    int readResult = 0;
    while (units.size() < 240 &&
           (readResult = av_read_frame(input.get(), &packet)) >= 0) {
        if ((packet.stream_index == videoIndex || packet.stream_index == audioIndex) &&
            packet.pts != AV_NOPTS_VALUE && packet.dts != AV_NOPTS_VALUE && packet.size > 0) {
            const auto timeBase = input->streams[packet.stream_index]->time_base;
            auto pts = MediaRunningTime::checkedFromTicks(
                packet.pts, timeBase.num, timeBase.den);
            auto dts = MediaRunningTime::checkedFromTicks(
                packet.dts, timeBase.num, timeBase.den);
            if (!pts || !dts) {
                av_packet_unref(&packet);
                return 1;
            }
            units.push_back(InputUnit{
                std::vector<std::uint8_t>(packet.data, packet.data + packet.size),
                packet.stream_index == videoIndex ? MediaScheduledStream::Video
                                                  : MediaScheduledStream::Audio,
                pts.value().nanoseconds(), dts.value().nanoseconds(),
                packet.stream_index == videoIndex && (packet.flags & AV_PKT_FLAG_KEY) != 0});
        }
        av_packet_unref(&packet);
    }
    if (units.size() < 240 && readResult != AVERROR_EOF) return 1;
    if (units.empty() ||
        std::none_of(units.begin(), units.end(), [](const auto& unit) {
            return unit.stream == MediaScheduledStream::Video; }) ||
        std::none_of(units.begin(), units.end(), [](const auto& unit) {
            return unit.stream == MediaScheduledStream::Audio; })) return 1;
    const auto firstKeyframe = std::find_if(
        units.begin(), units.end(), [](const auto& unit) {
            return unit.stream == MediaScheduledStream::Video && unit.randomAccess;
        });
    if (firstKeyframe == units.end()) return 1;
    const std::int64_t keyframeDts = firstKeyframe->dtsNs;
    std::erase_if(units, [keyframeDts](const auto& unit) {
        return unit.dtsNs < keyframeDts;
    });
    if (std::none_of(units.begin(), units.end(), [](const auto& unit) {
            return unit.stream == MediaScheduledStream::Audio;
        })) return 1;
    std::stable_sort(units.begin(), units.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.dtsNs < rhs.dtsNs;
    });
    const std::int64_t firstDts = units.front().dtsNs;
    TemporaryFile output{
        std::filesystem::temp_directory_path() /
        ("media_transcode_project_ts_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".ts")};
    auto sink = std::make_unique<FileByteSink>(output.path);
    if (!sink->open()) return 1;
    auto sampleVideoConfig = std::move(avccConfig).value();
    auto muxPlan = makePlan(
        sampleVideoConfig.nalLengthBytes, frequencyIndex.value(), channels);
    auto videoConfig = MediaTsMaterializedVideoConfig::create(
        MediaTsH264InputLayout::LengthPrefixed,
        sampleVideoConfig.nalLengthBytes,
        std::move(sampleVideoConfig.sequenceParameterSet),
        std::move(sampleVideoConfig.pictureParameterSet));
    auto audioConfig = MediaTsMaterializedAudioConfig::create(
        2, frequencyIndex.value(),
        static_cast<std::uint8_t>(channels));
    if (!videoConfig || !audioConfig) return 1;
    auto session = MediaTsMuxSession::create(MediaTsMuxSession::Binding{
        muxPlan,
        MediaPlaybackEpoch{MediaRunningTime::fromNanoseconds(0),
                           MediaRunningTime::fromNanoseconds(0), 1},
        std::move(videoConfig).value(),
        std::move(audioConfig).value(),
        std::move(sink)});
    if (!session || !session.value()->start(MediaRunningTime::fromNanoseconds(0))) return 1;
    for (const auto& unit : units) {
        auto dispatchDelta = MediaRunningTime::fromNanoseconds(unit.dtsNs).checkedSubtract(
            MediaRunningTime::fromNanoseconds(firstDts));
        auto presentationDelta = MediaRunningTime::fromNanoseconds(unit.ptsNs).checkedSubtract(
            MediaRunningTime::fromNanoseconds(firstDts));
        if (!dispatchDelta || !presentationDelta) return 1;
        auto dispatch = dispatchDelta.value().checkedAdd(
            MediaRunningTime::fromNanoseconds(200'000'000));
        auto presentation = presentationDelta.value().checkedAdd(
            MediaRunningTime::fromNanoseconds(200'000'000));
        auto emission = dispatchDelta.value().checkedAdd(
            MediaRunningTime::fromNanoseconds(100'000'000));
        if (!dispatch || !presentation || !emission) return 1;
        if (!session.value()->writeAccessUnit(MediaTsAccessUnitView{
                unit.bytes, unit.stream, 1, presentation.value(), dispatch.value(),
                emission.value(), unit.randomAccess})) return 1;
    }
    if (!session.value()->finish()) return 1;
    bool decoderUnavailable = false;
    const bool decoded = decodeBoth(output.path, decoderUnavailable);
    if (decoderUnavailable) return 77;
    if (!decoded) {
        std::cerr << "project-owned MPEG-TS decode probe did not decode both streams\n";
        return 1;
    }
    return 0;
}
