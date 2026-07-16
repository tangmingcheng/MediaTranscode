#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/audio/AudioDecodeNode.h"
#include "internal/graph/nodes/audio/AudioEncodeNode.h"
#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/nodes/audio/MediaAudioStartupTrimNode.h"
#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaDecodedAudioTrimInputBuffer.h"
#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
}

#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <vector>

using namespace media::ffmpeg::graph;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed: " #condition << " at " << __LINE__ << '\n'; \
    std::exit(1); } } while (false)

namespace {

class ScriptedAudioDecoderCodecApi final : public AudioDecoderCodecApi {
public:
    std::deque<int> sendResults;
    std::deque<int> receiveResults;
    std::vector<const AVPacket*> sentPacketPointers;
    std::vector<std::int64_t> sentPacketPts;
    std::vector<int> sentPacketFirstBytes;
    int flushCount = 0;

    int sendPacket(AVCodecContext*, const AVPacket* packet) noexcept override
    {
        if (packet) {
            sentPacketPointers.push_back(packet);
            sentPacketPts.push_back(packet->pts);
            sentPacketFirstBytes.push_back(
                packet->size > 0 && packet->data ? packet->data[0] : -1);
        }
        if (sendResults.empty()) return 0;
        const int result = sendResults.front();
        sendResults.pop_front();
        return result;
    }

    int receiveFrame(AVCodecContext* codec, AVFrame* frame) noexcept override
    {
        if (receiveResults.empty()) return AVERROR(EAGAIN);
        const int result = receiveResults.front();
        receiveResults.pop_front();
        if (result != 0) return result;
        frame->format = AV_SAMPLE_FMT_FLTP;
        frame->sample_rate = codec->sample_rate;
        frame->nb_samples = 1'024;
        av_channel_layout_default(&frame->ch_layout, 2);
        return av_frame_get_buffer(frame, 0);
    }

    void flushBuffers(AVCodecContext*) noexcept override
    {
        ++flushCount;
        receiveResults.clear();
    }
};

class ScriptedAudioEncoderCodecApi final : public AudioEncoderCodecApi {
public:
    std::deque<int> sendResults;
    std::deque<int> receiveResults;

    int sendFrame(AVCodecContext*, const AVFrame*) noexcept override
    {
        if (!sendResults.empty()) {
            const int result = sendResults.front();
            sendResults.pop_front();
            return result;
        }
        return 0;
    }

    int receivePacket(AVCodecContext*, AVPacket* packet) noexcept override
    {
        if (receiveResults.empty()) return AVERROR(EAGAIN);
        const int result = receiveResults.front();
        receiveResults.pop_front();
        if (result == 0) {
            if (av_new_packet(packet, 1) < 0) return AVERROR(ENOMEM);
            packet->pts = 0;
            packet->dts = 0;
            packet->duration = 480;
        }
        return result;
    }

