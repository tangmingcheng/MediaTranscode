#include "internal/graph/planner/capability/MediaRkmppEncoderCompletionAdapter.h"
#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
}
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
std::optional<MediaRkmppCompletionConfiguration>
MediaRkmppEncoderCompletionAdapter::configuration(const AVCodecContext& c)
{
    MediaRkmppCompletionConfiguration facts{};
    const std::pair<const char*, std::int64_t*> fields[]{
        {"split_state_known", &facts.known},
        {"effective_split_mode", &facts.splitMode}, {"effective_split_arg", &facts.splitArgument},
        {"effective_split_out", &facts.splitOutput}, {"effective_low_delay", &facts.lowDelay},
        {"effective_rc_drop_mode", &facts.dropMode},
        {"effective_fps_in_flex", &facts.inputFlexible}, {"effective_fps_in_num", &facts.inputNumerator},
        {"effective_fps_in_denom", &facts.inputDenominator},
        {"effective_fps_out_flex", &facts.outputFlexible}, {"effective_fps_out_num", &facts.outputNumerator},
        {"effective_fps_out_denom", &facts.outputDenominator}};
    for (const auto& [name, value] : fields) {
        const auto* option = c.priv_data ? av_opt_find(c.priv_data, name, nullptr, 0, 0) : nullptr;
        if (!option || !(option->flags & AV_OPT_FLAG_READONLY) ||
            (option->type != AV_OPT_TYPE_INT && option->type != AV_OPT_TYPE_INT64) ||
            av_opt_get_int(c.priv_data, name, 0, value) < 0) return {};
    }
    return facts;
}

std::optional<MediaVideoEncoderCompletionContract>
MediaRkmppEncoderCompletionAdapter::readAfterOpen(const AVCodecContext& c,
    const std::array<std::int64_t, 22>& publicFields, std::string_view& reason)
{
    reason = "rkmpp_dependency_identity_unavailable";
    if (!MediaRkmppDependencyIdentity::matchesRuntime()) return {};
    reason = "rkmpp_effective_configuration_unavailable";
    const auto facts = configuration(c);
    if (!facts) return {};
    reason = "rkmpp_complete_picture_configuration_unproven";
    // MPP_ENC_SPLIT_NONE and MPP_ENC_RC_DROP_FRM_DISABLED are both zero.
    // In split-none mode, split_arg/out and base:low_delay cannot enable the
    // partition paths in the audited MPP implementation; retain their values.
    if (facts->known != 1 || facts->splitMode != 0 || facts->splitArgument < 0 ||
        facts->splitOutput < 0 || facts->lowDelay < 0 || facts->dropMode != 0 ||
        facts->inputFlexible != 0 || facts->outputFlexible != 0 ||
        (c.flags & AV_CODEC_FLAG_INTERLACED_DCT) || c.max_b_frames != 0 || c.has_b_frames != 0)
        return {};
    reason = "rkmpp_effective_cadence_differs";
    const auto validPart = [](std::int64_t value) {
        return value > 0 && value <= (std::numeric_limits<int>::max)();
    };
    if (!validPart(facts->inputNumerator) || !validPart(facts->inputDenominator) ||
        !validPart(facts->outputNumerator) || !validPart(facts->outputDenominator) ||
        c.framerate.num <= 0 || c.framerate.den <= 0 ||
        av_cmp_q({static_cast<int>(facts->inputNumerator), static_cast<int>(facts->inputDenominator)}, c.framerate) ||
        av_cmp_q({static_cast<int>(facts->outputNumerator), static_cast<int>(facts->outputDenominator)}, c.framerate)) return {};
    reason = "rkmpp_microsecond_roundtrip_unproven";
    if (c.time_base.num <= 0 || c.time_base.den <= 0) return {};
    // Audited rkmppenc.h transports timestamps in AV_TIME_BASE microseconds.
    // An encoder tick >= one transport tick makes nearest rounding invertible
    // for representable integer input PTS; canonical lineage is never rescaled.
    const auto scaled = MediaCheckedArithmetic::multiply(
        static_cast<std::uint64_t>(c.time_base.num), AV_TIME_BASE, "MPP timestamp tick");
    if (!scaled || scaled.value() < static_cast<std::uint64_t>(c.time_base.den)) return {};
    reason = "authorized";
    return MediaVideoEncoderCompletionContract{
        {c.time_base.num, c.time_base.den}, {c.framerate.num, c.framerate.den},
        MediaEncoderCompletionLifetime::UnchangedPreparedContextUntilReset,
        MediaVideoLineagePropagation::SubmissionOrder,
        {publicFields, *facts}, "audited-rkmpp-complete-picture-fifo-microsecond-timestamp"};
}
}
