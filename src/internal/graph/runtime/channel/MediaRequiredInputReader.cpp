#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"

#include "internal/graph/runtime/channel/MediaChannel.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string inputMessage(
    std::string_view consumer,
    std::string_view detail,
    std::string_view inputName)
{
    std::string message;
    message.reserve(consumer.size() + detail.size() + inputName.size());
    message.append(consumer);
    message.append(detail);
    message.append(inputName);
    return message;
}

} // namespace

::media::Result<std::optional<MediaBufferRef>> tryReadRequiredInput(
    MediaChannel* channel,
    std::string_view consumer,
    std::string_view inputName)
{
    if (!channel) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            ::media::ErrorInfo::notInitialized(inputMessage(
                consumer, " requires an input channel: ", inputName)));
    }
    MediaBufferRef buffer;
    if (channel->tryPop(buffer)) {
        return ::media::Result<std::optional<MediaBufferRef>>::success(
            std::move(buffer));
    }
    if (channel->aborted()) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            ::media::ErrorInfo::cancelled(inputMessage(
                consumer,
                " required input aborted before a buffer was available: ",
                inputName)));
    }
    if (channel->closed()) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            ::media::ErrorInfo::cancelled(inputMessage(
                consumer,
                " required input closed before a buffer was available: ",
                inputName)));
    }
    return ::media::Result<std::optional<MediaBufferRef>>::success(
        std::nullopt);
}

} // namespace media::ffmpeg::graph
