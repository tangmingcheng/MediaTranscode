#include "unit/fixtures/MpegTsOutputArtifactVerifier.h"

#include "unit/fixtures/MediaTsPesTimestampInspector.h"

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace media_transcode::test {
namespace {

using namespace ::media::ffmpeg::graph;

struct FormatCloser final {
    void operator()(AVFormatContext* value) const noexcept
    {
        avformat_close_input(&value);
    }
};
struct CodecCloser final {
    void operator()(AVCodecContext* value) const noexcept
    {
        avcodec_free_context(&value);
    }
};
struct FrameCloser final {
    void operator()(AVFrame* value) const noexcept { av_frame_free(&value); }
};
using FormatPtr = std::unique_ptr<AVFormatContext, FormatCloser>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecCloser>;
using FramePtr = std::unique_ptr<AVFrame, FrameCloser>;

::media::ErrorInfo invalid(std::string message);

class InventorySink final : public MediaTsProgramInventorySink {
public:
    ::media::Status onProgramInventory(
        MediaTsProgramInventorySnapshot value) override
    {
        snapshot = std::move(value);
        return ::media::Status::success();
    }

    std::optional<MediaTsProgramInventorySnapshot> snapshot;
};

class FirstAudioAdtsSink final : public MediaTsPacketSink {
public:
    explicit FirstAudioAdtsSink(std::uint16_t audioPid) noexcept
        : m_audioPid(audioPid)
    {
    }

    ::media::Status onPacket(const MediaTsPacketView& packet) override
    {
        if (m_header.size() >= 9 || packet.pid != m_audioPid) {
            return ::media::Status::success();
        }
        std::span<const std::uint8_t> payload = packet.payloadSpan;
        if (packet.payloadUnitStart) {
            m_collecting = false;
            m_header.clear();
            if (payload.size() < 9 || payload[0] != 0 || payload[1] != 0 ||
                payload[2] != 1) {
                return ::media::Status::failure(invalid(
                    "scheduled TS audio PID has an invalid PES header"));
            }
            const std::size_t payloadOffset = 9 + payload[8];
            if (payloadOffset > payload.size()) {
                return ::media::Status::failure(invalid(
                    "scheduled TS audio PES header is truncated"));
            }
            payload = payload.subspan(payloadOffset);
            m_collecting = true;
        }
        if (!m_collecting) return ::media::Status::success();
        const std::size_t needed = 9 - m_header.size();
        const std::size_t copied = (std::min)(needed, payload.size());
        m_header.insert(
            m_header.end(), payload.begin(), payload.begin() + copied);
        return ::media::Status::success();
    }

    const std::vector<std::uint8_t>& header() const noexcept
    {
        return m_header;
    }

private:
    std::uint16_t m_audioPid;
    bool m_collecting = false;
    std::vector<std::uint8_t> m_header;
};

::media::ErrorInfo invalid(std::string message)
{
    return ::media::ErrorInfo::invalidArgument(std::move(message));
}

::media::Result<std::vector<std::uint8_t>> readFile(
    const std::filesystem::path& path)
{
    std::error_code sizeError;
    const std::uintmax_t fileBytes = std::filesystem::file_size(
        path, sizeError);
    if (sizeError || fileBytes == 0 ||
        fileBytes > static_cast<std::uintmax_t>(
            (std::numeric_limits<std::streamsize>::max)()) ||
        fileBytes > static_cast<std::uintmax_t>(
            (std::numeric_limits<std::size_t>::max)())) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            ::media::ErrorInfo::ioFailure(
                "scheduled TS output file size is unavailable or invalid"));
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            ::media::ErrorInfo::ioFailure("scheduled TS output file is unavailable"));
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileBytes));
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (input.bad() || input.gcount() !=
            static_cast<std::streamsize>(bytes.size())) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            ::media::ErrorInfo::ioFailure(
                "scheduled TS output file could not be read completely"));
    }
    return ::media::Result<std::vector<std::uint8_t>>::success(std::move(bytes));
}