    void flushBuffers(AVCodecContext*) noexcept override {}
};

MediaBufferRef makeReleasedPacket(
    std::uint64_t generation,
    std::uint64_t sequence,
    int sourceRate,
    int samples)
{
    auto packet = ::media::ffmpeg::makePacket();
    CHECK(packet && av_new_packet(packet.get(), 1) == 0);
    packet->pts = static_cast<std::int64_t>(sequence);
    packet->dts = packet->pts;
    packet->data[0] = static_cast<std::uint8_t>(sequence & 0xffU);
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(packet), MediaStreamKind::Audio, std::nullopt);
    CHECK(wrapped);
    auto duration = MediaRunningTime::checkedFromTicks(samples, 1, sourceRate);
    CHECK(duration);
    auto lineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            MediaRunningTime::fromNanoseconds(0), std::nullopt,
            duration.value(), MediaDecodeOrderMode::PresentationOrderNoReorder,
            "audio-lineage-node", MediaSourceAccessUnitSequence(sequence),
            MediaTimeMappingConfidence::Locked, generation});
    auto canonical = MediaCanonicalAccessUnitBuffer::create(
        wrapped.value(), lineage);
    CHECK(canonical);
    auto released = MediaAvReleasedAudioBuffer::create(
        canonical.value(), 0,
        {generation, MediaRunningTime::fromNanoseconds(0),
         MediaRunningTime::fromNanoseconds(0), 0, 48'000});
    CHECK(released);
    return std::move(released).value();
}

MediaBufferRef makeDecodedFrame(std::uint64_t generation,
                                std::uint64_t sequence)
{
    auto frame = ::media::ffmpeg::makeFrame();
    CHECK(frame);
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 48'000;
    frame->nb_samples = 480;
    frame->pts = 0;
    av_channel_layout_default(&frame->ch_layout, 2);
    CHECK(av_frame_get_buffer(frame.get(), 0) == 0);
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Audio);
    CHECK(wrapped);
    auto lineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            MediaRunningTime::fromNanoseconds(0), std::nullopt,
            MediaRunningTime::fromNanoseconds(10'000'000),
            MediaDecodeOrderMode::PresentationOrderNoReorder,
            "startup-retained", MediaSourceAccessUnitSequence(sequence),
            MediaTimeMappingConfidence::Locked, generation});
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        wrapped.value(), lineage, {0, 480, 48'000});
    CHECK(canonical);
    auto decoded = MediaDecodedAudioTrimInputBuffer::create(
        canonical.value(),
        {generation, MediaRunningTime::fromNanoseconds(0),
         MediaRunningTime::fromNanoseconds(0), 0, 48'000},
        0);
    CHECK(decoded);
    return std::move(decoded).value();
}

MediaBufferRef makeBoundFrame(std::uint64_t generation,
                              std::uint64_t sequence,
                              std::int64_t begin = 0)
{
    auto frame = ::media::ffmpeg::makeFrame();
    CHECK(frame);
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 48'000;
    frame->nb_samples = 480;
    frame->pts = begin;
    av_channel_layout_default(&frame->ch_layout, 2);
    CHECK(av_frame_get_buffer(frame.get(), 0) == 0);
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Audio);
    CHECK(wrapped);
    auto lineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            MediaRunningTime::checkedFromTicks(begin, 1, 48'000).value(),
            std::nullopt,
            MediaRunningTime::fromNanoseconds(10'000'000),
            MediaDecodeOrderMode::PresentationOrderNoReorder,
            "resample-retained", MediaSourceAccessUnitSequence(sequence),
            MediaTimeMappingConfidence::Locked, generation});
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        wrapped.value(), lineage, {begin, begin + 480, 48'000});
    CHECK(canonical);
    auto bound = MediaBoundCanonicalAudioBuffer::create(
        canonical.value(),
        {generation, MediaRunningTime::fromNanoseconds(0),
         MediaRunningTime::fromNanoseconds(0), 0, 48'000});
    CHECK(bound);
    return std::move(bound).value();
}

::media::Result<MediaBufferRef> makeAudioCodec(int sampleRate)
{
    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    if (!codec) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("test codec allocation failed"));
    }
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = sampleRate;
    codec->time_base = AVRational{1, sampleRate};
    codec->pkt_timebase = AVRational{1, sampleRate};
    av_channel_layout_default(&codec->ch_layout, 2);
    return FFmpegBufferFactory::wrapCodecContext(std::move(codec));
}

