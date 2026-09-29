#include "internal/graph/protocol/codec/MediaSpsVuiParser.h"
#include <algorithm>
#include <array>

namespace media::ffmpeg::graph {
namespace {
using Reader = MediaRbspBitReader;

bool signal(Reader& r, MediaVideoColorRangeFact& fact)
{
    std::uint32_t flag = 0, value = 0;
    if (!r.bit(flag)) return false;
    if (flag && (!r.bits(8, value) || (value == 255 && !r.skip(32)))) return false;
    if (!r.bit(flag) || (flag && !r.skip(1)) || !r.bit(flag)) return false;
    if (flag) {
        if (!r.skip(3) || !r.bit(value)) return false;
        fact = {value ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG, true};
        if (!r.bit(flag) || (flag && !r.skip(24))) return false;
    }
    if (!r.bit(flag)) return false;
    return !flag || (r.ue(value, 5) && r.ue(value, 5));
}

bool h264Hrd(Reader& r)
{
    std::uint32_t count = 0, value = 0;
    if (!r.ue(count, 31) || !r.skip(8)) return false;
    for (std::uint32_t i = 0; i <= count; ++i)
        if (!r.ue(value, UINT32_MAX - 1) || !r.ue(value, UINT32_MAX - 1) || !r.skip(1)) return false;
    return r.skip(20);
}

bool hevcHrd(Reader& r, std::uint32_t subLayers)
{
    std::uint32_t nal = 0, vcl = 0, subPic = 0, value = 0;
    if (!r.bit(nal) || !r.bit(vcl)) return false;
    if (nal || vcl) {
        if (!r.bit(subPic) || (subPic && !r.skip(19)) || !r.skip(8) ||
            (subPic && !r.skip(4)) || !r.skip(15)) return false;
    }
    for (std::uint32_t i = 0; i <= subLayers; ++i) {
        std::uint32_t fixed = 0, lowDelay = 0, count = 0;
        if (!r.bit(fixed) || (!fixed && !r.bit(fixed))) return false;
        if (fixed) { if (!r.ue(value, 2047)) return false; }
        else if (!r.bit(lowDelay)) return false;
        if (!lowDelay && !r.ue(count, 31)) return false;
        for (unsigned kind = 0; kind < 2; ++kind) {
            if (!(kind == 0 ? nal : vcl)) continue;
            for (std::uint32_t j = 0; j <= count; ++j) {
                if (!r.ue(value, UINT32_MAX - 1) || !r.ue(value, UINT32_MAX - 1) ||
                    (subPic && (!r.ue(value, UINT32_MAX - 1) || !r.ue(value, UINT32_MAX - 1))) ||
                    !r.skip(1)) return false;
            }
        }
    }
    return true;
}

bool hevcScaling(Reader& r)
{
    std::uint32_t flag = 0, value = 0;
    std::int32_t delta = 0;
    for (unsigned size = 0; size < 4; ++size) {
        for (unsigned matrix = 0; matrix < 6; matrix += size == 3 ? 3 : 1) {
            if (!r.bit(flag)) return false;
            if (!flag) { if (!r.ue(value, size == 3 ? matrix / 3 : matrix)) return false; }
            else {
                if (size > 1 && !r.se(delta, -7, 247)) return false;
                const auto count = (std::min)(64U, 1U << (4 + 2 * size));
                for (unsigned i = 0; i < count; ++i) if (!r.se(delta, -128, 127)) return false;
            }
        }
    }
    return true;
}

bool hevcShortTermSets(Reader& r, std::uint32_t count)
{
    // Retain delta POCs because predicted zero POCs are excluded, and the next
    // set's syntax length depends on the resulting count (H.265 7.4.8).
    std::array<std::int64_t, 16> previous{};
    std::uint32_t previousCount = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<std::int64_t, 16> next{};
        std::uint32_t predicted = 0, value = 0, nextCount = 0;
        if (i && !r.bit(predicted)) return false;
        if (predicted) {
            std::uint32_t sign = 0;
            if (!r.bit(sign) || !r.ue(value, INT16_MAX)) return false;
            const std::int64_t delta = (sign ? -1 : 1) * (std::int64_t{value} + 1);
            for (std::uint32_t j = 0; j <= previousCount; ++j) {
                std::uint32_t use = 0;
                if (!r.bit(use) || (!use && !r.bit(use))) return false;
                const auto poc = (j == previousCount ? 0 : previous[j]) + delta;
                if (use && poc != 0) next[nextCount++] = poc;
            }
            if (nextCount > 15) return false;
            std::sort(next.begin(), next.begin() + nextCount, [](auto a, auto b) {
                if ((a < 0) != (b < 0)) return a < 0;
                return a < 0 ? a > b : a < b;
            });
        } else {
            std::uint32_t negative = 0, positive = 0;
            if (!r.ue(negative, 15) || !r.ue(positive, 15 - negative)) return false;
            nextCount = negative + positive;
            std::int64_t poc = 0;
            for (std::uint32_t j = 0; j < nextCount; ++j) {
                if (j == negative) poc = 0;
                if (!r.ue(value, INT16_MAX) || !r.skip(1)) return false;
                poc += (j < negative ? -1 : 1) * (std::int64_t{value} + 1);
                next[j] = poc;
            }
        }
        previous = next;
        previousCount = nextCount;
    }
    return true;
}

bool hevcVui(Reader& r, std::uint32_t subLayers, MediaVideoColorRangeFact& fact)
{
    std::uint32_t flag = 0, value = 0;
    if (!signal(r, fact) || !r.skip(3) || !r.bit(flag)) return false;
    if (flag) for (unsigned i = 0; i < 4; ++i) if (!r.ue(value, 16384)) return false;
    if (!r.bit(flag)) return false;
    if (flag) {
        if (!r.bits(32, value) || !value || !r.bits(32, value) || !value || !r.bit(flag)) return false;
        if (flag && !r.ue(value, UINT32_MAX - 1)) return false;
        if (!r.bit(flag) || (flag && !hevcHrd(r, subLayers))) return false;
    }
    if (!r.bit(flag)) return false;
    if (flag) {
        if (!r.skip(3) || !r.ue(value, 4095)) return false;
        for (unsigned i = 0; i < 4; ++i) if (!r.ue(value, 16)) return false;
    }
    return true;
}
} // namespace