::media::Status verifyAdtsConfiguration(
    std::span<const std::uint8_t> bytes,
    const MediaTsMuxPlan& plan)
{
    const auto& expected = plan.parameters();
    FirstAudioAdtsSink sink(expected.audioPid);
    auto parser = MediaTsPacketParser::create(
        expected.packetSize, sink, nullptr);
    if (!parser) return ::media::Status::failure(parser.error());
    if (auto parsed = parser.value()->push(bytes); !parsed) return parsed;
    if (auto finished = parser.value()->finish(); !finished) return finished;
    const auto& header = sink.header();
    if (header.size() < 7 || (header[0] != 0xFF) ||
        (header[1] & 0xF6) != 0xF0) {
        return ::media::Status::failure(invalid(
            "scheduled TS audio PES does not begin with a valid ADTS sync/layer"));
    }
    const std::uint8_t mpegId = (header[1] >> 3) & 1;
    const std::uint8_t audioObjectType =
        static_cast<std::uint8_t>(((header[2] >> 6) & 3) + 1);
    const std::uint8_t samplingFrequencyIndex = (header[2] >> 2) & 0x0F;
    const std::uint8_t channelConfiguration = static_cast<std::uint8_t>(
        ((header[2] & 1) << 2) | ((header[3] >> 6) & 3));
    const std::size_t frameLength =
        (static_cast<std::size_t>(header[3] & 3) << 11) |
        (static_cast<std::size_t>(header[4]) << 3) |
        (header[5] >> 5);
    const std::size_t headerLength = (header[1] & 1) != 0 ? 7 : 9;
    const auto& aac = expected.aac;
    if (mpegId != aac.mpegId || audioObjectType != aac.audioObjectType ||
        samplingFrequencyIndex != aac.samplingFrequencyIndex ||
        channelConfiguration != aac.channelConfiguration ||
        frameLength <= headerLength) {
        return ::media::Status::failure(invalid(
            "scheduled TS ADTS configuration does not match its planner mux plan"));
    }
    return ::media::Status::success();
}

::media::Status verifyTransport(
    std::span<const std::uint8_t> bytes,
    const MediaTsMuxPlan& plan)
{
    const auto& expected = plan.parameters();
    if (bytes.size() % expected.packetSize != 0) {
        return ::media::Status::failure(invalid(
            "scheduled TS output is not packet aligned"));
    }
    MediaTsPesTimestampInspector inspector;
    auto parser = MediaTsPacketParser::create(
        expected.packetSize, inspector, nullptr);
    if (!parser) return ::media::Status::failure(parser.error());
    if (auto parsed = parser.value()->push(bytes); !parsed) return parsed;
    if (auto finished = parser.value()->finish(); !finished) return finished;
    if (inspector.continuityEventCount() != 0) {
        return ::media::Status::failure(invalid(
            "scheduled TS output has a continuity discontinuity"));
    }
    if (inspector.pcrValues().size() < 2 ||
        !std::is_sorted(inspector.pcrValues().begin(),
                        inspector.pcrValues().end())) {
        return ::media::Status::failure(invalid(
            "scheduled TS output has no monotonic PCR cadence"));
    }
    const std::uint64_t maximumPcrGap = static_cast<std::uint64_t>(
        expected.clock.pcrInterval.nanoseconds()) * 27'000'000ULL /
        1'000'000'000ULL;
    for (std::size_t index = 1; index < inspector.pcrValues().size(); ++index) {
        const std::uint64_t gap = inspector.pcrValues()[index] -
            inspector.pcrValues()[index - 1];
        if (gap == 0 || gap > maximumPcrGap) {
            return ::media::Status::failure(invalid(
                "scheduled TS output violates planned PCR cadence"));
        }
    }

    InventorySink inventory;
    MediaTsPsiSectionAssembler assembler(inventory);
    auto psi = MediaTsPacketParser::create(expected.packetSize, assembler, nullptr);
    if (!psi) return ::media::Status::failure(psi.error());
    if (auto parsed = psi.value()->push(bytes); !parsed) return parsed;
    if (auto finished = psi.value()->finish(); !finished) return finished;
    if (!inventory.snapshot || inventory.snapshot->programs.size() != 1) {
        return ::media::Status::failure(invalid(
            "scheduled TS output has no unique PAT/PMT program"));
    }
    const auto& program = inventory.snapshot->programs.front();
    if (program.programNumber != expected.programNumber ||
        program.pmtPid != expected.programMapPid ||
        program.pcrPid != expected.pcrPid ||
        program.elementaryStreams.size() != 2) {
        return ::media::Status::failure(invalid(
            "scheduled TS PAT/PMT identity does not match its plan"));
    }
    const auto hasElementaryStream = [&](std::uint16_t pid,
                                         std::uint8_t streamType) {
        return std::any_of(
            program.elementaryStreams.begin(), program.elementaryStreams.end(),
            [&](const MediaTsElementaryStreamInfo& stream) {
                return stream.pid == pid && stream.streamType == streamType;
            });
    };
    if (!hasElementaryStream(expected.videoPid, expected.videoStreamType) ||
        !hasElementaryStream(expected.audioPid, expected.audioStreamType)) {
        return ::media::Status::failure(invalid(
            "scheduled TS PMT does not contain planned video/audio PIDs"));
    }
    return verifyAdtsConfiguration(bytes, plan);
}

