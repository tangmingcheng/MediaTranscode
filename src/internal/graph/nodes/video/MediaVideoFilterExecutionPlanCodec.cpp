#include "internal/graph/nodes/video/MediaVideoFilterExecutionPlanCodec.h"
#include <charconv>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaNodeOptions> MediaVideoFilterExecutionPlanCodec::encode(
    const MediaVideoFilterExecutionPlan& plan)
{
    if (!plan.valid()) return ::media::Result<MediaNodeOptions>::failure(
        ::media::ErrorInfo::invalidArgument("video filter execution plan is incomplete"));
    MediaNodeOptions result;
    const bool source = plan.timing == MediaVideoFilterTimingAuthority::SourceFrame;
    result.set("filter.execution.aspect", "preserve_source_aspect");
    result.set("filter.execution.timing", source ? "source_frame" : "prepared_encoder_metadata");
    result.set("filter.execution.allocation", source ? "independent_output_pool" : "negotiated_output");
    result.set("filter.execution.completion", source ? "synchronous_on_return" : "negotiated_output");
    result.set("filter.execution.description", plan.output.filterDescription);
    result.set("filter.execution.allocation_authority", plan.output.allocationAuthority);
    result.set("filter.execution.completion_authority", plan.output.completionAuthority);
    if (source) {
        result.set("filter.execution.maximum_retained_input_frames", std::to_string(plan.output.maximumRetainedInputFrames));
        result.set("filter.execution.input_retention_authority", plan.output.inputRetentionAuthority);
        result.set("filter.execution.fps_num", std::to_string(plan.sourceFrameRate.num));
        result.set("filter.execution.fps_den", std::to_string(plan.sourceFrameRate.den));
        result.set("filter.execution.sar_num", std::to_string(plan.sourceSampleAspectRatio.num));
        result.set("filter.execution.sar_den", std::to_string(plan.sourceSampleAspectRatio.den));
    }
    return ::media::Result<MediaNodeOptions>::success(std::move(result));
}

::media::Result<MediaVideoFilterExecutionPlan> MediaVideoFilterExecutionPlanCodec::decode(
    const MediaNodeOptions& options)
{
    using Result = ::media::Result<MediaVideoFilterExecutionPlan>;
    const auto invalid = [] { return Result::failure(::media::ErrorInfo::invalidArgument(
        "video filter execution options differ from a complete planner product")); };
    MediaVideoFilterExecutionPlan plan{};
    if (options.value("filter.execution.aspect") != "preserve_source_aspect") return invalid();
    plan.aspect = MediaVideoFilterAspectPolicy::PreserveSourceAspect;
    const auto timing = options.value("filter.execution.timing");
    const bool source = timing == "source_frame";
    if (!source && timing != "prepared_encoder_metadata") return invalid();
    plan.timing = source ? MediaVideoFilterTimingAuthority::SourceFrame
                        : MediaVideoFilterTimingAuthority::PreparedEncoderMetadata;
    if (options.value("filter.execution.allocation") !=
            (source ? "independent_output_pool" : "negotiated_output") ||
        options.value("filter.execution.completion") !=
            (source ? "synchronous_on_return" : "negotiated_output")) return invalid();
    plan.output = {options.value("filter.execution.description"),
        source ? MediaVideoFilterAllocation::IndependentOutputPool : MediaVideoFilterAllocation::NegotiatedOutput,
        source ? MediaVideoFilterCompletion::SynchronousOnReturn : MediaVideoFilterCompletion::NegotiatedOutput,
        options.value("filter.execution.allocation_authority"), options.value("filter.execution.completion_authority"), 0, {}};
    if (source) {
        const auto retained = options.value("filter.execution.maximum_retained_input_frames");
        const auto parsed = std::from_chars(retained.data(), retained.data() + retained.size(),
            plan.output.maximumRetainedInputFrames);
        if (retained.empty() || parsed.ec != std::errc{} || parsed.ptr != retained.data() + retained.size())
            return invalid();
        plan.output.inputRetentionAuthority = options.value("filter.execution.input_retention_authority");
        const auto read = [&options](const char* name, int& value) {
            const auto text = options.value(name);
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
            return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value >= 0;
        };
        if (!read("filter.execution.fps_num", plan.sourceFrameRate.num) ||
            !read("filter.execution.fps_den", plan.sourceFrameRate.den) ||
            !read("filter.execution.sar_num", plan.sourceSampleAspectRatio.num) ||
            !read("filter.execution.sar_den", plan.sourceSampleAspectRatio.den)) return invalid();
    }
    if (!plan.valid()) return invalid();
    return Result::success(std::move(plan));
}
} // namespace media::ffmpeg::graph
