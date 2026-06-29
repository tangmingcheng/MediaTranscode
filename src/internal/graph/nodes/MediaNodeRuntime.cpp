#include "internal/graph/nodes/MediaNodeRuntime.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string describeChannels(const std::vector<MediaChannel*>& channels)
{
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const MediaChannel* channel : channels) {
        if (!channel) {
            continue;
        }
        if (!first) {
            out << "; ";
        }
        first = false;
        out << mediaGraphDiagnosticDescribeChannel(*channel);
    }
    out << "]";
    return out.str();
}

} // namespace

MediaNodeRuntime::MediaNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name)
    : m_nodeId(nodeId)
    , m_kind(kind)
    , m_name(std::move(name))
{
}

MediaNodeId MediaNodeRuntime::nodeId() const noexcept
{
    return m_nodeId;
}

MediaNodeKind MediaNodeRuntime::kind() const noexcept
{
    return m_kind;
}

const std::string& MediaNodeRuntime::name() const noexcept
{
    return m_name;
}

::media::Status MediaNodeRuntime::process(MediaGraphExecutionContext& context)
{
    if (context.diagnosticsEnabled()) {
        std::ostringstream begin;
        begin << "process.begin node=" << m_nodeId.value
              << " name=" << m_name
              << " kind=" << mediaGraphDiagnosticNodeKindName(m_kind)
              << " inputs=" << describeChannels(context.inputChannels(m_nodeId))
              << " outputs=" << describeChannels(context.outputChannels(m_nodeId));
        mediaGraphDiagnosticLog(true, MediaGraphDiagnosticPhase::RuntimeNode, begin.str());
    }

    auto status = onProcess(context);

    if (context.diagnosticsEnabled()) {
        std::ostringstream end;
        end << "process." << (status ? "done" : "failed")
            << " node=" << m_nodeId.value
            << " name=" << m_name
            << " kind=" << mediaGraphDiagnosticNodeKindName(m_kind)
            << " inputs=" << describeChannels(context.inputChannels(m_nodeId))
            << " outputs=" << describeChannels(context.outputChannels(m_nodeId));
        if (!status) {
            end << " error=" << status.error().describe();
        }
        mediaGraphDiagnosticLog(true, MediaGraphDiagnosticPhase::RuntimeNode, end.str());
    }

    return status;
}

::media::Status MediaNodeRuntime::onProcess(MediaGraphExecutionContext&)
{
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