std::optional<MediaVideoColorRangeFact> MediaSpsVuiParser::h264(Reader& r)
{
    MediaVideoColorRangeFact fact{AVCOL_RANGE_MPEG, false};
    std::uint32_t present = 0, value = 0;
    if (!r.bit(present)) return std::nullopt;
    if (present) {
        if (!signal(r, fact) || !r.bit(present)) return std::nullopt;
        if (present && (!r.bits(32, value) || !value || !r.bits(32, value) || !value || !r.skip(1))) return std::nullopt;
        std::uint32_t nalHrd = 0, vclHrd = 0;
        if (!r.bit(nalHrd) || (nalHrd && !h264Hrd(r)) ||
            !r.bit(vclHrd) || (vclHrd && !h264Hrd(r)) ||
            ((nalHrd || vclHrd) && !r.skip(1)) || !r.skip(1) || !r.bit(present)) return std::nullopt;
        if (present) {
            if (!r.skip(1)) return std::nullopt;
            for (unsigned i = 0; i < 6; ++i) if (!r.ue(value, 16)) return std::nullopt;
        }
    }
    return r.trailingBits() ? std::optional(fact) : std::nullopt;
}

std::optional<MediaVideoColorRangeFact> MediaSpsVuiParser::hevc(Reader& r, std::uint32_t subLayers)
{
    std::uint32_t lumaDepth = 0, chromaDepth = 0, poc = 0, flag = 0, value = 0;
    if (!r.ue(lumaDepth, 8) || !r.ue(chromaDepth, 8) || !r.ue(poc, 12) || !r.bit(flag)) return std::nullopt;
    for (std::uint32_t i = flag ? 0 : subLayers; i <= subLayers; ++i) {
        std::uint32_t buffering = 0;
        if (!r.ue(buffering, 15) || !r.ue(value, buffering) || !r.ue(value, UINT32_MAX - 1)) return std::nullopt;
    }
    std::uint32_t minCb = 0, diffCb = 0, minTb = 0;
    if (!r.ue(minCb, 3) || !r.ue(diffCb, 3) || minCb + diffCb > 3 ||
        !r.ue(minTb, minCb) || !r.ue(value, (std::min)(minCb + diffCb + 3, 5U) - minTb - 2) ||
        !r.ue(value, minCb + diffCb + 1 - minTb) ||
        !r.ue(value, minCb + diffCb + 1 - minTb)) return std::nullopt;
    if (!r.bit(flag)) return std::nullopt;
    if (flag && (!r.bit(flag) || (flag && !hevcScaling(r)))) return std::nullopt;
    if (!r.skip(2) || !r.bit(flag)) return std::nullopt;
    if (flag) {
        std::uint32_t pcm = 0;
        if (!r.bits(4, value) || value > lumaDepth + 7 || !r.bits(4, value) || value > chromaDepth + 7 ||
            !r.ue(pcm, (std::min)(minCb + diffCb + 3, 5U) - 3) ||
            pcm < (std::min)(minCb + 3, 5U) - 3 ||
            !r.ue(value, (std::min)(minCb + diffCb + 3, 5U) - pcm - 3) || !r.skip(1)) return std::nullopt;
    }
    if (!r.ue(value, 64) || !hevcShortTermSets(r, value) || !r.bit(flag)) return std::nullopt;
    if (flag) {
        std::uint32_t count = 0;
        if (!r.ue(count, 32)) return std::nullopt;
        for (std::uint32_t i = 0; i < count; ++i) if (!r.skip(poc + 5)) return std::nullopt;
    }
    MediaVideoColorRangeFact fact{AVCOL_RANGE_MPEG, false};
    if (!r.skip(2) || !r.bit(flag) || (flag && !hevcVui(r, subLayers, fact)) || !r.bit(flag)) return std::nullopt;
    if (flag) {
        std::uint32_t rangeExtension = 0;
        // Range extension has nine flags. Other extensions are deliberately
        // unsupported: no range fact is published for partially parsed SPSs.
        if (!r.bit(rangeExtension) || !r.bits(7, value) || value != 0 ||
            (rangeExtension && !r.skip(9))) return std::nullopt;
    }
    return r.trailingBits() ? std::optional(fact) : std::nullopt;
}
} // namespace media::ffmpeg::graph
