#include "internal/graph/nodes/MediaNodeRuntime.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <sstream>
#include <utility>
#include <vector>

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

std::string processDiagnosticMessage(const char* action,
                                     MediaNodeId nodeId,
                                     const std::string& name,
                                     MediaNodeKind kind,
                                     MediaGraphExecutionContext& context,
                                     const ::media::Status* status = nullptr,
                                     std::uint64_t sequence = 0)
{
    std::ostringstream out;
    out << action;
    if (sequence > 0) {
        out << " seq=" << sequence;
    }
    out << " node=" << nodeId.value
        << " name=" << name
        << " kind=" << mediaGraphDiagnosticNodeKindName(kind)
        << " inputs=" << describeChannels(context.inputChannels(nodeId))
        << " outputs=" << describeChannels(context.outputChannels(nodeId));
    if (status && !*status) {
        out << " error=" << status->error().describe();
    }
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

::media::Result<MediaNodeProcessResult> MediaNodeRuntime::process(MediaGraphExecutionContext& context)
{
    const std::string beginKey = "node:" + std::to_string(m_nodeId.value) + ":process.begin";
    auto beginDecision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Trace, beginKey);
    if (beginDecision.shouldLog) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::Trace,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                processDiagnosticMessage("process.begin",
                                                         m_nodeId,
                                                         m_name,
                                                         m_kind,
                                                         context,
                                                         nullptr,
                                                         beginDecision.sequence));
    }

    auto outcome = onProcess(context);

    if (!outcome) {
        auto status = ::media::Status::failure(outcome.error());
        const char* action = outcome.error().code == ::media::ErrorCode::WouldBlock
            ? "process.blocked"
            : "process.failed";
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                processDiagnosticMessage(action,
                                                         m_nodeId,
                                                         m_name,
                                                         m_kind,
                                                         context,
                                                         &status));
        return outcome;
    }

    const std::string doneKey = "node:" + std::to_string(m_nodeId.value) + ":process.done";
    auto doneDecision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Trace, doneKey);
    if (doneDecision.shouldLog) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::Trace,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                processDiagnosticMessage("process.done",
                                                         m_nodeId,
                                                         m_name,
                                                         m_kind,
                                                         context,
                                                         nullptr,
                                                         doneDecision.sequence));
    }

    if (outcome.value().state == MediaNodeProcessState::Finished && canFinishProcess()) {
        for (MediaChannel* channel : context.outputChannels(m_nodeId)) {
            channel->close();
        }
    }
    return outcome;
}

bool MediaNodeRuntime::canFinishProcess() const noexcept
{
    return true;
}

::media::Result<MediaNodeProcessResult> MediaNodeRuntime::onProcess(MediaGraphExecutionContext&)
{
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
}

::media::Result<MediaNodeProcessResult> MediaNodeRuntime::processProgress(::media::Status status)
{
    return status ? ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress())
                  : ::media::Result<MediaNodeProcessResult>::failure(status.error());
}

::media::Result<MediaNodeProcessResult> MediaNodeRuntime::processWaiting()
{
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
}

::media::Result<MediaNodeProcessResult> MediaNodeRuntime::processFinished(::media::Status status)
{
    return status ? ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished())
                  : ::media::Result<MediaNodeProcessResult>::failure(status.error());
}

} // namespace media::ffmpeg::graph
