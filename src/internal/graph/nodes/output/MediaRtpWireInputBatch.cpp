#include "internal/graph/nodes/output/MediaRtpWireInputBatch.h"
#include <type_traits>

namespace media::ffmpeg::graph {
std::size_t MediaRtpWireInputDatagram::payloadOctets() const noexcept
{
    return std::visit([](const auto& input) -> std::size_t {
        if constexpr (std::is_same_v<std::decay_t<decltype(input)>, MediaPacketizedRtpDatagramView>)
            return input.payloadOctets;
        else return input.payloadOctets();
    }, payload);
}

MediaRtpWireInputBatch::MediaRtpWireInputBatch(
    std::span<const MediaPacketizedRtpDatagramView> datagrams) noexcept : m_input(datagrams) {}
MediaRtpWireInputBatch::MediaRtpWireInputBatch(
    std::span<const MediaMpegTsDatagramView> datagrams,
    std::reference_wrapper<const MediaMpegTsRtpPacketizer> packetizer) noexcept
    : m_input(TsBatch<MediaMpegTsDatagramView>{datagrams, packetizer}) {}
MediaRtpWireInputBatch::MediaRtpWireInputBatch(
    std::span<const MediaMpegTsProtocolDatagram> datagrams,
    std::reference_wrapper<const MediaMpegTsRtpPacketizer> packetizer) noexcept
    : m_input(TsBatch<MediaMpegTsProtocolDatagram>{datagrams, packetizer}) {}

std::size_t MediaRtpWireInputBatch::size() const noexcept
{
    return std::visit([](const auto& input) {
        if constexpr (std::is_same_v<std::decay_t<decltype(input)>, std::span<const MediaPacketizedRtpDatagramView>>)
            return input.size();
        else return input.datagrams.size();
    }, m_input);
}

MediaRtpWireInputTiming MediaRtpWireInputBatch::timing(std::size_t index) const noexcept
{
    return std::visit([index](const auto& input) -> MediaRtpWireInputTiming {
        using Input = std::decay_t<decltype(input)>;
        if constexpr (std::is_same_v<Input, std::span<const MediaPacketizedRtpDatagramView>>) {
            const auto& item = input[index];
            return {item.presentationOnMaster, item.canonicalRelease, item.mediaBoundary};
        } else if constexpr (std::is_same_v<Input, TsBatch<MediaMpegTsDatagramView>>) {
            const auto& item = input.datagrams[index];
            return {item.canonicalRelease, item.canonicalRelease, item.mediaBoundary};
        } else {
            const auto& item = input.datagrams[index];
            // MP2T timestamps follow transport release, as in the existing packetizer path.
            return {item.canonicalRelease(), item.canonicalRelease(), item.mediaBoundary()};
        }
    }, m_input);
}

::media::Result<MediaRtpWireInputDatagram> MediaRtpWireInputBatch::at(std::size_t index) const
{
    using Result = ::media::Result<MediaRtpWireInputDatagram>;
    if (index >= size()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP input index exceeds the borrowed batch"));
    const auto facts = timing(index);
    return std::visit([index, facts](const auto& input) -> Result {
        using Input = std::decay_t<decltype(input)>;
        if constexpr (std::is_same_v<Input, std::span<const MediaPacketizedRtpDatagramView>>) {
            return Result::success({input[index], facts});
        } else {
            const auto bytes = [&] {
                if constexpr (std::is_same_v<Input, TsBatch<MediaMpegTsDatagramView>>)
                    return input.datagrams[index].completeTsPackets;
                else return input.datagrams[index].bytes();
            }();
            auto plan = input.packetizer.get().prepare(bytes, facts.canonicalRelease, 0);
            if (!plan) return Result::failure(plan.error());
            return Result::success({std::move(plan).value(), facts});
        }
    }, m_input);
}
} // namespace media::ffmpeg::graph
