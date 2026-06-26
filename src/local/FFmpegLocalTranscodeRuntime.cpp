#include "local/FFmpegLocalTranscodeRuntime.h"

#include "internal/FFmpegAudioPipeline.h"
#include "internal/FFmpegAudioStrategyPlanner.h"
#include "internal/FFmpegError.h"
#include "internal/FFmpegPhaseDiagnostics.h"
#include "internal/FFmpegPipelinePlanner.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegTranscodeLoopDiagnostics.h"
#include "internal/FFmpegUtils.h"
#include "internal/core/video/FFmpegVideoProcessingPipeline.h"
#include "internal/output/sessions/file/FFmpegFileOutputSession.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace media::ffmpeg {
namespace {

const char* executionModeName(VideoExecutionMode mode)
{
    switch (mode) {
    case VideoExecutionMode::HardwareZeroCopy:
        return "hardware-zero-copy";
    case VideoExecutionMode::HardwareDecodeSoftwareFilterHardwareEncode:
        return "hardware-decode-software-filter-hardware-encode";
    case VideoExecutionMode::HardwareDecodeSoftwareFilterGenericEncode:
        return "hardware-decode-software-filter-generic-encode";
    case VideoExecutionMode::Cpu:
    default:
        return "cpu";
    }
}

Status makeRuntimeError(std::string message)
{
    return Status::failure(ErrorInfo::internalError(std::move(message)));
}

} // namespace

