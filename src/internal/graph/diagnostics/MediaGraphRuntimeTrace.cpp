#include "internal/graph/diagnostics/MediaGraphRuntimeTrace.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <spdlog/spdlog.h>

namespace media::ffmpeg::graph {

bool MediaGraphRuntimeTrace::enabled(const MediaGraphExecutionContext& context) noexcept
{
    return true; // v2: always enabled via context later
}

bool MediaGraphRuntimeTrace::enabled(const MediaGraph& graph) noexcept
{
    return true;
}

void MediaGraphRuntimeTrace::compileBegin(const MediaGraph&)
{
    spdlog::info("[dag] compile begin");
}

void MediaGraphRuntimeTrace::compileEnd(const MediaGraphExecutionContext&)
{
    spdlog::info("[dag] compile end");
}

void MediaGraphRuntimeTrace::nodeEnter(const MediaGraphExecutionContext&, MediaNodeId nodeId, const std::string& name)
{
    spdlog::info("[dag][node enter] id={} name={}", nodeId.value, name);
}

void MediaGraphRuntimeTrace::nodeExit(const MediaGraphExecutionContext&, MediaNodeId nodeId, const std::string& name, const ::media::Status& status)
{
    spdlog::info("[dag][node exit] id={} name={} ok={}", nodeId.value, name, (bool)status);
}

void MediaGraphRuntimeTrace::edgePush(const MediaGraphExecutionContext&, const MediaChannel& channel, const MediaBufferRef&, const char*)
{
    spdlog::info("[dag][edge push] channel={}", channel.id().value);
}

void MediaGraphRuntimeTrace::edgePop(const MediaGraphExecutionContext&, const MediaChannel& channel, const MediaBufferRef&, const char*)
{
    spdlog::info("[dag][edge pop] channel={}", channel.id().value);
}

void MediaGraphRuntimeTrace::channelPush(const MediaChannel& channel, const MediaBufferRef&, bool enabled, const ::media::Status&)
{
    if (!enabled) return;
    spdlog::info("[dag][channel push] id={}", channel.id().value);
}

void MediaGraphRuntimeTrace::channelTryPush(const MediaChannel& channel, const MediaBufferRef&, bool enabled, bool ok)
{
    if (!enabled) return;
    spdlog::info("[dag][channel tryPush] id={} ok={}", channel.id().value, ok);
}

void MediaGraphRuntimeTrace::channelPop(const MediaChannel& channel, const MediaBufferRef&, bool enabled, const ::media::Status&)
{
    if (!enabled) return;
    spdlog::info("[dag][channel pop] id={}", channel.id().value);
}

void MediaGraphRuntimeTrace::channelTryPop(const MediaChannel& channel, const MediaBufferRef&, bool enabled, bool ok)
{
    if (!enabled) return;
    spdlog::info("[dag][channel tryPop] id={} ok={}", channel.id().value, ok);
}

void MediaGraphRuntimeTrace::channelState(const MediaChannel& channel, bool enabled, const char* action)
{
    if (!enabled) return;
    spdlog::info("[dag][channel state] id={} action={}", channel.id().value, action);
}

}