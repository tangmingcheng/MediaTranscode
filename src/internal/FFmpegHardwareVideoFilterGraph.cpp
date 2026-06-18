#include "internal/FFmpegHardwareVideoFilterGraph.h"

#include "internal/FFmpegHardwareBackend.h"
#include "internal/FFmpegUtils.h"

#include <cstdio>
#include <sstream>
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

    bool shouldUseScaleFilter(const HardwareVideoFilterGraphBuilder::Config& config)
    {
        return config.enableScale || config.enableFormatConversion;
    }

} // namespace

    std::string HardwareVideoFilterGraphBuilder::buildDescription(const Config& config,
                                                                  std::string* error)
    {
        if (config.deviceType == HardwareDeviceType::None ||
            config.deviceType == HardwareDeviceType::Auto) {
            if (error) {
                *error = "hardware filter graph build failed: invalid hardware device type";
            }
            return {};
        }

        std::ostringstream desc;
        bool hasFilter = false;

        if (shouldUseScaleFilter(config)) {
            if (!supportsHardwareScale(config.deviceType)) {
                if (error) {
                    *error = "hardware filter graph build failed: device does not have a mapped scale filter";
                }
                return {};
            }

            if (config.outputWidth <= 0 || config.outputHeight <= 0) {
                if (error) {
                    *error = "hardware filter graph build failed: invalid output size";
                }
                return {};
            }

            desc << scaleFilterName(config.deviceType)
                 << "="
                 << config.outputWidth
                 << ":"
                 << config.outputHeight;

            if (config.deviceType == HardwareDeviceType::CUDA) {
                const char* formatName = softwarePixelFormatName(config.softwareFormat);
                if (formatName && *formatName) {
                    desc << ":format=" << formatName;
                }
            }

            hasFilter = true;
        }

        if (!config.keepFramesOnDevice) {
            if (hasFilter) {
                desc << ",";
            }

            desc << "hwdownload";
            hasFilter = true;

            const char* formatName = softwarePixelFormatName(config.softwareFormat);
            if (formatName) {
                desc << ",format=pix_fmts=" << formatName;
            }
        }

        if (!hasFilter) {
            return "null";
        }

        return desc.str();
    }

    bool HardwareVideoFilterGraphBuilder::supportsHardwareScale(HardwareDeviceType deviceType)
    {
        return scaleFilterName(deviceType) != nullptr;
    }

    const char* HardwareVideoFilterGraphBuilder::scaleFilterName(HardwareDeviceType deviceType)
    {
        return HardwareBackendRegistry::scaleFilterName(deviceType);
    }

    const char* HardwareVideoFilterGraphBuilder::softwarePixelFormatName(AVPixelFormat format)
    {
        if (format == AV_PIX_FMT_NONE) {
            return nullptr;
        }

        return av_get_pix_fmt_name(format);
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

        other.m_bufferSrcCtx = nullptr;
        other.m_bufferSinkCtx = nullptr;
        other.m_inputFrameRate = AVRational{ 0, 1 };

        return *this;
    }

    void HardwareVideoFilterGraph::reset()
    {
        m_graph.reset();
        m_bufferSrcCtx = nullptr;
        m_bufferSinkCtx = nullptr;
        m_inputFrameRate = AVRational{ 0, 1 };
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

        char args[512] = {};
        std::snprintf(
            args,
            sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1:frame_rate=%d/%d",
            config.inputWidth,
            config.inputHeight,
            config.inputHardwarePixelFormat,
            config.inputTimeBase.num,
            config.inputTimeBase.den,
            m_inputFrameRate.num,
            m_inputFrameRate.den
        );

        int ret = avfilter_graph_create_filter(
            &m_bufferSrcCtx,
            bufferSrc,
            "in",
            args,
            nullptr,
            m_graph.get()
        );

        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_create_filter hardware buffer failed: " + errorString(ret);
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
        srcParams->sample_aspect_ratio = AVRational{ 1, 1 };
        srcParams->frame_rate = m_inputFrameRate;
        srcParams->hw_frames_ctx = av_buffer_ref(config.inputHardwareFramesContext);
        if (!srcParams->hw_frames_ctx) {
            if (error) {
                *error = "av_buffer_ref input hardware frames context failed";
            }
            reset();
            return false;
        }

        ret = av_buffersrc_parameters_set(m_bufferSrcCtx, srcParams.get());

        if (ret < 0) {
            if (error) {
                *error = "av_buffersrc_parameters_set hardware buffer failed: " + errorString(ret);
            }
            reset();
            return false;
        }

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

        HardwareVideoFilterGraphBuilder::Config builderConfig;
        builderConfig.deviceType = config.deviceType;
        builderConfig.outputWidth = config.outputWidth;
        builderConfig.outputHeight = config.outputHeight;
        builderConfig.softwareFormat = config.softwarePixelFormat;
        builderConfig.enableScale = config.enableScale;
        builderConfig.enableFormatConversion = config.enableFormatConversion;
        builderConfig.keepFramesOnDevice = config.keepFramesOnDevice;

        std::string builderError;
        const std::string filterDesc = HardwareVideoFilterGraphBuilder::buildDescription(
            builderConfig,
            &builderError
        );

        if (filterDesc.empty()) {
            if (error) {
                *error = builderError.empty()
                    ? "build hardware video filter description failed"
                    : builderError;
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
            filterDesc.c_str(),
            &inputsRaw,
            &outputsRaw,
            nullptr
        );
        inputs.reset(inputsRaw);
        outputs.reset(outputsRaw);

        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_parse_ptr hardware failed [" + filterDesc + "]: " + errorString(ret);
            }
            reset();
            return false;
        }

        ret = avfilter_graph_config(m_graph.get(), nullptr);
        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_config hardware failed [" + filterDesc + "]: " + errorString(ret);
            }
            reset();
            return false;
        }

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

} // namespace media::ffmpeg