::media::Status verifyProductionTimestamps(
    std::span<const std::uint8_t> bytes,
    const MediaTsMuxPlan& plan,
    const MediaAvSyncPlan& avSyncPlan)
{
    MediaTsPesTimestampInspector inspector;
    auto parser = MediaTsPacketParser::create(
        plan.parameters().packetSize, inspector, nullptr);
    if (!parser) return ::media::Status::failure(parser.error());
    if (auto parsed = parser.value()->push(bytes); !parsed) return parsed;
    if (auto finished = parser.value()->finish(); !finished) return finished;
    const auto& timestamps = inspector.timestamps();
    const auto collectPid = [&](std::uint16_t pid, const char* stream) {
        std::vector<std::uint64_t> presentationTimes;
        std::uint64_t previousDts = 0;
        for (const auto& observed : timestamps) {
            if (observed.pid != pid) continue;
            if (!presentationTimes.empty() && observed.dts < previousDts) {
                return ::media::Result<std::vector<std::uint64_t>>::failure(
                    invalid(
                    std::string("production TS ") + stream +
                    " DTS regressed"));
            }
            presentationTimes.push_back(observed.pts);
            previousDts = observed.dts;
        }
        if (presentationTimes.size() < 2) {
            return ::media::Result<std::vector<std::uint64_t>>::failure(
                invalid(
                  std::string("production TS has no ") + stream +
                  " PES timeline"));
        }
        return ::media::Result<std::vector<std::uint64_t>>::success(
            std::move(presentationTimes));
    };
    auto video = collectPid(plan.parameters().videoPid, "video");
    auto audio = collectPid(plan.parameters().audioPid, "audio");
    if (!video || !audio) {
        return ::media::Status::failure(
            !video ? video.error() : audio.error());
    }
    if (!avSyncPlan.metrics.maximumStartupSkewNs ||
        !avSyncPlan.metrics.maximumSteadyP99SkewNs ||
        avSyncPlan.metrics.maximumStartupSkewNs->nanoseconds() <= 0 ||
        avSyncPlan.metrics.maximumSteadyP99SkewNs->nanoseconds() <= 0) {
        return ::media::Status::failure(invalid(
            "production TS A/V metric thresholds are incomplete"));
    }
    constexpr std::uint64_t PtsModulus = std::uint64_t{1} << 33;
    const auto ptsDistance = [](std::uint64_t left, std::uint64_t right) {
        const std::uint64_t linear =
            left > right ? left - right : right - left;
        return (std::min)(linear, PtsModulus - linear);
    };
    const auto durationToPts = [](MediaRunningTime duration) {
        return static_cast<std::uint64_t>(duration.nanoseconds()) * 90'000ULL /
            1'000'000'000ULL;
    };
    if (ptsDistance(video.value().front(), audio.value().front()) >
        durationToPts(*avSyncPlan.metrics.maximumStartupSkewNs)) {
        return ::media::Status::failure(invalid(
            "production TS decoded A/V startup skew exceeds plan"));
    }
    const std::uint64_t maximumSteadySkew =
        durationToPts(*avSyncPlan.metrics.maximumSteadyP99SkewNs);
    std::vector<std::uint64_t> steadySkews;
    for (const auto videoPts : video.value()) {
        if (videoPts < audio.value().front() ||
            videoPts > audio.value().back()) {
            continue;
        }
        std::uint64_t nearestAudio = PtsModulus;
        for (const auto audioPts : audio.value()) {
            nearestAudio = (std::min)(
                nearestAudio, ptsDistance(videoPts, audioPts));
        }
        steadySkews.push_back(nearestAudio);
    }
    if (steadySkews.size() < 2) {
        return ::media::Status::failure(invalid(
            "production TS has insufficient overlapping A/V timestamps"));
    }
    std::sort(steadySkews.begin(), steadySkews.end());
    const std::size_t percentileIndex =
        (steadySkews.size() * 99 + 99) / 100 - 1;
    if (steadySkews[percentileIndex] > maximumSteadySkew) {
        return ::media::Status::failure(invalid(
            "production TS decoded A/V steady P99 skew exceeds plan"));
    }
    return ::media::Status::success();
}

