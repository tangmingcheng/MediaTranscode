#include "internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

#include <limits>

namespace media::ffmpeg::graph {
namespace {

class ProductionProtocolAvioOpener final : public FFmpegProtocolAvioOpener {
public:
    ::media::Result<AVIOContext*> open(const std::string& protocolUrl,
                                       AVDictionary** protocolOptions,
                                       const AVIOInterruptCB* interrupt) override
    {
        AVIOContext* context = nullptr;
        const int result = avio_open2(&context, protocolUrl.c_str(), AVIO_FLAG_READ,
                                      interrupt, protocolOptions);
        if (result < 0) {
            return ::media::Result<AVIOContext*>::failure(
                ::media::ErrorInfo::ioFailure("failed to open protocol AVIO", result));
        }
        return ::media::Result<AVIOContext*>::success(context);
    }

    void close(AVIOContext** context) noexcept override
    {
        if (context && *context) avio_closep(context);
    }
};

ProductionProtocolAvioOpener& productionOpener()
{
    static ProductionProtocolAvioOpener opener;
    return opener;
}

} // namespace

FFmpegObservedReadAvio::FFmpegObservedReadAvio(
    FFmpegObservedByteSink& observer,
    FFmpegAvioInterruptState& interruptState,
    FFmpegProtocolAvioOpener& opener) noexcept
    : m_observer(observer), m_interruptState(interruptState), m_opener(opener)
{
}

FFmpegObservedReadAvio::~FFmpegObservedReadAvio()
{
    m_interruptState.cancel();
    if (m_outer) {
        av_freep(&m_outer->buffer);
        avio_context_free(&m_outer);
    }
    m_opener.close(&m_inner);
}

::media::Result<std::unique_ptr<FFmpegObservedReadAvio>> FFmpegObservedReadAvio::open(
    const std::string& protocolUrl,
    AVDictionary* protocolOptions,
    std::size_t bufferBytes,
    FFmpegObservedByteSink& observer,
    FFmpegAvioInterruptState& interruptState)
{
    return open(protocolUrl, protocolOptions, bufferBytes, observer, interruptState,
                productionOpener());
}

::media::Result<std::unique_ptr<FFmpegObservedReadAvio>> FFmpegObservedReadAvio::open(
    const std::string& protocolUrl,
    AVDictionary* protocolOptions,
    std::size_t bufferBytes,
    FFmpegObservedByteSink& observer,
    FFmpegAvioInterruptState& interruptState,
    FFmpegProtocolAvioOpener& opener)
{
    if (protocolUrl.empty() || bufferBytes == 0 ||
        bufferBytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return ::media::Result<std::unique_ptr<FFmpegObservedReadAvio>>::failure(
            ::media::ErrorInfo::invalidArgument("invalid observed AVIO configuration"));
    }
    auto result = std::unique_ptr<FFmpegObservedReadAvio>(
        new FFmpegObservedReadAvio(observer, interruptState, opener));
    AVIOInterruptCB interruptCallback{&FFmpegObservedReadAvio::interrupt, &interruptState};
    AVDictionary* options = nullptr;
    av_dict_copy(&options, protocolOptions, 0);
    auto inner = opener.open(protocolUrl, &options, &interruptCallback);
    av_dict_free(&options);
    if (!inner) {
        return ::media::Result<std::unique_ptr<FFmpegObservedReadAvio>>::failure(inner.error());
    }
    result->m_inner = inner.value();
    auto* buffer = static_cast<unsigned char*>(av_malloc(bufferBytes));
    if (!buffer) {
        return ::media::Result<std::unique_ptr<FFmpegObservedReadAvio>>::failure(
            ::media::ErrorInfo::allocationFailed("failed to allocate observed AVIO buffer"));
    }
    result->m_outer = avio_alloc_context(buffer, static_cast<int>(bufferBytes), 0,
                                         result.get(), &FFmpegObservedReadAvio::read,
                                         nullptr, nullptr);
    if (!result->m_outer) {
        av_free(buffer);
        return ::media::Result<std::unique_ptr<FFmpegObservedReadAvio>>::failure(
            ::media::ErrorInfo::allocationFailed("failed to allocate observed AVIO context"));
    }
    result->m_outer->seekable = 0;
    return ::media::Result<std::unique_ptr<FFmpegObservedReadAvio>>::success(std::move(result));
}

int FFmpegObservedReadAvio::read(void* opaque, std::uint8_t* destination, int requested)
{
    auto& self = *static_cast<FFmpegObservedReadAvio*>(opaque);
    if (self.m_interruptState.cancelled()) return AVERROR_EXIT;
    const int result = avio_read(self.m_inner, destination, requested);
    if (result <= 0) return result;
    const auto count = static_cast<std::uint64_t>(result);
    if (self.m_offset > std::numeric_limits<std::uint64_t>::max() - count) {
        self.m_observerFailure =
            ::media::ErrorInfo::invalidArgument("observed AVIO byte offset overflow");
        return result;
    }
    if (!self.m_observerFailure) {
        auto status = self.m_observer.onBytes(
            self.m_offset, std::span<const std::uint8_t>(destination, result));
        if (!status) self.m_observerFailure = status.error();
    }
    self.m_offset += count;
    return result;
}

int FFmpegObservedReadAvio::interrupt(void* opaque) noexcept
{
    return static_cast<FFmpegAvioInterruptState*>(opaque)->cancelled() ? 1 : 0;
}

::media::Status FFmpegObservedReadAvio::status() const
{
    return m_observerFailure
        ? ::media::Status::failure(*m_observerFailure)
        : ::media::Status::success();
}

} // namespace media::ffmpeg::graph