void decoderRetryAnd44100To48000Chain()
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(4);
    const auto decodeCodecSource = graph.addNode(MediaNodeKind::DebugDump, "audio.decode.codec");
    const auto resampleCodecSource = graph.addNode(MediaNodeKind::DebugDump, "audio.resample.codec");
    const auto packetSource = graph.addNode(MediaNodeKind::DebugDump, "audio.packet");
    const auto decode = graph.addNode(MediaNodeKind::AudioDecode, "audio.decode");
    const auto trim = graph.addNode(MediaNodeKind::AudioStartupTrim, "audio.trim");
    const auto resample = graph.addNode(MediaNodeKind::AudioResample, "audio.resample");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "audio.sink");
    CHECK(graph.setNodeOption(
        resample, MediaAudioCorrectionOptionKey::Mode, "disabled"));

    graph.addOutputPort(decodeCodecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(resampleCodecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(packetSource, "packet", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(decode, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(decode, "packet", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(decode, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(trim, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(trim, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    CHECK(graph.connect(decodeCodecSource, "codec", decode, "codec", "decode.codec", policy));
    CHECK(graph.connect(resampleCodecSource, "codec", resample, "codec", "resample.codec", policy));
    CHECK(graph.connect(packetSource, "packet", decode, "packet", "decode.packet", policy));
    CHECK(graph.connect(decode, "frame", trim, "frame", "decode.trim", policy));
    CHECK(graph.connect(trim, "frame", resample, "frame", "trim.resample", policy));
    CHECK(graph.connect(resample, "frame", sink, "frame", "resample.output", policy));

    MediaGraphExecutionContext execution;
    CHECK(execution.compile(graph));
    auto decodeCodec = makeAudioCodec(44'100);
    auto resampleCodec = makeAudioCodec(48'000);
    CHECK(decodeCodec && resampleCodec);
    CHECK(execution.findInputChannel(decode, "codec")->push(decodeCodec.value()));
    CHECK(execution.findInputChannel(resample, "codec")->push(resampleCodec.value()));

    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto decodeState = std::make_shared<AudioDecodeLineageState>(mode, 2);
    auto trimState = std::make_shared<MediaAudioStartupTrimLineageState>(mode, 2);
    auto resampleState = std::make_shared<AudioResampleLineageState>(mode, 2);
    auto api = std::make_shared<ScriptedAudioDecoderCodecApi>();
    AudioDecodeNode decodeNode(decode, mode, decodeState, api);
    MediaAudioStartupTrimNode trimNode(trim, trimState);
    AudioResampleNode resampleNode(resample, mode, resampleState);
    CHECK(decodeNode.start(execution));
    CHECK(trimNode.start(execution));
    CHECK(resampleNode.start(execution));
    CHECK(decodeNode.process(execution));
    CHECK(resampleNode.process(execution));

    CHECK(trimState->observe(7));
    auto flush = FFmpegBufferFactory::makeFlush(MediaStreamKind::Audio);
    CHECK(flush && execution.findInputChannel(trim, "frame")->push(flush.value()));
    auto flushResult = trimNode.process(execution);
    CHECK(flushResult && flushResult.value().state == MediaNodeProcessState::Progress);
    MediaBufferRef forwarded;
    CHECK(execution.findInputChannel(resample, "frame")->tryPop(forwarded));
    CHECK(forwarded == flush.value());

    const auto released = makeReleasedPacket(7, 1, 44'100, 1'024);
    CHECK(execution.findInputChannel(decode, "packet")->push(released));
    api->sendResults = {AVERROR(EAGAIN), 0};
    api->receiveResults = {AVERROR(EAGAIN), 0, AVERROR(EAGAIN)};
    CHECK(decodeNode.process(execution));
    CHECK(api->sentPacketPointers.size() == 1);
    CHECK(decodeState->intervals.queuedSamples() == 1'024);
    CHECK(decodeNode.process(execution));
    CHECK(api->sentPacketPointers.size() == 2);
    CHECK(api->sentPacketPointers[0] == api->sentPacketPointers[1]);
    CHECK(decodeState->intervals.queuedSamples() == 0);
    CHECK(trimNode.process(execution));
    CHECK(resampleNode.process(execution));

    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
    CHECK(eof && execution.findInputChannel(trim, "frame")->push(eof.value()));
    auto eofResult = trimNode.process(execution);
    CHECK(eofResult && eofResult.value().state == MediaNodeProcessState::Finished);
    std::int64_t expectedBegin = 0;
    std::int64_t totalSamples = 0;
    bool resampleFinished = false;
    bool eofObserved = false;
    for (int step = 0; step < 32 && !resampleFinished; ++step) {
        auto result = resampleNode.process(execution);
        CHECK(result);
        resampleFinished =
            result.value().state == MediaNodeProcessState::Finished;
        MediaBufferRef output;
        while (execution.findOutputChannel(resample, "frame")->tryPop(output)) {
            if (output->isEof()) {
                eofObserved = true;
                continue;
            }
            const auto* bound =
                dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(output.get());
            CHECK(bound);
            const AVFrame* frame =
                FFmpegFrameView::frame(bound->media()->media());
            CHECK(frame && frame->sample_rate == 48'000 &&
                  frame->nb_samples > 0);
            CHECK(bound->media()->interval().sampleRate == 48'000);
            CHECK(bound->media()->interval().begin == expectedBegin);
            expectedBegin = bound->media()->interval().end;
            totalSamples += frame->nb_samples;
            CHECK(bound->audioOrigin().outputSampleRate == 48'000);
        }
    }
    const auto expectedSamples = av_rescale_q_rnd(
        1'024, AVRational{1, 44'100}, AVRational{1, 48'000},
        AV_ROUND_NEAR_INF);
    CHECK(resampleFinished);
    CHECK(eofObserved);
    CHECK(totalSamples == expectedSamples);
    CHECK(expectedBegin == expectedSamples);
    CHECK(resampleState->outputIntervals.queuedSamples() == 0);
    MediaBufferRef extra;
    CHECK(!execution.findOutputChannel(resample, "frame")->tryPop(extra));

    resampleNode.abort(execution);
    trimNode.abort(execution);
    decodeNode.abort(execution);
}

void decoderPurgeDropsEagainPacketBeforeRetry()
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    const auto codecSource = graph.addNode(MediaNodeKind::DebugDump, "purge.codec");
    const auto packetSource = graph.addNode(MediaNodeKind::DebugDump, "purge.packet");
    const auto decode = graph.addNode(MediaNodeKind::AudioDecode, "purge.decode");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "purge.sink");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(packetSource, "packet", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(decode, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(decode, "packet", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(decode, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    CHECK(graph.connect(codecSource, "codec", decode, "codec", "purge.codec", policy));
    CHECK(graph.connect(packetSource, "packet", decode, "packet", "purge.packet", policy));
    CHECK(graph.connect(decode, "frame", sink, "frame", "purge.output", policy));
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(graph));
    auto codec = makeAudioCodec(48'000);
    CHECK(codec && execution.findInputChannel(decode, "codec")->push(codec.value()));
    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<AudioDecodeLineageState>(mode, 1);
    auto api = std::make_shared<ScriptedAudioDecoderCodecApi>();
    AudioDecodeNode node(decode, mode, state, api);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(execution.findInputChannel(decode, "packet")->push(
        makeReleasedPacket(7, 1, 48'000, 1'024)));
    api->sendResults = {AVERROR(EAGAIN)};
    api->receiveResults = {AVERROR(EAGAIN)};
    CHECK(node.process(execution));
    CHECK(api->sentPacketPointers.size() == 1);
    CHECK(node.generationPurgeTarget()->purge({7, 8, 1}));
    CHECK(api->flushCount == 1);
    const auto sendsAfterPurge = api->sentPacketPointers.size();
    CHECK(node.process(execution));
    CHECK(api->sentPacketPointers.size() == sendsAfterPurge);
    CHECK(execution.findInputChannel(decode, "packet")->push(
        makeReleasedPacket(8, 2, 48'000, 1'024)));
    api->sendResults = {0};
    api->receiveResults = {0, AVERROR(EAGAIN)};
    CHECK(node.process(execution));
    CHECK(api->sentPacketPointers.size() == 2);
    CHECK(api->sentPacketPts.back() == 2);
    CHECK(api->sentPacketFirstBytes.back() == 2);
    MediaBufferRef output;
    CHECK(execution.findOutputChannel(decode, "frame")->tryPop(output));
    const auto* decoded =
        dynamic_cast<const MediaDecodedAudioTrimInputBuffer*>(output.get());
    CHECK(decoded && decoded->audioOrigin().generation == 8);

    auto* frameOutput = execution.findOutputChannel(decode, "frame");
    auto blocker = FFmpegBufferFactory::wrapFrame(
        ::media::ffmpeg::makeFrame(), MediaStreamKind::Audio);
    CHECK(blocker && frameOutput->push(blocker.value()));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    CHECK(eof && execution.findInputChannel(decode, "packet")->push(eof.value()));
    api->sendResults = {0};
    api->receiveResults = {AVERROR_EOF};
    auto retainedEof = node.process(execution);
    CHECK(retainedEof &&
          retainedEof.value().state == MediaNodeProcessState::Progress);
    CHECK(node.generationPurgeTarget()->purge({8, 9, 2}));
    CHECK(frameOutput->tryPop(output) && output == blocker.value());
    auto afterTerminalPurge = node.process(execution);
    CHECK(afterTerminalPurge &&
          afterTerminalPurge.value().state == MediaNodeProcessState::Waiting);
    CHECK(!frameOutput->closed());
    CHECK(!frameOutput->tryPop(output));

    CHECK(execution.findInputChannel(decode, "packet")->push(
        makeReleasedPacket(9, 3, 48'000, 1'024)));
    api->sendResults = {0};
    api->receiveResults = {0, AVERROR(EAGAIN)};
    CHECK(node.process(execution));
    CHECK(frameOutput->tryPop(output));
    decoded = dynamic_cast<const MediaDecodedAudioTrimInputBuffer*>(output.get());
    CHECK(decoded && decoded->audioOrigin().generation == 9);
    node.abort(execution);
}

void retainedControlFreshnessIsExactAndGenerationBound()
{
    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<AudioDecodeLineageState>(mode, 1);
    auto genericEof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    auto audioFlush = FFmpegBufferFactory::makeFlush(MediaStreamKind::Audio);
    auto videoEof = FFmpegBufferFactory::makeEof(MediaStreamKind::Video);
    CHECK(genericEof && audioFlush && videoEof);
    CHECK(!state->authorizeRetainedControl(genericEof.value()));
    CHECK(state->observe(7));
    CHECK(state->authorizeRetainedControl(genericEof.value()));
    CHECK(state->pendingOutputIsCurrent(genericEof.value(), std::nullopt));
    CHECK(!state->pendingOutputIsCurrent(videoEof.value(), std::nullopt));
    CHECK(!state->pendingOutputIsCurrent(audioFlush.value(), std::nullopt));
    CHECK(state->authorizeRetainedControl(audioFlush.value()));
    CHECK(state->pendingOutputIsCurrent(audioFlush.value(), std::nullopt));
    CHECK(state->purge({7, 8, 1}));
    CHECK(!state->pendingOutputIsCurrent(audioFlush.value(), std::nullopt));

    auto legacy = std::make_shared<AudioDecodeLineageState>(
        MediaAudioLineageExecutionMode::LegacyPlainPacket, 0);
    CHECK(legacy->pendingOutputIsCurrent(videoEof.value(), std::nullopt));
}

void startupTrimDropsRetainedEofAndContinuesNextGeneration()
{
    MediaGraph graph;
    auto policy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    policy.queuePolicy.allowFlushControlBypass = false;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "startup.source");
    const auto trim = graph.addNode(MediaNodeKind::AudioStartupTrim, "startup.trim");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "startup.sink");
    graph.addOutputPort(source, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(trim, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(trim, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    CHECK(graph.connect(source, "frame", trim, "frame", "startup.input", policy));
    CHECK(graph.connect(trim, "frame", sink, "frame", "startup.output", policy));
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(graph));
    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<MediaAudioStartupTrimLineageState>(mode, 1);
    MediaAudioStartupTrimNode node(trim, state);
    CHECK(node.start(execution));
    CHECK(state->observe(7));
    auto blocker = FFmpegBufferFactory::wrapFrame(
        ::media::ffmpeg::makeFrame(), MediaStreamKind::Audio);
    CHECK(blocker);
    auto* output = execution.findOutputChannel(trim, "frame");
    CHECK(output && output->push(blocker.value()));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    CHECK(eof && execution.findInputChannel(trim, "frame")->push(eof.value()));
    auto retained = node.process(execution);
    CHECK(retained && retained.value().state == MediaNodeProcessState::Progress);
    CHECK(node.generationPurgeTarget()->purge({7, 8, 1}));
    MediaBufferRef popped;
    CHECK(output->tryPop(popped) && popped == blocker.value());
    auto afterPurge = node.process(execution);
    CHECK(afterPurge && afterPurge.value().state == MediaNodeProcessState::Waiting);
    CHECK(!output->closed());
    CHECK(!output->tryPop(popped));
    CHECK(execution.findInputChannel(trim, "frame")->push(
        makeDecodedFrame(8, 2)));
    CHECK(node.process(execution));
    CHECK(output->tryPop(popped));
    const auto* bound =
        dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(popped.get());
    CHECK(bound && bound->audioOrigin().generation == 8);
    node.abort(execution);
}

void encoderCodecConfigSurvivesOutputBackpressureExactlyOnce()
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    const auto codecSource = graph.addNode(
        MediaNodeKind::DebugDump, "encoder-config.codec-source");
    const auto frameSource = graph.addNode(
        MediaNodeKind::DebugDump, "encoder-config.frame-source");
    const auto encoder = graph.addNode(
        MediaNodeKind::AudioEncode, "encoder-config.encoder");
    const auto codecSink = graph.addNode(
        MediaNodeKind::DebugDump, "encoder-config.codec-sink");
    const auto packetSink = graph.addNode(
        MediaNodeKind::DebugDump, "encoder-config.packet-sink");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(encoder, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(encoder, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(encoder, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(encoder, "packet", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    graph.addInputPort(codecSink, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(packetSink, "packet", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    CHECK(graph.connect(codecSource, "codec", encoder, "codec",
                        "encoder-config.codec", policy));
    CHECK(graph.connect(frameSource, "frame", encoder, "frame",
                        "encoder-config.frame", policy));
    CHECK(graph.connect(encoder, "codec", codecSink, "codec",
                        "encoder-config.output", policy));
    CHECK(graph.connect(encoder, "packet", packetSink, "packet",
                        "encoder-config.packet", policy));

    MediaGraphExecutionContext execution;
    CHECK(execution.compile(graph));
    auto codec = makeAudioCodec(48'000);
    CHECK(codec);
    auto* codecInput = execution.findInputChannel(encoder, "codec");
    auto* codecOutput = execution.findOutputChannel(encoder, "codec");
    CHECK(codecInput && codecOutput);
    auto blocker = makeAudioCodec(48'000);
    CHECK(blocker && codecOutput->push(blocker.value()));
    CHECK(codecInput->push(codec.value()));

    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<AudioEncodeLineageState>(mode, 1);
    auto api = std::make_shared<ScriptedAudioEncoderCodecApi>();
    AudioEncodeNode node(encoder, mode, state, api);
    CHECK(node.start(execution));
    auto retained = node.process(execution);
    CHECK(retained && retained.value().state == MediaNodeProcessState::Waiting);
    MediaBufferRef output;
    CHECK(codecOutput->tryPop(output) && output == blocker.value());
    auto drained = node.process(execution);
    CHECK(drained && drained.value().state == MediaNodeProcessState::Progress);
    CHECK(codecOutput->tryPop(output) && output == codec.value());
    CHECK(!codecOutput->tryPop(output));
    CHECK(node.process(execution));
    CHECK(!codecOutput->tryPop(output));
    node.abort(execution);
}

void resampleDropsRetainedEofAndContinuesNextGeneration()
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    const auto codecSource = graph.addNode(
        MediaNodeKind::DebugDump, "resample-retained.codec");
    const auto frameSource = graph.addNode(
        MediaNodeKind::DebugDump, "resample-retained.frame");
    const auto resample = graph.addNode(
        MediaNodeKind::AudioResample, "resample-retained.node");
    const auto sink = graph.addNode(
        MediaNodeKind::DebugDump, "resample-retained.sink");
    CHECK(graph.setNodeOption(
        resample, MediaAudioCorrectionOptionKey::Mode, "disabled"));
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    CHECK(graph.connect(codecSource, "codec", resample, "codec",
                        "resample-retained.codec", policy));
    CHECK(graph.connect(frameSource, "frame", resample, "frame",
                        "resample-retained.frame", policy));
    CHECK(graph.connect(resample, "frame", sink, "frame",
                        "resample-retained.output", policy));

    MediaGraphExecutionContext execution;
    CHECK(execution.compile(graph));
    auto codec = makeAudioCodec(48'000);
    CHECK(codec && execution.findInputChannel(resample, "codec")->push(
                       codec.value()));
    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<AudioResampleLineageState>(mode, 1);
    AudioResampleNode node(resample, mode, state);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(state->observe(7));

    auto blocker = FFmpegBufferFactory::wrapFrame(
        ::media::ffmpeg::makeFrame(), MediaStreamKind::Audio);
    auto* output = execution.findOutputChannel(resample, "frame");
    CHECK(blocker && output->push(blocker.value()));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    CHECK(eof && execution.findInputChannel(resample, "frame")->push(
                     eof.value()));
    auto retained = node.process(execution);
    CHECK(retained && retained.value().state == MediaNodeProcessState::Progress);
    CHECK(node.generationPurgeTarget()->purge({7, 8, 1}));
    MediaBufferRef popped;
    CHECK(output->tryPop(popped) && popped == blocker.value());
    auto afterPurge = node.process(execution);
    CHECK(afterPurge && afterPurge.value().state == MediaNodeProcessState::Waiting);
    CHECK(!output->closed());
    CHECK(!output->tryPop(popped));

    CHECK(execution.findInputChannel(resample, "frame")->push(
        makeBoundFrame(8, 2)));
    CHECK(node.process(execution));
    CHECK(output->tryPop(popped));
    const auto* bound =
        dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(popped.get());
    CHECK(bound && bound->audioOrigin().generation == 8);
    node.abort(execution);
}

void resampleMapperFailureDoesNotConsumeLineage()
{
    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<AudioResampleLineageState>(mode, 1);
    AudioResampleLineageMapper mapper(state);
    auto input = makeBoundFrame(7, 1);
    const auto* bound =
        dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(input.get());
    CHECK(bound);
    const AVFrame* frame = FFmpegFrameView::frame(bound->media()->media());
    CHECK(frame);
    CHECK(mapper.acceptInput(*bound, *frame, 48'000));
    CHECK(state->outputIntervals.queuedSamples() == 480);
    auto invalid = mapper.bindOutput(bound->media()->media(), 0);
    CHECK(!invalid);
    CHECK(state->outputIntervals.queuedSamples() == 480);
    auto valid = mapper.bindOutput(bound->media()->media(), 480);
    CHECK(valid);
    CHECK(state->outputIntervals.queuedSamples() == 0);
}

void encoderDropsRetainedEofAndContinuesNextGeneration()
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    const auto codecSource = graph.addNode(
        MediaNodeKind::DebugDump, "encode-retained.codec");
    const auto frameSource = graph.addNode(
        MediaNodeKind::DebugDump, "encode-retained.frame");
    const auto encoder = graph.addNode(
        MediaNodeKind::AudioEncode, "encode-retained.node");
    const auto sink = graph.addNode(
        MediaNodeKind::DebugDump, "encode-retained.sink");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(encoder, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(encoder, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(encoder, "packet", MediaStreamKind::Audio,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    graph.addInputPort(sink, "packet", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    CHECK(graph.connect(codecSource, "codec", encoder, "codec",
                        "encode-retained.codec", policy));
    CHECK(graph.connect(frameSource, "frame", encoder, "frame",
                        "encode-retained.frame", policy));
    CHECK(graph.connect(encoder, "packet", sink, "packet",
                        "encode-retained.output", policy));

    MediaGraphExecutionContext execution;
    CHECK(execution.compile(graph));
    auto codec = makeAudioCodec(48'000);
    CHECK(codec && execution.findInputChannel(encoder, "codec")->push(
                       codec.value()));
    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<AudioEncodeLineageState>(mode, 2);
    auto api = std::make_shared<ScriptedAudioEncoderCodecApi>();
    AudioEncodeNode node(encoder, mode, state, api);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(state->observe(7));

    CHECK(execution.findInputChannel(encoder, "frame")->push(
        makeBoundFrame(7, 1)));
    api->sendResults = {0};
    api->receiveResults = {AVERROR(EAGAIN)};
    CHECK(node.process(execution));
    auto missingPacketEof = FFmpegBufferFactory::makeEof(
        MediaStreamKind::Control);
    CHECK(missingPacketEof &&
          execution.findInputChannel(encoder, "frame")->push(
              missingPacketEof.value()));
    api->sendResults = {0};
    api->receiveResults = {AVERROR_EOF};
    CHECK(!node.process(execution));
    CHECK(node.generationPurgeTarget()->purge({7, 8, 1}));

    auto blockerPacket = ::media::ffmpeg::makePacket();
    CHECK(blockerPacket && av_new_packet(blockerPacket.get(), 1) == 0);
    auto blocker = FFmpegBufferFactory::wrapPacket(
        std::move(blockerPacket), MediaStreamKind::Audio, std::nullopt);
    auto* output = execution.findOutputChannel(encoder, "packet");
    CHECK(blocker && output->push(blocker.value()));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    CHECK(eof && execution.findInputChannel(encoder, "frame")->push(
                     eof.value()));
    api->sendResults = {0};
    api->receiveResults = {AVERROR_EOF};
    auto retained = node.process(execution);
    CHECK(retained && retained.value().state == MediaNodeProcessState::Progress);
    CHECK(node.generationPurgeTarget()->purge({8, 9, 2}));
    MediaBufferRef popped;
    CHECK(output->tryPop(popped) && popped == blocker.value());
    auto afterPurge = node.process(execution);
    CHECK(afterPurge && afterPurge.value().state == MediaNodeProcessState::Waiting);
    CHECK(!output->closed());
    CHECK(!output->tryPop(popped));

    CHECK(execution.findInputChannel(encoder, "frame")->push(
        makeBoundFrame(9, 2)));
    api->sendResults = {0};
    api->receiveResults = {0, AVERROR(EAGAIN)};
    CHECK(node.process(execution));
    CHECK(output->tryPop(popped));
    const auto* encoded =
        dynamic_cast<const MediaEncodedAudioLineageBuffer*>(popped.get());
    CHECK(encoded && encoded->audioOrigin().generation == 9);

    CHECK(execution.findInputChannel(encoder, "frame")->push(
        makeBoundFrame(9, 3, 480)));
    api->sendResults = {0};
    api->receiveResults = {AVERROR(EAGAIN)};
    CHECK(node.process(execution));
    CHECK(!output->tryPop(popped));
    CHECK(execution.findInputChannel(encoder, "frame")->push(
        makeBoundFrame(9, 4, 960)));
    api->sendResults = {0};
    api->receiveResults = {0, AVERROR(EAGAIN)};
    CHECK(node.process(execution));
    CHECK(output->tryPop(popped));
    encoded = dynamic_cast<const MediaEncodedAudioLineageBuffer*>(popped.get());
    CHECK(encoded && encoded->fragments().size() == 1);
    CHECK(encoded->fragments().front().lineage->sourceSequence ==
          MediaSourceAccessUnitSequence(3));
    node.abort(execution);
}

} // namespace

int main()
{
    decoderRetryAnd44100To48000Chain();
    decoderPurgeDropsEagainPacketBeforeRetry();
    retainedControlFreshnessIsExactAndGenerationBound();
    startupTrimDropsRetainedEofAndContinuesNextGeneration();
    encoderCodecConfigSurvivesOutputBackpressureExactlyOnce();
    resampleDropsRetainedEofAndContinuesNextGeneration();
    resampleMapperFailureDoesNotConsumeLineage();
    encoderDropsRetainedEofAndContinuesNextGeneration();
    std::cout << "audio lineage node tests passed\n";
    return 0;
}
