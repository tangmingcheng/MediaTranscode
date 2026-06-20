#include "internal/FFmpegHardwareVideoFilterGraph.h"

#include "internal/FFmpegBufferSourceMetadataApplier.h"
#include "internal/FFmpegHardwareBackend.h"
#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

#include <utility>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avstring.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    bool isValidRatio(AVRational ratio)
    {
        return ratio.num > 0 && ratio.den > 0;
    }

    bool hasResolvedBackend(const HardwareBackendProfile& backend)
    {
        return backend.deviceType != HardwareDeviceType::None &&
            backend.deviceType != HardwareDeviceType::Auto;
    }

} // namespace

    std::string HardwareVideoFilterGraphBuilder::buildDescription(const Config& config,
                                                                  std::string* error)
    {
        HardwareVideoFilterRequest request;
        request.backend = hasResolvedBackend(config.backend)
            ? config.backend
            : HardwareBackendRegistry::profileFor(config.deviceType);
        request.outputWidth = config.outputWidth;
        request.outputHeight = config.outputHeight;
        request.softwareFormat = config.softwareFormat;
        request.outputFps = config.outputFps;
        request.enableConstantFps = config.enableConstantFps;
        request.enableScale = config.enableScale;
        request.enableFormatConversion = config.enableFormatConversion;
        request.keepFramesOnDevice = config.keepFramesOnDevice;

        const HardwareVideoFilterPlan plan = HardwareVideoFilterPipelinePlanner::build(
            request,
            error
        );

        return plan.description;
    }

    bool HardwareVideoFilterGraphBuilder::supportsHardwareScale(HardwareDeviceType deviceType)
    {
        return HardwareVideoFilterPipelinePlanner::supportsHardwareScale(
            HardwareBackendRegistry::profileFor(deviceType)
        );
    }

    const char* HardwareVideoFilterGraphBuilder::scaleFilterName(HardwareDeviceType deviceType)
    {
        return HardwareBackendRegistry::scaleFilterName(deviceType);
    }

    const char* HardwareVideoFilterGraphBuilder::softwarePixelFormatName(AVPixelFormat format)
    {
        return HardwareVideoFilterPipelinePlanner::softwarePixelFormatName(format);
    }

    HardwareVideoFilterGraph::~HardwareVideoFilterGraph()
    {
        reset();
    }

    HardwareVideoFilterGraph::HardwareVideoFilterGraph(HardwareVideoFilterGraph&& other) noexcept
    {
        *this = std::move(other);
    }

    HardwareVideoFilterGraph& HardwareVideoFilterGraph::operator=(HardwareVideoFilterGraph&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        reset();

        m_graph = std::move(other.m_graph);
        m_bufferSrcCtx = other.m_bufferSrcCtx;
        m_bufferSinkCtx = other.m_bufferSinkCtx;
        m_inputFrameRate = other.m_inputFrameRate;
        m_colorMetadata = other.m_colorMetadata;

        other.m_bufferSrcCtx = nullptr;
        other.m_bufferSinkCtx = nullptr;
        other.m_inputFrameRate = AVRational{ 0, 1 };
        other.m_colorMetadata = VideoColorMetadata{};

        return *this;
    }

    void HardwareVideoFilterGraph::reset()
    {
        m_graph.reset();
        m_bufferSrcCtx = nullptr;
        m_bufferSinkCtx = nullptr;
        m_inputFrameRate = AVRational{ 0, 1 };
        m_colorMetadata = VideoColorMetadata{};
    }

    bool HardwareVideoFilterGraph::initialize(const Config& config, std::string* error)
    {
        reset();

        if (!isValidRatio(config.inputTimeBase)) {
            if (error) {
                *error = "HardwareVideoFilterGraph initialize failed: input time base is invalid";
            }
            return false;
        }

        if (!config.inputHardwareFramesContext) {
            if (error) {
                *error = "HardwareVideoFilterGraph initialize failed: input hardware frames context is null";
            }
            return false;
        }

        if (config.inputHardwarePixelFormat == AV_PIX_FMT_NONE) {
            if (error) {
                *error = "HardwareVideoFilterGraph initialize failed: invalid hardware pixel format";
            }
            return false;
        }

        if (config.inputWidth <= 0 || config.inputHeight <= 0) {
            if (error) {
                *error = "HardwareVideoFilterGraph initialize failed: invalid input size";
            }
            return false;
        }

        const HardwareBackendProfile backend = resolveBackend(
            config.backend,
            config.deviceType
        );

        HardwareVideoFilterRequest filterRequest;
        filterRequest.backend = backend;
        filterRequest.outputWidth = config.outputWidth;
        filterRequest.outputHeight = config.outputHeight;
        filterRequest.softwareFormat = config.softwarePixelFormat;
        filterRequest.outputFps = config.outputFps;
        filterRequest.enableConstantFps = config.enableConstantFps;
        filterRequest.enableScale = config.enableScale;
        filterRequest.enableFormatConversion = config.enableFormatConversion;
        filterRequest.keepFramesOnDevice = config.keepFramesOnDevice;

        std::string planError;
        const HardwareVideoFilterPlan filterPlan = HardwareVideoFilterPipelinePlanner::build(
            filterRequest,
            &planError
        );

        if (filterPlan.description.empty()) {
            if (error) {
                *error = planError.empty()
                    ? "build hardware video filter plan failed"
                    : planError;
            }
            reset();
            return false;
        }

        const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
        const AVFilter* bufferSink = avfilter_get_by_name("buffersink");

        if (!bufferSrc || !bufferSink) {
            if (error) {
                *error = "avfilter_get_by_name buffer/buffersink failed";
            }
            return false;
        }

        m_graph = makeFilterGraph();
        if (!m_graph) {
            if (error) {
                *error = "avfilter_graph_alloc failed";
            }
            return false;
        }

        m_inputFrameRate = chooseInputFrameRate(config.inputFrameRate);
        m_colorMetadata = config.colorMetadata;
        m_colorMetadata.sampleAspectRatio = VideoColorMetadataUtils::sanitizeSampleAspectRatio(
            m_colorMetadata.sampleAspectRatio
        );

        AVFilterContext* bufferSrcCtx = avfilter_graph_alloc_filter(
            m_graph.get(),
            bufferSrc,
            "in"
        );

        if (!bufferSrcCtx) {
            if (error) {
                *error = "avfilter_graph_alloc_filter hardware buffer failed";
            }
            reset();
            return false;
        }

        BufferSrcParametersPtr srcParams = makeBufferSrcParameters();
        if (!srcParams) {
            if (error) {
                *error = "av_buffersrc_parameters_alloc failed";
            }
            reset();
            return false;
        }

        srcParams->format = config.inputHardwarePixelFormat;
        srcParams->time_base = config.inputTimeBase;
        srcParams->width = config.inputWidth;
        srcParams->height = config.inputHeight;
        srcParams->sample_aspect_ratio = m_colorMetadata.sampleAspectRatio;
        srcParams->frame_rate = m_inputFrameRate;
        srcParams->hw_frames_ctx = av_buffer_ref(config.inputHardwareFramesContext);
        if (!srcParams->hw_frames_ctx) {
            if (error) {
                *error = "av_buffer_ref input hardware frames context failed";
            }
            reset();
            return false;
        }

        int ret = av_buffersrc_parameters_set(bufferSrcCtx, srcParams.get());
        if (ret < 0) {
            if (error) {
                *error = "av_buffersrc_parameters_set hardware buffer failed: " + errorString(ret);
            }
            reset();
            return false;
        }

        const BufferSourceMetadataApplyReport metadataApplyReport =
            BufferSourceMetadataApplier::apply(bufferSrcCtx, m_colorMetadata);

        ret = avfilter_init_str(bufferSrcCtx, nullptr);
        if (ret < 0) {
            if (error) {
                *error = "avfilter_init_str hardware buffer failed: " + errorString(ret);
            }
            reset();
            return false;
        }

        m_bufferSrcCtx = bufferSrcCtx;

        ret = avfilter_graph_create_filter(
            &m_bufferSinkCtx,
            bufferSink,
            "out",
            nullptr,
            nullptr,
            m_graph.get()
        );

        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_create_filter hardware buffersink failed: " + errorString(ret);
            }
            reset();
            return false;
        }

        FilterInOutPtr outputs = makeFilterInOut();
        FilterInOutPtr inputs = makeFilterInOut();

        if (!outputs || !inputs) {
            if (error) {
                *error = "avfilter_inout_alloc failed";
            }
            reset();
            return false;
        }

        outputs->name = av_strdup("in");
        outputs->filter_ctx = m_bufferSrcCtx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = m_bufferSinkCtx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        if (!outputs->name || !inputs->name) {
            if (error) {
                *error = "av_strdup hardware filter endpoint name failed";
            }
            reset();
            return false;
        }

        AVFilterInOut* inputsRaw = inputs.release();
        AVFilterInOut* outputsRaw = outputs.release();
        ret = avfilter_graph_parse_ptr(
            m_graph.get(),
            filterPlan.description.c_str(),
            &inputsRaw,
            &outputsRaw,
            nullptr
        );
        inputs.reset(inputsRaw);
        outputs.reset(outputsRaw);

        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_parse_ptr hardware failed [" +
                    filterPlan.description + "]: " + errorString(ret);
            }
            reset();
            return false;
        }

        ret = avfilter_graph_config(m_graph.get(), nullptr);
        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_config hardware failed [" +
                    filterPlan.description + "]: " + errorString(ret);
            }
            reset();
            return false;
        }

        spdlog::info(
            "[ZC][FILTER] hardware plan backend={}, desc={}, keep_on_device={}, fps={}, scale={}, hwdownload={}, format={}, color_metadata={}, color_options_applied={}",
            backend.name ? backend.name : "unknown",
            filterPlan.description,
            filterPlan.keepsFramesOnDevice,
            filterPlan.hasFrameRateFilter,
            filterPlan.hasHardwareScale,
            filterPlan.downloadsToSoftware,
            filterPlan.hasSoftwareFormat,
            VideoColorMetadataUtils::describe(m_colorMetadata),
            metadataApplyReport.anyApplied()
        );

        return true;
    }

    bool HardwareVideoFilterGraph::sendFrame(AVFrame* frame, std::string* error)
    {
        if (!m_bufferSrcCtx) {
            if (error) {
                *error = "HardwareVideoFilterGraph sendFrame failed: graph is not initialized";
            }
            return false;
        }

        const int ret = av_buffersrc_add_frame_flags(
            m_bufferSrcCtx,
            frame,
            AV_BUFFERSRC_FLAG_KEEP_REF
        );

        if (ret < 0) {
            if (error) {
                *error = "av_buffersrc_add_frame_flags hardware video failed: " + errorString(ret);
            }
            return false;
        }

        return true;
    }

    bool HardwareVideoFilterGraph::flush(std::string* error)
    {
        if (!m_bufferSrcCtx) {
            return true;
        }

        const int ret = av_buffersrc_add_frame_flags(
            m_bufferSrcCtx,
            nullptr,
            0
        );

        if (ret < 0) {
            if (error) {
                *error = "av_buffersrc_add_frame_flags hardware video flush failed: " + errorString(ret);
            }
            return false;
        }

        return true;
    }

    int HardwareVideoFilterGraph::receiveFrame(AVFrame* frame, std::string* error)
    {
        if (!m_bufferSinkCtx) {
            if (error) {
                *error = "HardwareVideoFilterGraph receiveFrame failed: graph is not initialized";
            }
            return -1;
        }

        const int ret = av_buffersink_get_frame(m_bufferSinkCtx, frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }

        if (ret < 0) {
            if (error) {
                *error = "av_buffersink_get_frame hardware video failed: " + errorString(ret);
            }
            return -1;
        }

        VideoColorMetadataUtils::applyMissingToFrame(frame, m_colorMetadata);
        return 1;
    }

    bool HardwareVideoFilterGraph::isInitialized() const
    {
        return m_graph && m_bufferSrcCtx && m_bufferSinkCtx;
    }

    AVRational HardwareVideoFilterGraph::sinkTimeBase() const
    {
        if (!m_bufferSinkCtx) {
            return AVRational{ 0, 1 };
        }

        return av_buffersink_get_time_base(m_bufferSinkCtx);
    }

    AVRational HardwareVideoFilterGraph::inputFrameRate() const
    {
        return m_inputFrameRate;
    }

    AVRational HardwareVideoFilterGraph::chooseInputFrameRate(AVRational inputFrameRate)
    {
        if (isValidRatio(inputFrameRate)) {
            const double fps = av_q2d(inputFrameRate);

            if (fps > 1.0 && fps < 240.0) {
                return inputFrameRate;
            }
        }

        return AVRational{ 25, 1 };
    }

    HardwareBackendProfile HardwareVideoFilterGraph::resolveBackend(
        const HardwareBackendProfile& backend,
        HardwareDeviceType fallbackDeviceType)
    {
        if (hasResolvedBackend(backend)) {
            return backend;
        }

        return HardwareBackendRegistry::profileFor(fallbackDeviceType);
    }

} // namespace media::ffmpeg
