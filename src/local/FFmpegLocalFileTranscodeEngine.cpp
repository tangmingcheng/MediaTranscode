#include "local/FFmpegLocalFileTranscodeEngine.h"

#include "internal/FFmpegAudioPipeline.h"
#include "internal/FFmpegAudioStrategyPlanner.h"
#include "internal/FFmpegPhaseDiagnostics.h"
#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegTranscodeLoopDiagnostics.h"
#include "internal/FFmpegUtils.h"
#include "internal/FFmpegVideoTranscodePipeline.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace media {
namespace {

class RunningStateGuard {
public:
    explicit RunningStateGuard(std::atomic_bool& running)
        : m_running(running)
    {
    }

    ~RunningStateGuard()
    {
        m_running.store(false);
    }

    RunningStateGuard(const RunningStateGuard&) = delete;
    RunningStateGuard& operator=(const RunningStateGuard&) = delete;

private:
    std::atomic_bool& m_running;
};

const char* executionModeName(ffmpeg::VideoExecutionMode mode)
{
    switch (mode) {
    case ffmpeg::VideoExecutionMode::HardwareZeroCopy:
        return "hardware-zero-copy";
    case ffmpeg::VideoExecutionMode::HardwareDecodeSoftwareFilterHardwareEncode:
        return "hardware-decode-software-filter-hardware-encode";
    case ffmpeg::VideoExecutionMode::HardwareDecodeSoftwareFilterGenericEncode:
        return "hardware-decode-software-filter-generic-encode";
    case ffmpeg::VideoExecutionMode::Cpu:
    default:
        return "cpu";
    }
}

} // namespace

FFmpegLocalFileTranscodeEngine::FFmpegLocalFileTranscodeEngine()
{
    avformat_network_init();
}

FFmpegLocalFileTranscodeEngine::~FFmpegLocalFileTranscodeEngine()
{
    stop();
}

bool FFmpegLocalFileTranscodeEngine::initialize(const TranscodeConfig& config)
{
    if (m_running.load()) {
        setLastError("initialize failed: local file transcode engine is running");
        return false;
    }

    if (config.inputUrl.empty()) {
        setLastError("initialize failed: input path is empty");
        return false;
    }

    if (config.outputUrl.empty()) {
        setLastError("initialize failed: output path is empty");
        return false;
    }

    if (config.videoCodec == VideoCodec::Copy) {
        setLastError("initialize failed: VideoCodec::Copy is not implemented in local video transcode API");
        return false;
    }

    m_config = config;
    clearLastError();

    return true;
}

bool FFmpegLocalFileTranscodeEngine::start()
{
    if (m_running.load()) {
        setLastError("start failed: local file transcode engine is already running");
        return false;
    }

    if (m_config.inputUrl.empty() || m_config.outputUrl.empty()) {
        setLastError("start failed: local file transcode engine is not initialized");
        return false;
    }

    /*
     * 防止同一个对象二次 start 时，之前线程虽然结束但还没有 join。
     */
    if (m_transcodeThread.joinable()) {
        m_transcodeThread.join();
    }

    clearLastError();

    m_stopRequested.store(false);
    m_running.store(true);

    try {
        m_transcodeThread = std::thread(&FFmpegLocalFileTranscodeEngine::transcodeThread, this);
    }
    catch (const std::exception& e) {
        m_running.store(false);
        setLastError(std::string("start failed: create thread failed: ") + e.what());
        return false;
    }

    return true;
}

void FFmpegLocalFileTranscodeEngine::stop()
{
    m_stopRequested.store(true);

    if (m_transcodeThread.joinable()) {
        m_transcodeThread.join();
    }

    m_running.store(false);
}

bool FFmpegLocalFileTranscodeEngine::wait()
{
    if (m_transcodeThread.joinable()) {
        m_transcodeThread.join();
    }

    return !m_running.load();
}

bool FFmpegLocalFileTranscodeEngine::isRunning() const
{
    return m_running.load();
}

std::string FFmpegLocalFileTranscodeEngine::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void FFmpegLocalFileTranscodeEngine::setProgressCallback(ProgressCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCallback = std::move(cb);
}

