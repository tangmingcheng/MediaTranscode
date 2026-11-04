#include "internal/graph/planner/realtime/MediaRtpConfigurationStoragePlanner.h"
#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {
std::string_view trimmed(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
    return value;
}
bool sameKey(std::string_view actual, std::string_view expected)
{
    return actual.size() == expected.size() && std::equal(actual.begin(), actual.end(), expected.begin(),
        [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}
}

::media::Result<MediaRtpConfigurationStoragePlan> MediaRtpConfigurationStoragePlanner::plan(
    const MediaRtpDepacketizerConfig& config, const MediaPreparedRtpAccessUnitEnvelope& envelope)
{
    using R = ::media::Result<MediaRtpConfigurationStoragePlan>;
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto result = MediaCheckedArithmetic::add(a, b, "RTP configuration owner overlap");
        if (!result) { failure = result.error(); return std::uint64_t{0}; }
        return result.value();
    };
    const auto mul = [&](std::uint64_t a, std::uint64_t b) {
        auto result = MediaCheckedArithmetic::multiply(a, b, "RTP configuration owner overlap");
        if (!result) { failure = result.error(); return std::uint64_t{0}; }
        return result.value();
    };
    const auto cost = [&](const ::media::Result<std::size_t>& value) {
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return static_cast<std::uint64_t>(value.value());
    };
    const auto text = [&](std::size_t size) { return cost(MediaStandardContainerStorageReadback::stringCopy(size)); };
    const auto proxy = cost(MediaStandardContainerGeometry::proxyBytes());
    if (failure) return R::failure(failure);
    const auto configCopy = add(text(config.codecName.size()), text(config.fmtp.size()));
    auto envelopeCopy = add(text(envelope.codecName.size()), text(envelope.sizeAuthority.size()));
    envelopeCopy = add(envelopeCopy, text(envelope.completionAuthority.size()));
    // One config lives in the depacketizer; the protocol independently owns
    // another config and its envelope. Stream/m_plan strings are charged by F.
    const auto retained = add(mul(2, configCopy), envelopeCopy);
    const auto normalizedCodec = add(text(config.codecName.size()), proxy);
    const bool opus = sameKey(config.codecName, "opus");
    const bool h264 = sameKey(config.codecName, "h264");
    const bool hevc = sameKey(config.codecName, "hevc");
    const bool aac = sameKey(config.codecName, "aac");
    if (!opus && !h264 && !hevc && !aac)
        return R::failure(::media::ErrorInfo::unsupported("Configuration geometry requires a validated RTP codec"));
    std::uint64_t validation = normalizedCodec;
    if (!opus) {
        std::size_t entries = 0;
        std::uint64_t mapStrings = 0, tokenPeak = 0, decodePeak = 0, modePeak = 0;
        const auto decoded = [&](std::size_t count, bool incremental) {
            auto geometry = MediaStandardContainerGeometry::vector<std::uint8_t>(count, incremental);
            if (!geometry) { failure = geometry.error(); return std::uint64_t{0}; }
            // local bytes -> success parameter -> Result constructor parameter
            // -> optional: backing moves, three old proxy owners remain.
            return add(geometry.value().peakBytes, mul(3, proxy));
        };
        std::string_view input(config.fmtp);
        std::size_t offset = 0;
        while (offset <= input.size()) {
            const auto delimiter = input.find(';', offset);
            const auto raw = input.substr(offset, delimiter == std::string_view::npos ? input.size() - offset : delimiter - offset);
            const auto token = trimmed(raw);
            tokenPeak = (std::max)(tokenPeak, add(text(raw.size()), text(token.size())));
            if (!token.empty()) {
                const auto equal = token.find('=');
                if (equal == std::string_view::npos)
                    return R::failure(::media::ErrorInfo::invalidArgument("Configuration geometry requires validated fmtp tokens"));
                const auto rawKey = token.substr(0, equal);
                const auto rawValue = token.substr(equal + 1);
                const auto key = trimmed(rawKey), value = trimmed(rawValue);
                if (key.empty() || value.empty())
                    return R::failure(::media::ErrorInfo::invalidArgument("Configuration geometry requires nonempty fmtp fields"));
                ++entries;
                const auto tokenBytes = text(token.size()), keyBytes = text(key.size()), valueBytes = text(value.size());
                mapStrings = add(mapStrings, add(keyBytes, valueBytes));
                // parseRtpFmtp has distinct full-expression phases. trim's input
                // substring and output coexist; lowercase returns by move.
                tokenPeak = (std::max)(tokenPeak, add(tokenBytes, add(text(rawKey.size()), add(keyBytes, proxy))));
                tokenPeak = (std::max)(tokenPeak, add(add(tokenBytes, keyBytes), add(text(rawValue.size()), valueBytes)));
                tokenPeak = (std::max)(tokenPeak, add(tokenBytes, add(keyBytes, valueBytes)));
                if ((h264 && sameKey(key, "sprop-parameter-sets")) ||
                    (hevc && (sameKey(key, "sprop-vps") || sameKey(key, "sprop-sps") || sameKey(key, "sprop-pps")))) {
                    const auto ownerLength = h264 ? std::string_view("H264 RTP").size() : std::string_view("HEVC RTP ").size() + key.size();
                    // map.at's temporary key and validateBase64List's owner
                    // both remain alive while a substring and decoded vector exist.
                    auto call = add(text(key.size()), text(ownerLength));
                    if (hevc) call = add(call, proxy); // moved-from concatenation prefix
                    std::size_t start = 0;
                    while (start <= value.size()) {
                        const auto comma = value.find(',', start);
                        const auto part = value.substr(start, comma == std::string_view::npos ? value.size() - start : comma - start);
                        if (part.empty() || part.size() % 4)
                            return R::failure(::media::ErrorInfo::invalidArgument("Configuration geometry requires validated base64 lengths"));
                        const auto padding = static_cast<std::size_t>(part.back() == '=') +
                            static_cast<std::size_t>(part.size() > 1 && part[part.size() - 2] == '=');
                        const auto output = part.size() / 4 * 3 - padding;
                        decodePeak = (std::max)(decodePeak, add(call, add(text(part.size()), decoded(output, true))));
                        if (comma == std::string_view::npos) break;
                        start = comma + 1;
                    }
                } else if ((h264 && sameKey(key, "profile-level-id")) || (aac && sameKey(key, "config"))) {
                    auto decode = decoded(value.size() / 2, false);
                    if (h264) decode = add(decode, text(key.size())); // at() argument
                    decodePeak = (std::max)(decodePeak, decode);
                }
                if ((hevc && sameKey(key, "tx-mode")) || (aac && sameKey(key, "mode")))
                    modePeak = (std::max)(modePeak, add(valueBytes, proxy));
            }
            if (delimiter == std::string_view::npos) break;
            offset = delimiter + 1;
        }
        const auto tree = cost(MediaStandardContainerGeometry::tree<MediaRtpFmtpParameters::value_type>(entries));
        const auto emptyTree = cost(MediaStandardContainerGeometry::tree<MediaRtpFmtpParameters::value_type>(0));
        const auto map = add(tree, mapStrings);
        // Map return has the same three by-value move shells as Result<vector>;
        // each moved-from map retains its own sentinel and debug proxy.
        const auto parsing = add(map, (std::max)(tokenPeak, mul(3, emptyTree)));
        std::uint64_t lookup = 0;
        const auto integerLookup = [&](std::string_view key, std::string_view owner) {
            auto live = add(mul(2, text(key.size())), proxy);
            if (!owner.empty()) live = add(live, text(owner.size()));
            lookup = (std::max)(lookup, live);
        };
        if (h264) {
            integerLookup("packetization-mode", {});
            lookup = (std::max)(lookup, text(std::string_view("sprop-parameter-sets").size()));
        }
        if (hevc) {
            integerLookup("sprop-max-don-diff", {});
            lookup = (std::max)(lookup, text(std::string_view("tx-mode").size()));
        }
        if (aac) {
            for (const auto key : {"sizelength", "indexlength", "indexdeltalength"}) integerLookup(key, {});
            for (const auto key : {"ctsdeltalength", "dtsdeltalength", "randomaccessindication",
                    "streamstateindication", "auxiliarydatasizelength"}) integerLookup(key, "AAC MPEG4-GENERIC");
        }
        const auto inspection = add(map, (std::max)({lookup, decodePeak, modePeak}));
        validation = add(normalizedCodec, (std::max)(parsing, inspection));
    }
    // Factory validation ends before ProtocolSession validation begins; the
    // depacketizer config is already resident for the latter. Never sum them.
    // Protocol config/envelope have five strings. The create parameters,
    // private constructor parameters, success parameter, Result parameter and
    // optional overlap; only the four moved-from groups add proxy requests.
    const auto construction = mul(mul(4, 5), proxy);
    const auto comparison = add(add(text(config.codecName.size()), text(envelope.codecName.size())), mul(2, proxy));
    const auto transient = (std::max)({validation, construction, comparison});
    const auto peak = add(retained, transient);
    if (failure || peak > (std::numeric_limits<std::size_t>::max)())
        return R::failure(failure ? failure : ::media::ErrorInfo::allocationFailed("Configuration allocation exceeds addressable storage"));
    return R::success({static_cast<std::size_t>(retained), static_cast<std::size_t>(peak)});
}
} // namespace media::ffmpeg::graph
