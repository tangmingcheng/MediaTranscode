#include "internal/graph/planner/realtime/MediaDecoderOpaqueRetentionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
namespace media::ffmpeg::graph {
::media::Result<std::uint64_t> MediaDecoderOpaqueRetentionPlanner::registryCapacity(
    const MediaDecoderOpaqueRetention& facts)
{
    using Result = ::media::Result<std::uint64_t>;
    using A = MediaCheckedArithmetic;
    if (facts.threadCount <= 0 || facts.activeThreadType < 0 ||
        (facts.activeThreadType & ~(FF_THREAD_FRAME | FF_THREAD_SLICE)) ||
        !facts.privateFrameSlots || !facts.genericSlots ||
        facts.frameWorkers != ((facts.activeThreadType & FF_THREAD_FRAME)
            ? static_cast<std::uint64_t>(facts.threadCount) : 0))
        return Result::failure(::media::ErrorInfo::invalidArgument("Opaque retention requires opened decoder container facts"));
    auto context = A::add(facts.privateFrameSlots, facts.genericSlots, "decoder opaque context owners");
    if (!context) return context;
    if (facts.outputFifo) {
        auto fifo = A::add(context.value(), 1, "decoder output FIFO unique input tokens");
        if (!fifo) return fifo;
        context = A::add(context.value(), fifo.value(), "decoder context and FIFO opaque owners");
        if (!context) return context;
    }
    // Engine pending input and receive output coexist with codec-owned tokens.
    if (!facts.frameWorkers) return A::add(context.value(), 2, "serial decoder opaque progress slots");
    auto batch = A::add(context.value(), 1, "frame worker batch unique input tokens");
    if (!batch) return batch;
    auto perContext = A::add(context.value(), batch.value(), "decoder context and output batch owners");
    if (!perContext) return perContext;
    auto contexts = A::add(facts.frameWorkers, 1, "decoder workers and parent context");
    if (!contexts) return contexts;
    auto total = A::multiply(contexts.value(), perContext.value(), "threaded decoder opaque owners");
    if (!total) return total;
    auto handoff = A::add(facts.frameWorkers, 3, "worker packets, parent next packet and engine working slots");
    if (!handoff) return handoff;
    return A::add(total.value(), handoff.value(), "decoder opaque registry capacity");
}
}