int firstStreamIndex(const AVFormatContext& input, AVMediaType type)
{
    for (unsigned int index = 0; index < input.nb_streams; ++index) {
        if (input.streams[index]->codecpar->codec_type == type) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

::media::Result<CodecPtr> openDecoder(const AVStream& stream)
{
    const AVCodec* codec = avcodec_find_decoder(stream.codecpar->codec_id);
    if (!codec) {
        return ::media::Result<CodecPtr>::failure(
            invalid("scheduled TS decoder is unavailable after preflight"));
    }
    CodecPtr context(avcodec_alloc_context3(codec));
    if (!context) {
        return ::media::Result<CodecPtr>::failure(
            ::media::ErrorInfo::allocationFailed("scheduled TS decoder"));
    }
    const int copied = avcodec_parameters_to_context(
        context.get(), stream.codecpar);
    const int opened = copied < 0 ? copied
        : avcodec_open2(context.get(), codec, nullptr);
    if (opened < 0) {
        return ::media::Result<CodecPtr>::failure(
            ::media::ErrorInfo::ffmpegFailure(
                "avcodec_open2(scheduled TS decode)", opened));
    }
    return ::media::Result<CodecPtr>::success(std::move(context));
}

bool receiveFrames(AVCodecContext& decoder,
                   AVFrame& frame,
                   bool& decoded,
                   bool flushing)
{
    for (;;) {
        const int received = avcodec_receive_frame(&decoder, &frame);
        if (received == AVERROR_EOF) return true;
        if (received == AVERROR(EAGAIN)) return !flushing;
        if (received < 0) return false;
        decoded = true;
        av_frame_unref(&frame);
    }
}

::media::Status decodeBoth(const std::filesystem::path& path)
{
    AVFormatContext* raw = nullptr;
    const int opened = avformat_open_input(
        &raw, path.string().c_str(), nullptr, nullptr);
    FormatPtr input(raw);
    if (opened < 0 || !input) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ffmpegFailure(
                "avformat_open_input(scheduled TS output)", opened));
    }
    const int probed = avformat_find_stream_info(input.get(), nullptr);
    if (probed < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ffmpegFailure(
                "avformat_find_stream_info(scheduled TS output)", probed));
    }
    const int videoIndex = firstStreamIndex(*input, AVMEDIA_TYPE_VIDEO);
    const int audioIndex = firstStreamIndex(*input, AVMEDIA_TYPE_AUDIO);
    if (videoIndex < 0 || audioIndex < 0) {
        return ::media::Status::failure(invalid(
            "scheduled TS output does not contain both streams"));
    }
    auto video = openDecoder(*input->streams[videoIndex]);
    auto audio = openDecoder(*input->streams[audioIndex]);
    if (!video) return ::media::Status::failure(video.error());
    if (!audio) return ::media::Status::failure(audio.error());
    FramePtr frame(av_frame_alloc());
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("scheduled TS decode frame"));
    }
    bool videoDecoded = false;
    bool audioDecoded = false;
    AVPacket packet{};
    int read = 0;
    while ((read = av_read_frame(input.get(), &packet)) >= 0) {
        AVCodecContext* decoder = packet.stream_index == videoIndex
            ? video.value().get()
            : packet.stream_index == audioIndex ? audio.value().get() : nullptr;
        bool* decoded = packet.stream_index == videoIndex
            ? &videoDecoded
            : packet.stream_index == audioIndex ? &audioDecoded : nullptr;
        if (decoder && decoded) {
            int sent = avcodec_send_packet(decoder, &packet);
            if (sent == AVERROR(EAGAIN) &&
                receiveFrames(*decoder, *frame, *decoded, false)) {
                sent = avcodec_send_packet(decoder, &packet);
            }
            if (sent < 0 || !receiveFrames(*decoder, *frame, *decoded, false)) {
                av_packet_unref(&packet);
                return ::media::Status::failure(invalid(
                    "scheduled TS output packet did not decode"));
            }
        }
        av_packet_unref(&packet);
    }
    if (read != AVERROR_EOF) {
        return ::media::Status::failure(invalid(
            "scheduled TS demux did not finish at EOF"));
    }
    for (auto* decoder : {video.value().get(), audio.value().get()}) {
        const int sent = avcodec_send_packet(decoder, nullptr);
        bool& decoded = decoder == video.value().get()
            ? videoDecoded : audioDecoded;
        if ((sent < 0 && sent != AVERROR_EOF) ||
            !receiveFrames(*decoder, *frame, decoded, true)) {
            return ::media::Status::failure(invalid(
                "scheduled TS decoder did not flush at EOF"));
        }
    }
    if (!videoDecoded || !audioDecoded) {
        return ::media::Status::failure(invalid(
            "scheduled TS output did not decode both H264 and AAC"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MpegTsOutputArtifactVerifier::verify(
    const std::filesystem::path& path,
    const MediaTsMuxPlan& muxPlan,
    const MediaAvSyncPlan& avSyncPlan)
{
    auto bytes = readFile(path);
    if (!bytes) return ::media::Status::failure(bytes.error());
    auto transport = verifyTransport(bytes.value(), muxPlan);
    if (!transport) return transport;
    auto timing = verifyProductionTimestamps(
        bytes.value(), muxPlan, avSyncPlan);
    if (!timing) return timing;
    return decodeBoth(path);
}

} // namespace media_transcode::test