void FFmpegLocalFileTranscodeEngine::transcodeThread()
{
    RunningStateGuard runningGuard(m_running);

    ffmpeg::InputFormatContextPtr inputFmtCtx;
    ffmpeg::OutputFormatContextPtr outputFmtCtx;
    ffmpeg::PacketPtr inputPacket;

    int videoStreamIndex = -1;
    int audioStreamIndex = -1;

    AVStream* inputVideoStream = nullptr;
    AVStream* inputAudioStream = nullptr;

    ffmpeg::TimelineNormalizer timeline;
    ffmpeg::FFmpegVideoTranscodePipeline videoPipeline;
    ffmpeg::FFmpegAudioPipeline audioPipeline;
    ffmpeg::FFmpegTranscodeLoopDiagnostics loopDiagnostics(1000);
    ffmpeg::FFmpegAudioStrategyPlanner::Plan audioStrategyPlan;

    int64_t encodedVideoPacketCount = 0;
    int64_t encodedAudioPacketCount = 0;
    int64_t lastWrittenVideoOutTimeMs = 0;
    int64_t lastWrittenAudioOutTimeMs = 0;
    int64_t progressCallbackCount = 0;

    const auto startTime = std::chrono::steady_clock::now();

    auto currentFinalizeCounters = [&]() {
        return ffmpeg::FFmpegPhaseDiagnostics::Counters{
            encodedVideoPacketCount,
            encodedAudioPacketCount,
            progressCallbackCount
        };
    };

    auto currentLoopCounters = [&]() {
        return ffmpeg::FFmpegTranscodeLoopDiagnostics::OutputCounters{
            encodedVideoPacketCount,
            encodedAudioPacketCount,
            progressCallbackCount,
            std::max(lastWrittenVideoOutTimeMs, lastWrittenAudioOutTimeMs)
        };
    };

    auto emitProgress = [&](const std::string& raw) {
        ProgressCallback callback;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callback = m_progressCallback;
        }

        if (!callback) {
            return;
        }

        ProgressInfo info;
        info.frame = encodedVideoPacketCount;
        info.outTimeMs = std::max(lastWrittenVideoOutTimeMs, lastWrittenAudioOutTimeMs);

        if (info.outTimeMs <= 0) {
            info.outTimeMs = videoPipeline.estimatedOutTimeMs();
        }

        const auto now = std::chrono::steady_clock::now();
        const double elapsedSec =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;

        if (elapsedSec > 0.001) {
            const double mediaSec = info.outTimeMs / 1000.0;
            info.speed = mediaSec / elapsedSec;
        }
        else {
            info.speed = 0.0;
        }

        info.raw = raw;
        ++progressCallbackCount;
        callback(info);
    };

    auto fail = [&](const std::string& error) {
        setLastError(error);
    };

    auto failStatus = [&](const Status& status) {
        setLastError(status.error().message);
    };

    emitProgress("initialized");

    AVFormatContext* rawInputFmtCtx = nullptr;
    int ret = avformat_open_input(&rawInputFmtCtx, m_config.inputUrl.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fail("avformat_open_input failed: " + ffmpeg::errorString(ret));
        return;
    }
    inputFmtCtx.reset(rawInputFmtCtx);

    ret = avformat_find_stream_info(inputFmtCtx.get(), nullptr);
    if (ret < 0) {
        fail("avformat_find_stream_info failed: " + ffmpeg::errorString(ret));
        return;
    }

    ret = av_find_best_stream(inputFmtCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        fail("av_find_best_stream video failed: " + ffmpeg::errorString(ret));
        return;
    }

    videoStreamIndex = ret;
    inputVideoStream = inputFmtCtx->streams[videoStreamIndex];

    if (m_config.audioEnabled) {
        ret = av_find_best_stream(inputFmtCtx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (ret >= 0) {
            audioStreamIndex = ret;
            inputAudioStream = inputFmtCtx->streams[audioStreamIndex];
        }
    }

    timeline.initStartFromFormat(inputFmtCtx.get(), inputVideoStream, inputAudioStream);

    AVFormatContext* rawOutputFmtCtx = nullptr;
    ret = avformat_alloc_output_context2(&rawOutputFmtCtx, nullptr, nullptr, m_config.outputUrl.c_str());
    if (ret < 0 || !rawOutputFmtCtx) {
        fail("avformat_alloc_output_context2 failed: " +
            (ret < 0 ? ffmpeg::errorString(ret) : std::string("no output context allocated")));
        return;
    }
    outputFmtCtx.reset(rawOutputFmtCtx);

    audioStrategyPlan = ffmpeg::FFmpegAudioStrategyPlanner::plan(
        m_config,
        inputAudioStream,
        outputFmtCtx.get()
    );
    spdlog::info(
        "[AUDIO][PLAN] enabled={}, selected_mode={}, requested_codec={}, selected_codec={}, target_bitrate_kbps={}, smart_copy={}, {}",
        m_config.audioEnabled,
        ffmpeg::audioPipelineModeName(audioStrategyPlan.mode),
        ffmpeg::FFmpegAudioStrategyPlanner::audioCodecName(m_config.audioCodec),
        ffmpeg::FFmpegAudioStrategyPlanner::audioCodecName(audioStrategyPlan.codec),
        audioStrategyPlan.audioBitrateKbps,
        audioStrategyPlan.smartCopy,
        audioStrategyPlan.diagnostic
    );

    const AVCodec* decoder = avcodec_find_decoder(inputVideoStream->codecpar->codec_id);
    ffmpeg::HardwarePipelinePlan plan;

    if (m_config.hardware.enabled) {
        plan = ffmpeg::FFmpegPipelinePlanner::planHardwarePipeline(
            m_config,
            decoder
        );
    }
    else {
        plan.executionMode = ffmpeg::VideoExecutionMode::Cpu;
        plan.diagnostic = "hardware disabled by config; using CPU pipeline";
        spdlog::warn("[PLAN] {}", plan.diagnostic);
    }

    const ffmpeg::HardwarePipelinePlan* executionPlan = nullptr;
    if (plan.valid && plan.executionMode != ffmpeg::VideoExecutionMode::Cpu) {
        executionPlan = &plan;
        if (plan.executionMode == ffmpeg::VideoExecutionMode::HardwareZeroCopy) {
            spdlog::info(
                "[PLAN] execution mode: {}: {}",
                executionModeName(plan.executionMode),
                plan.diagnostic
            );
        }
        else {
            spdlog::warn(
                "[PLAN] execution mode: {}: {}",
                executionModeName(plan.executionMode),
                plan.diagnostic
            );
        }
    }
    else {
        if (m_config.hardware.enabled && !m_config.hardware.allowZeroCopyFallback) {
            fail(plan.diagnostic.empty()
                ? std::string("zero-copy pipeline planning failed and fallback is disabled")
                : plan.diagnostic);
            return;
        }

        spdlog::warn(
            "[PLAN] execution mode: cpu-frame-pipeline fallback: {}",
            plan.diagnostic
        );
    }

    {
        ffmpeg::FFmpegVideoTranscodePipeline::Config videoConfig;
        videoConfig.transcodeConfig = &m_config;
        videoConfig.hardwarePlan = executionPlan;
        videoConfig.inputVideoStream = inputVideoStream;
        videoConfig.outputFmtCtx = outputFmtCtx.get();
        videoConfig.timeline = &timeline;

        Status videoStatus = videoPipeline.initialize(videoConfig);
        if (!videoStatus) {
            failStatus(videoStatus);
            return;
        }
    }

    if (inputAudioStream && audioStrategyPlan.mode != ffmpeg::FFmpegAudioPipelineMode::None) {
        ffmpeg::FFmpegAudioPipeline::Config audioConfig;
        audioConfig.mode = audioStrategyPlan.mode;
        audioConfig.codec = audioStrategyPlan.codec;
        audioConfig.inputAudioStream = inputAudioStream;
        audioConfig.outputFmtCtx = outputFmtCtx.get();
        audioConfig.timeline = &timeline;
        audioConfig.audioBitrateKbps = audioStrategyPlan.audioBitrateKbps;

        Status audioStatus = audioPipeline.initialize(audioConfig);
        if (!audioStatus) {
            failStatus(audioStatus);
            return;
        }
    }

    if (!(outputFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outputFmtCtx->pb, m_config.outputUrl.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            fail("avio_open output failed: " + ffmpeg::errorString(ret));
            return;
        }
    }

    ret = avformat_write_header(outputFmtCtx.get(), nullptr);
    if (ret < 0) {
        fail("avformat_write_header failed: " + ffmpeg::errorString(ret));
        return;
    }

    inputPacket = ffmpeg::makePacket();
    if (!inputPacket) {
        fail("av_packet_alloc input packet failed");
        return;
    }

    auto onVideoPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
        encodedVideoPacketCount = packetCount;
        lastWrittenVideoOutTimeMs = outTimeMs;

        if (encodedVideoPacketCount == 1 ||
            encodedVideoPacketCount % 25 == 0) {
            emitProgress("transcoding");
        }
    };

    auto onAudioPacketWritten = [&](int64_t packetCount, int64_t outTimeMs) {
        encodedAudioPacketCount = packetCount;
        lastWrittenAudioOutTimeMs = outTimeMs;

        if (encodedAudioPacketCount == 1 ||
            encodedAudioPacketCount % 25 == 0) {
            emitProgress("transcoding");
        }
    };

    loopDiagnostics.maybeLog(currentLoopCounters());

    while (!m_stopRequested.load()) {
        auto readStart = loopDiagnostics.mark();
        ret = av_read_frame(inputFmtCtx.get(), inputPacket.get());

        if (ret == AVERROR_EOF) {
            break;
        }

        if (ret < 0) {
            loopDiagnostics.flush(currentLoopCounters());
            fail("av_read_frame failed: " + ffmpeg::errorString(ret));
            return;
        }

        if (inputPacket->stream_index == videoStreamIndex) {
            loopDiagnostics.recordReadPacket(
                ffmpeg::FFmpegTranscodeLoopDiagnostics::StreamKind::Video,
                readStart
            );

            auto processStart = loopDiagnostics.mark();
            const Status videoStatus = videoPipeline.processPacket(
                inputPacket.get(),
                onVideoPacketWritten
            );
            loopDiagnostics.recordProcessPacket(
                ffmpeg::FFmpegTranscodeLoopDiagnostics::StreamKind::Video,
                processStart
            );

            av_packet_unref(inputPacket.get());

            if (!videoStatus) {
                loopDiagnostics.flush(currentLoopCounters());
                failStatus(videoStatus);
                return;
            }
        }
        else if (inputPacket->stream_index == audioStreamIndex &&
            audioPipeline.outputStream()) {
            loopDiagnostics.recordReadPacket(
                ffmpeg::FFmpegTranscodeLoopDiagnostics::StreamKind::Audio,
                readStart
            );

            auto processStart = loopDiagnostics.mark();
            const Status audioStatus = audioPipeline.processPacket(
                inputPacket.get(),
                onAudioPacketWritten
            );
            loopDiagnostics.recordProcessPacket(
                ffmpeg::FFmpegTranscodeLoopDiagnostics::StreamKind::Audio,
                processStart
            );

            av_packet_unref(inputPacket.get());

            if (!audioStatus) {
                loopDiagnostics.flush(currentLoopCounters());
                failStatus(audioStatus);
                return;
            }
        }
        else {
            loopDiagnostics.recordReadPacket(
                ffmpeg::FFmpegTranscodeLoopDiagnostics::StreamKind::Other,
                readStart
            );

            auto processStart = loopDiagnostics.mark();
            av_packet_unref(inputPacket.get());
            loopDiagnostics.recordProcessPacket(
                ffmpeg::FFmpegTranscodeLoopDiagnostics::StreamKind::Other,
                processStart
            );
        }

        loopDiagnostics.maybeLog(currentLoopCounters());
    }

    loopDiagnostics.flush(currentLoopCounters());

    const ffmpeg::FFmpegPhaseDiagnostics::Counters finalizeCountersBefore = currentFinalizeCounters();
    ffmpeg::FFmpegPhaseDiagnostics::Session finalizeDiagnostics("TRANSCODE_FINALIZE");

    auto phaseStart = finalizeDiagnostics.mark();
    auto countersBeforeStep = currentFinalizeCounters();
    if (!m_stopRequested.load()) {
        Status videoStatus = videoPipeline.flushDecoder(onVideoPacketWritten);
        auto countersAfterStep = currentFinalizeCounters();
        if (!videoStatus) {
            finalizeDiagnostics.logFailure("video_flush_decoder", phaseStart, countersBeforeStep, countersAfterStep);
            finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
            failStatus(videoStatus);
            return;
        }
        finalizeDiagnostics.logStep("video_flush_decoder", phaseStart, countersBeforeStep, countersAfterStep);
    }
    else {
        finalizeDiagnostics.logStep("video_flush_decoder_skipped", phaseStart, countersBeforeStep, currentFinalizeCounters(), "reason=stop_requested");
    }

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    if (!m_stopRequested.load() && audioPipeline.outputStream()) {
        Status audioStatus = audioPipeline.flush(onAudioPacketWritten);
        auto countersAfterStep = currentFinalizeCounters();
        if (!audioStatus) {
            finalizeDiagnostics.logFailure(
                "audio_flush",
                phaseStart,
                countersBeforeStep,
                countersAfterStep,
                std::string("audio_pipeline_mode=") + ffmpeg::audioPipelineModeName(audioStrategyPlan.mode)
            );
            finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
            failStatus(audioStatus);
            return;
        }
        finalizeDiagnostics.logStep(
            "audio_flush",
            phaseStart,
            countersBeforeStep,
            countersAfterStep,
            std::string("audio_pipeline_mode=") + ffmpeg::audioPipelineModeName(audioStrategyPlan.mode)
        );
    }
    else {
        std::string reason = m_stopRequested.load()
            ? "reason=stop_requested"
            : audioPipeline.outputStream()
                ? "reason=unknown"
                : "reason=no_audio_output_stream";
        finalizeDiagnostics.logStep(
            "audio_flush_skipped",
            phaseStart,
            countersBeforeStep,
            currentFinalizeCounters(),
            reason + ", audio_pipeline_mode=" + ffmpeg::audioPipelineModeName(audioStrategyPlan.mode)
        );
    }

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    if (!m_stopRequested.load()) {
        Status videoStatus = videoPipeline.flushFilterAndEncoder(onVideoPacketWritten);
        auto countersAfterStep = currentFinalizeCounters();
        if (!videoStatus) {
            finalizeDiagnostics.logFailure("video_flush_filter_encoder", phaseStart, countersBeforeStep, countersAfterStep);
            finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
            failStatus(videoStatus);
            return;
        }
        finalizeDiagnostics.logStep("video_flush_filter_encoder", phaseStart, countersBeforeStep, countersAfterStep);
    }
    else {
        finalizeDiagnostics.logStep("video_flush_filter_encoder_skipped", phaseStart, countersBeforeStep, currentFinalizeCounters(), "reason=stop_requested");
    }

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    ret = av_write_trailer(outputFmtCtx.get());
    auto countersAfterStep = currentFinalizeCounters();
    if (ret < 0) {
        finalizeDiagnostics.logFailure("write_trailer", phaseStart, countersBeforeStep, countersAfterStep);
        finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
        fail("av_write_trailer failed: " + ffmpeg::errorString(ret));
        return;
    }
    finalizeDiagnostics.logStep("write_trailer", phaseStart, countersBeforeStep, countersAfterStep);

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    emitProgress(m_stopRequested.load() ? "stopped" : "finished");
    finalizeDiagnostics.logStep("emit_final_progress", phaseStart, countersBeforeStep, currentFinalizeCounters());

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    audioPipeline.reset();
    finalizeDiagnostics.logStep("cleanup_audio_pipeline", phaseStart, countersBeforeStep, currentFinalizeCounters());

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    videoPipeline.reset();
    finalizeDiagnostics.logStep("cleanup_video_pipeline", phaseStart, countersBeforeStep, currentFinalizeCounters());

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    inputPacket.reset();
    finalizeDiagnostics.logStep("cleanup_input_packet", phaseStart, countersBeforeStep, currentFinalizeCounters());

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    outputFmtCtx.reset();
    finalizeDiagnostics.logStep("cleanup_output_format", phaseStart, countersBeforeStep, currentFinalizeCounters());

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    inputFmtCtx.reset();
    finalizeDiagnostics.logStep("cleanup_input_format", phaseStart, countersBeforeStep, currentFinalizeCounters());

    finalizeDiagnostics.finish(true, finalizeCountersBefore, currentFinalizeCounters());
}

void FFmpegLocalFileTranscodeEngine::setLastError(const std::string& error)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = error;
}

void FFmpegLocalFileTranscodeEngine::clearLastError()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError.clear();
}

} // namespace media
