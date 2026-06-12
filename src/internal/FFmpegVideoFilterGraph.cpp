#include "internal/FFmpegVideoFilterGraph.h"

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

    VideoFilterGraph::~VideoFilterGraph()
    {
        reset();
    }

    VideoFilterGraph::VideoFilterGraph(VideoFilterGraph&& other) noexcept
    {
        *this = std::move(other);
    }

    VideoFilterGraph& VideoFilterGraph::operator=(VideoFilterGraph&& other) noexcept
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

    void VideoFilterGraph::reset()
    {
        if (m_graph) {
            avfilter_graph_free(&m_graph);
        }

        m_graph = nullptr;
        m_bufferSrcCtx = nullptr;
        m_bufferSinkCtx = nullptr;
        m_inputFrameRate = AVRational{ 0, 1 };
    }

    bool VideoFilterGraph::initialize(const Config& config, std::string* error)
    {
        reset();

        if (!config.decoderCtx) {
            if (error) {
                *error = "VideoFilterGraph initialize failed: decoderCtx is null";
            }
            return false;
        }

        if (!config.encoderCtx) {
            if (error) {
                *error = "VideoFilterGraph initialize failed: encoderCtx is null";
            }
            return false;
        }

        if (!config.inputStream) {
            if (error) {
                *error = "VideoFilterGraph initialize failed: inputStream is null";
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

        m_inputFrameRate = chooseInputFrameRate(config.inputStream, config.outputFps);

        const AVRational pixelAspect =
            config.decoderCtx->sample_aspect_ratio.num > 0 &&
            config.decoderCtx->sample_aspect_ratio.den > 0
            ? config.decoderCtx->sample_aspect_ratio
            : AVRational{ 1, 1 };

        char args[512] = {};
        std::snprintf(
            args,
            sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
            config.decoderCtx->width,
            config.decoderCtx->height,
            config.decoderCtx->pix_fmt,
            config.inputStream->time_base.num,
            config.inputStream->time_base.den,
            pixelAspect.num,
            pixelAspect.den,
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
                *error = "avfilter_graph_create_filter buffer failed: " + errorString(ret);
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
                *error = "avfilter_graph_create_filter buffersink failed: " + errorString(ret);
            }
            reset();
            return false;
        }

        const std::string filterDesc = buildDescription(
            config.encoderCtx,
            config.outputFps,
            config.enableConstantFps
        );

        if (filterDesc.empty()) {
            if (error) {
                *error = "build video filter description failed: invalid encoder pixel format";
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
                *error = "av_strdup filter endpoint name failed";
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
                *error = "avfilter_graph_parse_ptr failed [" + filterDesc + "]: " + errorString(ret);
            }
            reset();
            return false;
        }

        ret = avfilter_graph_config(m_graph, nullptr);
        if (ret < 0) {
            if (error) {
                *error = "avfilter_graph_config failed [" + filterDesc + "]: " + errorString(ret);
            }
            reset();
            return false;
        }

        return true;
    }

    bool VideoFilterGraph::sendFrame(AVFrame* frame, std::string* error)
    {
        if (!m_bufferSrcCtx) {
            if (error) {
                *error = "VideoFilterGraph sendFrame failed: graph is not initialized";
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
                *error = "av_buffersrc_add_frame_flags failed: " + errorString(ret);
            }
            return false;
        }

        return true;
    }

    bool VideoFilterGraph::flush(std::string* error)
    {
        if (!m_bufferSrcCtx) {
            if (error) {
                *error = "VideoFilterGraph flush failed: graph is not initialized";
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
                *error = "av_buffersrc_add_frame_flags flush failed: " + errorString(ret);
            }
            return false;
        }

        return true;
    }

    int VideoFilterGraph::receiveFrame(AVFrame* frame, std::string* error)
    {
        if (!m_bufferSinkCtx) {
            if (error) {
                *error = "VideoFilterGraph receiveFrame failed: graph is not initialized";
            }
            return -1;
        }

        if (!frame) {
            if (error) {
                *error = "VideoFilterGraph receiveFrame failed: frame is null";
            }
            return -1;
        }

        const int ret = av_buffersink_get_frame(m_bufferSinkCtx, frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }

        if (ret < 0) {
            if (error) {
                *error = "av_buffersink_get_frame failed: " + errorString(ret);
            }
            return -1;
        }

        return 1;
    }

    bool VideoFilterGraph::isInitialized() const
    {
        return m_graph && m_bufferSrcCtx && m_bufferSinkCtx;
    }

    AVRational VideoFilterGraph::sinkTimeBase() const
    {
        if (!m_bufferSinkCtx) {
            return AVRational{ 0, 1 };
        }

        return av_buffersink_get_time_base(m_bufferSinkCtx);
    }

    AVRational VideoFilterGraph::inputFrameRate() const
    {
        return m_inputFrameRate;
    }

    AVRational VideoFilterGraph::chooseInputFrameRate(const AVStream* inputStream, int outputFps)
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

        return AVRational{ outputFps > 0 ? outputFps : 25, 1 };
    }

    std::string VideoFilterGraph::buildDescription(const AVCodecContext* encoderCtx,
                                                   int outputFps,
                                                   bool enableConstantFps)
    {
        if (!encoderCtx) {
            return {};
        }

        const char* pixFmtName = av_get_pix_fmt_name(encoderCtx->pix_fmt);
        if (!pixFmtName) {
            return {};
        }

        std::ostringstream desc;

        desc << "scale="
            << encoderCtx->width
            << ":"
            << encoderCtx->height
            << ":flags=bicubic";

        if (enableConstantFps) {
            desc << ",fps=fps=" << (outputFps > 0 ? outputFps : 25) << ":round=near";
        }

        desc << ",format=pix_fmts=" << pixFmtName;

        return desc.str();
    }

} // namespace media::ffmpeg
