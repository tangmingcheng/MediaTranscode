#include "internal/graph/nodes/video/MediaVideoFrameReadinessPlanCodec.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <utility>
namespace media::ffmpeg::graph {
namespace {
constexpr std::array names{
    std::pair{MediaCudaFrameProducer::NativeSafeDecode, std::string_view{"native_safe_decode"}},
    std::pair{MediaCudaFrameProducer::CuvidDecode, std::string_view{"cuvid_decode"}},
    std::pair{MediaCudaFrameProducer::ScaleCuda, std::string_view{"scale_cuda"}},
    std::pair{MediaCudaFrameProducer::HardwareTransfer, std::string_view{"hardware_transfer"}}};
constexpr const char* kindKey = "video.readiness.producer";
constexpr const char* contextKey = "video.readiness.context";
constexpr const char* streamKey = "video.readiness.stream";
constexpr const char* referencesKey = "video.readiness.maximum_extended_references";
}
::media::Result<MediaNodeOptions> MediaVideoFrameReadinessPlanCodec::encode(const MediaCudaFrameProducerFacts& facts)
{
    using Result = ::media::Result<MediaNodeOptions>;
    if (!facts.context) return Result::failure(::media::ErrorInfo::invalidArgument("Frame readiness requires its prepared CUDA context"));
    std::string_view name;
    for (const auto& entry : names) if (entry.first == facts.kind) name = entry.second;
    if (name.empty()) return Result::failure(::media::ErrorInfo::invalidArgument("Frame readiness producer is not admitted"));
    MediaNodeOptions result;
    result.set(kindKey, std::string(name));
    result.set(contextKey, std::to_string(reinterpret_cast<std::uintptr_t>(facts.context)));
    // NULL is a real legacy CUDA stream, not an omitted option.
    result.set(streamKey, std::to_string(reinterpret_cast<std::uintptr_t>(facts.stream)));
    if (facts.maximumExtendedBufferReferences)
        result.set(referencesKey, std::to_string(*facts.maximumExtendedBufferReferences));
    return Result::success(std::move(result));
}
::media::Result<std::optional<MediaCudaFrameProducerFacts>> MediaVideoFrameReadinessPlanCodec::decode(const MediaNodeOptions& options)
{
    using Result = ::media::Result<std::optional<MediaCudaFrameProducerFacts>>;
    const auto invalid = [] { return Result::failure(::media::ErrorInfo::invalidArgument("Frame readiness options require a complete prepared producer contract")); };
    if (!options.has(kindKey) && !options.has(contextKey) && !options.has(streamKey) && !options.has(referencesKey)) return Result::success(std::nullopt);
    if (!options.has(kindKey) || !options.has(contextKey) || !options.has(streamKey)) return invalid();
    std::optional<MediaCudaFrameProducer> kind;
    const auto name = options.value(kindKey);
    for (const auto& entry : names) if (entry.second == name) kind = entry.first;
    if (!kind) return invalid();
    const auto readHandle = [&options](const char* key, std::uintptr_t& handle) {
        const auto value = options.value(key);
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), handle);
        return !value.empty() && parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
    };
    std::uintptr_t context = 0, stream = 0;
    if (!readHandle(contextKey, context) || !context || !readHandle(streamKey, stream)) return invalid();
    std::optional<std::size_t> references;
    if (options.has(referencesKey)) {
        std::uintptr_t value = 0;
        if (!readHandle(referencesKey, value)) return invalid();
        references = value;
    }
    return Result::success(MediaCudaFrameProducerFacts{*kind, reinterpret_cast<void*>(context), reinterpret_cast<void*>(stream), references});
}
}
