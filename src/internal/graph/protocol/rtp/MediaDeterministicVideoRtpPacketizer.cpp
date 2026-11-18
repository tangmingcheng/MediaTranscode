#include "internal/graph/protocol/rtp/MediaDeterministicVideoRtpPacketizer.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/protocol/codec/MediaVideoNalUnitScanner.h"
#include <algorithm>
#include <limits>
#include <optional>

namespace media::ffmpeg::graph {
namespace {

// One access unit, one caller-owned workspace. NAL parsing remains shared with
// the source validators; STAP/AP packing and FU boundaries retain decode order.
struct PayloadWriter final {
    MediaAnnexBCodec codec;
    std::span<std::uint8_t> workspace;
    void* state;
    MediaDeterministicVideoRtpPacketizer::Sink sink;
    std::size_t remainingUnits;
    std::span<const std::uint8_t> single;
    std::size_t aggregateBytes = 0;
    std::optional<::media::ErrorInfo> error;

    std::size_t headerBytes() const noexcept
    { return codec == MediaAnnexBCodec::H264 ? 1U : 2U; }

    bool emit(std::size_t bytes, bool marker)
    {
        auto status = sink(state, workspace.first(bytes), marker);
        if (!status) error = status.error();
        return status.ok();
    }

    void appendUnit(std::span<const std::uint8_t> unit)
    {
        workspace[aggregateBytes++] = static_cast<std::uint8_t>(unit.size() >> 8U);
        workspace[aggregateBytes++] = static_cast<std::uint8_t>(unit.size());
        std::copy(unit.begin(), unit.end(), workspace.begin() + aggregateBytes);
        aggregateBytes += unit.size();
        if (codec == MediaAnnexBCodec::H264) {
            workspace[0] = static_cast<std::uint8_t>(
                24U | (std::max)(static_cast<std::uint8_t>(workspace[0] & 0x60U),
                                static_cast<std::uint8_t>(unit[0] & 0x60U)));
        } else {
            const auto layer = (std::min)(
                static_cast<unsigned>(((workspace[0] & 1U) << 5U) | (workspace[1] >> 3U)),
                static_cast<unsigned>(((unit[0] & 1U) << 5U) | (unit[1] >> 3U)));
            const auto tid = (std::min)(workspace[1] & 7U, unit[1] & 7U);
            workspace[0] = static_cast<std::uint8_t>((48U << 1U) | (layer >> 5U));
            workspace[1] = static_cast<std::uint8_t>((layer << 3U) | tid);
        }
    }

    bool flush(bool marker)
    {
        if (aggregateBytes) {
            if (!emit(aggregateBytes, marker)) return false;
        } else if (!single.empty()) {
            std::copy(single.begin(), single.end(), workspace.begin());
            if (!emit(single.size(), marker)) return false;
        }
        single = {};
        aggregateBytes = 0;
        return true;
    }

