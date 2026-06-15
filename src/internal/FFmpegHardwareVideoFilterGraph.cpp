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
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

    void freeBufferSrcParameters(AVBufferSrcParameters** params)
    {
        if (!params || !*params) {
            return;
        }

        if ((*params)->hw_frames_ctx) {
            av_buffer_unref(&(*params)->hw_frames_ctx);
        }

        av_freep(params);
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

        if (config.enableScale) {
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

        m_graph = other.m_graph;
        m_bufferSrcCtx = other.m_bufferSrcCtx;
        m_bufferSinkCtx = other.m_bufferSinkCtx;
        m_inputFrameRate = other.m_inputFrameRate;

        other.m_graph = nullptr;
        other.m_bufferSrcCtx = nullptr;
        other.m_bufferSinkCtx = nullptr;
        other.m_inputFrameRate = AVRational{ 0, 1 };

        return *this;
    }

    void HardwareVideoFilterGraph::reset()
    {
        if (m_graph) {
            avfilter_graph_free(&m_graph);
        }

        m_graph = nullptr;
        m_bufferSrcCtx = nullptr;
        m_bufferSinkCtx = nullptr;
        m_inputFrameRate = AVRational{ 0, 1 };
    }

    bool HardwareVideoFilterGraph::initialize(const Config& config, std::string* error)
    {
        reset();

        if (!config.inputStream) {
            if (error) {
                *error = "HardwareVideoFilterGraph initialize failed: inputStream is null";
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

        m_graph = avfilter_graph_alloc();
        if (!m_graph) {
            if (error) {
                *error = "avfilter_graph_alloc failed";
            }
            return false;
        }

        m_inputFrameRate = chooseInputFrameRate(config.inputStream);

        char args[512] = {};
        std::snprintf(
            args,
            sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1:frame_rate=%d/%d",
            config.inputWidth,
            config.inputHeight,
            config.inputHardwarePixelFormat,
            config.inputStream->time_base.num,
            config.inputStream->time_base.den,
            m_inputFrameRate.num,
            m_inputFrameRate.den
        );

        int ret = avfilter_graph_create_filter(
            &m_bufferSrcCtx,
            bufferSrc,
            "in",
            args,
            nullptr,
            m_graph
        );

        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_create_filter hardware buffer failed: " + errorString(ret);
            }
            reset();
            return false;
        }

        AVBufferSrcParameters* srcParams = av_buffersrc_parameters_alloc();
        if (!srcParams) {
            if (error) {
                *error = "av_buffersrc_parameters_alloc failed";
            }
            reset();
            return false;
        }

        srcParams->format = config.inputHardwarePixelFormat;
        srcParams->time_base = config.inputStream->time_base;
        srcParams->width = config.inputWidth;
        srcParams->height = config.inputHeight;
        srcParams->sample_aspect_ratio = AVRational{ 1, 1 };
        srcParams->frame_rate = m_inputFrameRate;
        srcParams->hw_frames_ctx = av_buffer_ref(config.inputHardwareFramesContext);

        if (!srcParams->hw_frames_ctx) {
            freeBufferSrcParameters(&srcParams);
            if (error) {
                *error = "av_buffer_ref input hardware frames context failed";
            }
            reset();
            return false;
        }

        ret = av_buffersrc_parameters_set(m_bufferSrcCtx, srcParams);
        freeBufferSrcParameters(&srcParams);

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
            m_graph
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

        AVFilterInOut* outputs = avfilter_inout_alloc();
        AVFilterInOut* inputs = avfilter_inout_alloc();

        if (!outputs || !inputs) {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);

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
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);

            if (error) {
                *error = "av_strdup hardware filter endpoint name failed";
            }
            reset();
            return false;
        }

        ret = avfilter_graph_parse_ptr(
            m_graph,
            filterDesc.c_str(),
            &inputs,
            &outputs,
            nullptr
        );

        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);

        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_parse_ptr hardware failed [" + filterDesc + "]: " + errorString(ret);
            }
            reset();
            return false;
        }

        ret = avfilter_graph_config(m_graph, nullptr);
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
                *error = "av_buffersrc_add_frame_flags hardware failed: " + errorString(ret);
            }
            return false;
        }

        return true;
    }

    bool HardwareVideoFilterGraph::flush(std::string* error)
    {
        if (!m_bufferSrcCtx) {
            if (error) {
                *error = "HardwareVideoFilterGraph flush failed: graph is not initialized";
            }
            return false;
        }

        const int ret = av_buffersrc_add_frame_flags(
            m_bufferSrcCtx,
            nullptr,
            0
        );

        if (ret < 0) {
            if (error) {
                *error = "av_buffersrc_add_frame_flags hardware flush failed: " + errorString(ret);
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

        if (!frame) {
            if (error) {
                *error = "HardwareVideoFilterGraph receiveFrame failed: frame is null";
            }
            return -1;
        }

        const int ret = av_buffersink_get_frame(m_bufferSinkCtx, frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }

        if (ret < 0) {
            if (error) {
                *error = "av_buffersink_get_frame hardware failed: " + errorString(ret);
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

    AVRational HardwareVideoFilterGraph::chooseInputFrameRate(const AVStream* inputStream)
    {
        if (inputStream) {
            if (inputStream->avg_frame_rate.num > 0 &&
                inputStream->avg_frame_rate.den > 0) {
                return inputStream->avg_frame_rate;
            }

            if (inputStream->r_frame_rate.num > 0 &&
                inputStream->r_frame_rate.den > 0) {
                return inputStream->r_frame_rate;
            }
        }

        return AVRational{ 25, 1 };
    }

} // namespace media::ffmpeg
