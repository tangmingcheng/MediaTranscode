#include "internal/graph/nodes/mux/FileMuxNode.h"

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<bool> requiredBool(const MediaNodeOptions* options, const char* key)
{
    if (!options || !options->has(key)) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(std::string("FileMuxNode requires ") + key));
    }
    const std::string value = options->value(key);
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return ::media::Result<bool>::success(true);
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return ::media::Result<bool>::success(false);
    }
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::invalidArgument(std::string("FileMuxNode invalid ") + key));
}

} // namespace

FileMuxNode::FileMuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
{
}

MediaNodeKind FileMuxNode::staticKind() noexcept
{
    return MediaNodeKind::FileMux;
}

::media::Status FileMuxNode::onProcess(MediaGraphExecutionContext& context)
{
    auto expected = bindMuxExpectations(context);
    if (!expected) {
        return expected;
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = *input.value();
    if (m_writer.bindOutputContext(buffer)) {
        auto status = m_writer.registerPendingStreamConfigs();
        return status ? m_writer.writePendingPacketsIfReady() : status;
    }

    auto configStatus = m_writer.tryBindStreamConfig(buffer);
    if (!configStatus) {
        return configStatus;
    }

    if (buffer->isEof() || buffer->isFlush()) {
        return forwardIfOutputsExist(context, buffer);
    }

    if (FFmpegPacketView::isPacket(buffer)) {
        auto status = m_writer.writePacket(buffer);
        if (!status) {
            return status;
        }
    }

    return forwardIfOutputsExist(context, buffer);
}

::media::Status FileMuxNode::flush(MediaGraphExecutionContext& context)
{
    auto pending = m_writer.writePendingPacketsIfReady();
    if (!pending) {
        return pending;
    }
    auto trailer = m_writer.writeTrailerIfNeeded();
    return trailer ? FFmpegNodeRuntime::flush(context) : trailer;
}

::media::Status FileMuxNode::stop(MediaGraphExecutionContext& context)
{
    auto pending = m_writer.writePendingPacketsIfReady();
    if (!pending) {
        releaseRuntimeViews();
        return pending;
    }
    auto trailer = m_writer.writeTrailerIfNeeded();
    if (!trailer) {
        releaseRuntimeViews();
        return trailer;
    }
    auto stopped = FFmpegNodeRuntime::stop(context);
    releaseRuntimeViews();
    return stopped;
}

::media::Status FileMuxNode::bindMuxExpectations(MediaGraphExecutionContext& context)
{
    if (m_expectationsBound) {
        return ::media::Status::success();
    }
    auto video = requiredBool(nodeOptions(context), MediaTranscodeOptionKey::MuxExpectVideo);
    if (!video) {
        return ::media::Status::failure(video.error());
    }
    auto audio = requiredBool(nodeOptions(context), MediaTranscodeOptionKey::MuxExpectAudio);
    if (!audio) {
        return ::media::Status::failure(audio.error());
    }
    m_expectationsBound = true;
    return m_writer.configureExpectations(video.value(), audio.value());
}

void FileMuxNode::releaseRuntimeViews() noexcept
{
    m_writer.reset();
    m_expectationsBound = false;
}

::media::Status FileMuxNode::forwardIfOutputsExist(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    if (outputChannels(context).empty()) {
        return ::media::Status::success();
    }
    if (buffer->isEof() || buffer->isFlush()) {
        return broadcastControlToAllOutputs(context, buffer);
    }
    return pushToAllOutputs(context, buffer);
}

} // namespace media::ffmpeg::graph