    void consume(std::span<const std::uint8_t> unit)
    {
        if (error) return;
        --remainingUnits;
        if (unit.size() > workspace.size()) {
            if (!flush(false)) return;
            const auto fuHeader = codec == MediaAnnexBCodec::H264 ? 2U : 3U;
            const auto capacity = workspace.size() - fuHeader;
            const auto type = codec == MediaAnnexBCodec::H264
                ? static_cast<std::uint8_t>(unit[0] & 0x1fU)
                : static_cast<std::uint8_t>((unit[0] >> 1U) & 0x3fU);
            auto remaining = unit.subspan(headerBytes());
            bool first = true;
            while (!remaining.empty()) {
                const auto count = (std::min)(capacity, remaining.size());
                if (codec == MediaAnnexBCodec::H264) {
                    workspace[0] = static_cast<std::uint8_t>((unit[0] & 0xe0U) | 28U);
                } else {
                    workspace[0] = static_cast<std::uint8_t>((unit[0] & 0x81U) | (49U << 1U));
                    workspace[1] = unit[1];
                }
                workspace[fuHeader - 1U] = static_cast<std::uint8_t>(
                    type | (first ? 0x80U : 0U) |
                    (count == remaining.size() ? 0x40U : 0U));
                std::copy_n(remaining.begin(), count, workspace.begin() + fuHeader);
                if (!emit(fuHeader + count, remainingUnits == 0 && count == remaining.size())) return;
                remaining = remaining.subspan(count);
                first = false;
            }
            return;
        }

        const auto lengthLimit = (std::numeric_limits<std::uint16_t>::max)();
        if (aggregateBytes) {
            if (unit.size() <= lengthLimit &&
                2U + unit.size() <= workspace.size() - aggregateBytes) {
                appendUnit(unit);
                return;
            }
            if (!flush(false)) return;
        } else if (!single.empty()) {
            const auto base = headerBytes();
            if (single.size() <= lengthLimit && unit.size() <= lengthLimit &&
                2U + single.size() <= workspace.size() - base &&
                2U + unit.size() <= workspace.size() - base - 2U - single.size()) {
                if (codec == MediaAnnexBCodec::H264) {
                    workspace[0] = 24U;
                } else {
                    workspace[0] = static_cast<std::uint8_t>((48U << 1U) | 1U);
                    workspace[1] = 0xffU;
                }
                aggregateBytes = base;
                appendUnit(single);
                single = {};
                appendUnit(unit);
                return;
            }
            if (!flush(false)) return;
        }
        single = unit;
    }
};
} // namespace
::media::Result<std::uint64_t>
MediaDeterministicVideoRtpPacketizer::maximumDatagramsForPayloadWindow(
    std::uint64_t maximumPayloadBytes,
    std::uint64_t accessUnitCount,
    MediaAnnexBCodec codec,
    std::size_t maximumRtpPayloadBytes)
{
    const std::uint64_t fuHeader =
        codec == MediaAnnexBCodec::H264 ? 2U : 3U;
    if (maximumPayloadBytes == 0 || accessUnitCount == 0 ||
        maximumRtpPayloadBytes <= fuHeader) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "video RTP emission geometry is incomplete"));
    }
    // Greedy AP/STAP packing guarantees that every adjacent pair of output
    // payloads, apart from the terminal payload, consumes at least one FU
    // capacity of access-unit bytes. Prefix bytes already present in Annex-B
    // or length-prefixed input cover each two-byte aggregation length field.
    const auto fragments =
        MediaCheckedArithmetic::ceilScale(
            maximumPayloadBytes, 1U,
            static_cast<std::uint64_t>(maximumRtpPayloadBytes) - fuHeader,
            "deterministic video RTP access-unit fragments");
    if (!fragments) return fragments;
    auto doubled = MediaCheckedArithmetic::multiply(
        fragments.value(), 2U,
        "deterministic video RTP aggregation bound");
    auto accessUnitBound = MediaCheckedArithmetic::multiply(
        accessUnitCount, 3U,
        "deterministic video RTP access-unit boundary bound");
    auto combined = doubled && accessUnitBound
        ? MediaCheckedArithmetic::add(
              doubled.value(), accessUnitBound.value(),
              "deterministic video RTP payload-window bound")
        : (!doubled ? doubled : accessUnitBound);
    if (!combined) return combined;
    return ::media::Result<std::uint64_t>::success(combined.value() - 2U);
}

::media::Result<std::uint64_t>
MediaDeterministicVideoRtpPacketizer::maximumDatagramsPerAccessUnit(
    std::uint64_t maximumAccessUnitBytes,
    MediaAnnexBCodec codec,
    std::size_t maximumRtpPayloadBytes)
{
    return maximumDatagramsForPayloadWindow(
        maximumAccessUnitBytes, 1U, codec, maximumRtpPayloadBytes);
}

::media::Status MediaDeterministicVideoRtpPacketizer::packetize(
    std::span<const std::uint8_t> accessUnit,
    MediaAnnexBCodec codec,
    const MediaEncodedPacketLayout& layout,
    std::span<std::uint8_t> workspace,
    void* state, Sink sink)
{
    const auto fuHeader = codec == MediaAnnexBCodec::H264 ? 2U : 3U;
    if (!sink || workspace.size() <= fuHeader) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "video RTP packetizer requires a sink and fragmentation workspace"));
    }
    // Validate the complete AU before the first side-effecting sink invocation.
    // The count also identifies the final FU without retaining a NAL index.
    std::size_t units = 0;
    auto validated = MediaVideoNalUnitScanner::visit(accessUnit, codec, layout, &units,
        [](void* count, std::span<const std::uint8_t>) {
            ++*static_cast<std::size_t*>(count);
        });
    if (!validated) return validated;
    if (!units) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "video RTP packetizer emitted no payload"));
    PayloadWriter writer{codec, workspace, state, sink, units};
    auto written = MediaVideoNalUnitScanner::visit(accessUnit, codec, layout, &writer,
        [](void* context, std::span<const std::uint8_t> unit) {
            static_cast<PayloadWriter*>(context)->consume(unit);
        });
    if (!written) return written;
    if (!writer.error) writer.flush(true);
    return writer.error ? ::media::Status::failure(*writer.error)
                        : ::media::Status::success();
}
} // namespace media::ffmpeg::graph