Status FFmpegLocalTranscodeRuntime::run(Config config)
{
    const TranscodeConfig& transcodeConfig = config.transcodeConfig;

    auto stopRequested = [&]() {
        return config.stopRequested && config.stopRequested->load();
    };

    InputFormatContextPtr inputFmtCtx;
    PacketPtr inputPacket;

    int videoStreamIndex = -1;
    int audioStreamIndex = -1;

    AVStream* inputVideoStream = nullptr;
    AVStream* inputAudioStream = nullptr;

    TimelineNormalizer timeline;
    FFmpegFileOutputSession outputSession;
    FFmpegVideoProcessingPipeline videoPipeline;
    FFmpegAudioPipeline audioPipeline;
    FFmpegTranscodeLoopDiagnostics loopDiagnostics(1000);
    FFmpegAudioStrategyPlanner::Plan audioStrategyPlan;

    int64_t encodedVideoPacketCount = 0;
    int64_t encodedAudioPacketCount = 0;
    int64_t lastWrittenVideoOutTimeMs = 0;
    int64_t lastWrittenAudioOutTimeMs = 0;
    int64_t progressCallbackCount = 0;

    const auto startTime = std::chrono::steady_clock::now();

    auto currentFinalizeCounters = [&]() {
        return FFmpegPhaseDiagnostics::Counters{
            encodedVideoPacketCount,
            encodedAudioPacketCount,
            progressCallbackCount
        };
    };

    auto currentLoopCounters = [&]() {
        return FFmpegTranscodeLoopDiagnostics::OutputCounters{
            encodedVideoPacketCount,
            encodedAudioPacketCount,
            progressCallbackCount,
            std::max(lastWrittenVideoOutTimeMs, lastWrittenAudioOutTimeMs)
        };
    };

    auto emitProgress = [&](const std::string& raw) {
        if (!config.progressCallback) {
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
        config.progressCallback(info);
    };

    emitProgress("initialized");

    AVFormatContext* rawInputFmtCtx = nullptr;
    int ret = avformat_open_input(&rawInputFmtCtx, transcodeConfig.inputUrl.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError("avformat_open_input failed", ret));
    }
    inputFmtCtx.reset(rawInputFmtCtx);

    ret = avformat_find_stream_info(inputFmtCtx.get(), nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError("avformat_find_stream_info failed", ret));
    }

    ret = av_find_best_stream(inputFmtCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        return Status::failure(makeFFmpegError("av_find_best_stream video failed", ret));
    }

    videoStreamIndex = ret;
    inputVideoStream = inputFmtCtx->streams[videoStreamIndex];

    if (transcodeConfig.audioEnabled) {
        ret = av_find_best_stream(inputFmtCtx.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (ret >= 0) {
            audioStreamIndex = ret;
            inputAudioStream = inputFmtCtx->streams[audioStreamIndex];
        }
    }

    timeline.initStartFromFormat(inputFmtCtx.get(), inputVideoStream, inputAudioStream);

    FFmpegFileOutputSession::Config outputConfig;
    outputConfig.outputUrl = transcodeConfig.outputUrl;

    Status outputStatus = outputSession.initialize(std::move(outputConfig));
    if (!outputStatus) {
        return outputStatus;
    }

    audioStrategyPlan = FFmpegAudioStrategyPlanner::plan(
        transcodeConfig,
        inputAudioStream,
        outputSession.context()
    );
    spdlog::info(
        "[AUDIO][PLAN] enabled={}, selected_mode={}, requested_codec={}, selected_codec={}, target_bitrate_kbps={}, smart_copy={}, {}",
        transcodeConfig.audioEnabled,
        audioPipelineModeName(audioStrategyPlan.mode),
        FFmpegAudioStrategyPlanner::audioCodecName(transcodeConfig.audioCodec),
        FFmpegAudioStrategyPlanner::audioCodecName(audioStrategyPlan.codec),
        audioStrategyPlan.audioBitrateKbps,
        audioStrategyPlan.smartCopy,
        audioStrategyPlan.diagnostic
    );

    const AVCodec* decoder = avcodec_find_decoder(inputVideoStream->codecpar->codec_id);
    HardwarePipelinePlan plan;

    if (transcodeConfig.hardware.enabled) {
        plan = FFmpegPipelinePlanner::planHardwarePipeline(
            transcodeConfig,
            decoder
        );
    }
    else {
        plan.executionMode = VideoExecutionMode::Cpu;
        plan.diagnostic = "hardware disabled by config; using CPU pipeline";
        spdlog::warn("[PLAN] {}", plan.diagnostic);
    }

    const HardwarePipelinePlan* executionPlan = nullptr;
    if (plan.valid && plan.executionMode != VideoExecutionMode::Cpu) {
        executionPlan = &plan;
        if (plan.executionMode == VideoExecutionMode::HardwareZeroCopy) {
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
        if (transcodeConfig.hardware.enabled && !transcodeConfig.hardware.allowZeroCopyFallback) {
            return makeRuntimeError(plan.diagnostic.empty()
                ? std::string("zero-copy pipeline planning failed and fallback is disabled")
                : plan.diagnostic);
        }

        spdlog::warn(
            "[PLAN] execution mode: cpu-frame-pipeline fallback: {}",
            plan.diagnostic
        );
    }

    {
        FFmpegVideoProcessingPipeline::Config videoConfig;
        videoConfig.transcodeConfig = &transcodeConfig;
        videoConfig.hardwarePlan = executionPlan;
        videoConfig.inputVideoStream = inputVideoStream;
        videoConfig.outputStreamProvider = outputSession.videoStreamProvider();
        videoConfig.outputNode = outputSession.outputNode();
        videoConfig.timeline = &timeline;

        Status videoStatus = videoPipeline.initialize(videoConfig);
        if (!videoStatus) {
            return videoStatus;
        }
    }

    if (inputAudioStream && audioStrategyPlan.mode != FFmpegAudioPipelineMode::None) {
        FFmpegAudioPipeline::Config audioConfig;
        audioConfig.mode = audioStrategyPlan.mode;
        audioConfig.codec = audioStrategyPlan.codec;
        audioConfig.inputAudioStream = inputAudioStream;
        audioConfig.outputStreamProvider = outputSession.audioStreamProvider();
        audioConfig.outputNode = outputSession.outputNode();
        audioConfig.timeline = &timeline;
        audioConfig.audioBitrateKbps = audioStrategyPlan.audioBitrateKbps;

        Status audioStatus = audioPipeline.initialize(audioConfig);
        if (!audioStatus) {
            return audioStatus;
        }
    }

    outputStatus = outputSession.openIo();
    if (!outputStatus) {
        return outputStatus;
    }

    outputStatus = outputSession.writeHeader();
    if (!outputStatus) {
        return outputStatus;
    }

    inputPacket = makePacket();
    if (!inputPacket) {
        return Status::failure(makeAllocationError("av_packet_alloc input packet failed"));
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

    while (!stopRequested()) {
        auto readStart = loopDiagnostics.mark();
        ret = av_read_frame(inputFmtCtx.get(), inputPacket.get());

        if (ret == AVERROR_EOF) {
            break;
        }

        if (ret < 0) {
            loopDiagnostics.flush(currentLoopCounters());
            return Status::failure(makeFFmpegError("av_read_frame failed", ret));
        }

        if (inputPacket->stream_index == videoStreamIndex) {
            loopDiagnostics.recordReadPacket(
                FFmpegTranscodeLoopDiagnostics::StreamKind::Video,
                readStart
            );

            auto processStart = loopDiagnostics.mark();
            const Status videoStatus = videoPipeline.processPacket(
                inputPacket.get(),
                onVideoPacketWritten
            );
            loopDiagnostics.recordProcessPacket(
                FFmpegTranscodeLoopDiagnostics::StreamKind::Video,
                processStart
            );

            av_packet_unref(inputPacket.get());

            if (!videoStatus) {
                loopDiagnostics.flush(currentLoopCounters());
                return videoStatus;
            }
        }
        else if (inputPacket->stream_index == audioStreamIndex &&
            audioPipeline.outputStream()) {
            loopDiagnostics.recordReadPacket(
                FFmpegTranscodeLoopDiagnostics::StreamKind::Audio,
                readStart
            );

            auto processStart = loopDiagnostics.mark();
            const Status audioStatus = audioPipeline.processPacket(
                inputPacket.get(),
                onAudioPacketWritten
            );
            loopDiagnostics.recordProcessPacket(
                FFmpegTranscodeLoopDiagnostics::StreamKind::Audio,
                processStart
            );

            av_packet_unref(inputPacket.get());

            if (!audioStatus) {
                loopDiagnostics.flush(currentLoopCounters());
                return audioStatus;
            }
        }
        else {
            loopDiagnostics.recordReadPacket(
                FFmpegTranscodeLoopDiagnostics::StreamKind::Other,
                readStart
            );

            auto processStart = loopDiagnostics.mark();
            av_packet_unref(inputPacket.get());
            loopDiagnostics.recordProcessPacket(
                FFmpegTranscodeLoopDiagnostics::StreamKind::Other,
                processStart
            );
        }

        loopDiagnostics.maybeLog(currentLoopCounters());
    }

    loopDiagnostics.flush(currentLoopCounters());

    const FFmpegPhaseDiagnostics::Counters finalizeCountersBefore = currentFinalizeCounters();
    FFmpegPhaseDiagnostics::Session finalizeDiagnostics("TRANSCODE_FINALIZE");

    auto phaseStart = finalizeDiagnostics.mark();
    auto countersBeforeStep = currentFinalizeCounters();
    if (!stopRequested()) {
        Status videoStatus = videoPipeline.flushDecoder(onVideoPacketWritten);
        auto countersAfterStep = currentFinalizeCounters();
        if (!videoStatus) {
            finalizeDiagnostics.logFailure("video_flush_decoder", phaseStart, countersBeforeStep, countersAfterStep);
            finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
            return videoStatus;
        }
        finalizeDiagnostics.logStep("video_flush_decoder", phaseStart, countersBeforeStep, countersAfterStep);
    }
    else {
        finalizeDiagnostics.logStep("video_flush_decoder_skipped", phaseStart, countersBeforeStep, currentFinalizeCounters(), "reason=stop_requested");
    }

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    if (!stopRequested() && audioPipeline.outputStream()) {
        Status audioStatus = audioPipeline.flush(onAudioPacketWritten);
        auto countersAfterStep = currentFinalizeCounters();
        if (!audioStatus) {
            finalizeDiagnostics.logFailure(
                "audio_flush",
                phaseStart,
                countersBeforeStep,
                countersAfterStep,
                std::string("audio_pipeline_mode=") + audioPipelineModeName(audioStrategyPlan.mode)
            );
            finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
            return audioStatus;
        }
        finalizeDiagnostics.logStep(
            "audio_flush",
            phaseStart,
            countersBeforeStep,
            countersAfterStep,
            std::string("audio_pipeline_mode=") + audioPipelineModeName(audioStrategyPlan.mode)
        );
    }
    else {
        std::string reason = stopRequested()
            ? "reason=stop_requested"
            : audioPipeline.outputStream()
                ? "reason=unknown"
                : "reason=no_audio_output_stream";
        finalizeDiagnostics.logStep(
            "audio_flush_skipped",
            phaseStart,
            countersBeforeStep,
            currentFinalizeCounters(),
            reason + ", audio_pipeline_mode=" + audioPipelineModeName(audioStrategyPlan.mode)
        );
    }

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    if (!stopRequested()) {
        Status videoStatus = videoPipeline.flushFilterAndEncoder(onVideoPacketWritten);
        auto countersAfterStep = currentFinalizeCounters();
        if (!videoStatus) {
            finalizeDiagnostics.logFailure("video_flush_filter_encoder", phaseStart, countersBeforeStep, countersAfterStep);
            finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
            return videoStatus;
        }
        finalizeDiagnostics.logStep("video_flush_filter_encoder", phaseStart, countersBeforeStep, countersAfterStep);
    }
    else {
        finalizeDiagnostics.logStep("video_flush_filter_encoder_skipped", phaseStart, countersBeforeStep, currentFinalizeCounters(), "reason=stop_requested");
    }

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    outputStatus = outputSession.writeTrailer();
    auto countersAfterStep = currentFinalizeCounters();
    if (!outputStatus) {
        finalizeDiagnostics.logFailure("write_trailer", phaseStart, countersBeforeStep, countersAfterStep);
        finalizeDiagnostics.finish(false, finalizeCountersBefore, countersAfterStep);
        return outputStatus;
    }
    finalizeDiagnostics.logStep("write_trailer", phaseStart, countersBeforeStep, countersAfterStep);

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    emitProgress(stopRequested() ? "stopped" : "finished");
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
    outputSession.reset();
    finalizeDiagnostics.logStep("cleanup_output_session", phaseStart, countersBeforeStep, currentFinalizeCounters());

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    inputPacket.reset();
    finalizeDiagnostics.logStep("cleanup_input_packet", phaseStart, countersBeforeStep, currentFinalizeCounters());

    phaseStart = finalizeDiagnostics.mark();
    countersBeforeStep = currentFinalizeCounters();
    inputFmtCtx.reset();
    finalizeDiagnostics.logStep("cleanup_input_format", phaseStart, countersBeforeStep, currentFinalizeCounters());

    finalizeDiagnostics.finish(true, finalizeCountersBefore, currentFinalizeCounters());
    return Status::success();
}

} // namespace media::ffmpeg
