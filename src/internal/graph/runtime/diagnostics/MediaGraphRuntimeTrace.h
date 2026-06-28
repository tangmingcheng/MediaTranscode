#pragma once

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include <string>
#include <vector>
#include <sstream>

namespace media::ffmpeg::graph {

struct MediaGraphChannelTrace {
    std::string label;
    std::size_t queued = 0;
    std::size_t capacity = 0;
    uint64_t pushed = 0;
    uint64_t popped = 0;
    uint64_t dropped = 0;
    bool backpressure = false;
};

struct MediaGraphNodeTrace {
    std::string name;
    std::size_t inputQueued = 0;
    std::size_t outputQueued = 0;
    bool stalled = false;
    std::string stallReason;
};

struct MediaGraphRuntimeTrace {
    std::vector<MediaGraphNodeTrace> nodes;
    std::vector<MediaGraphChannelTrace> channels;
    std::size_t totalQueued = 0;

    std::string formatText() const
    {
        std::ostringstream oss;
        oss << "[graph][trace] nodes=" << nodes.size()
            << " channels=" << channels.size()
            << " totalQueued=" << totalQueued << "\n";

        for (const auto& c : channels) {
            oss << "[channel] " << c.label
                << " q=" << c.queued << "/" << c.capacity
                << " pushed=" << c.pushed
                << " popped=" << c.popped
                << " dropped=" << c.dropped
                << " bp=" << (c.backpressure ? 1 : 0)
                << "\n";
        }

        for (const auto& n : nodes) {
            oss << "[node] " << n.name
                << " inQ=" << n.inputQueued
                << " outQ=" << n.outputQueued
                << " stalled=" << (n.stalled ? 1 : 0)
                << " reason=" << n.stallReason
                << "\n";
        }

        return oss.str();
    }
};

class MediaGraphRuntimeTracer {
public:
    static MediaGraphRuntimeTrace capture(const MediaGraphRuntime& runtime)
    {
        MediaGraphRuntimeTrace trace;

        const auto& ctx = runtime.context();
        const auto* graph = ctx.graph();
        if (!graph) {
            return trace;
        }

        for (const auto& e : graph->edges()) {
            const MediaChannel* ch = ctx.channels().findOutputChannel(e.from.nodeId, e.from.portId ? "" : "");

            MediaGraphChannelTrace ct;
            ct.label = e.name;
            ct.queued = ch ? ch->size() : 0;
            ct.capacity = ch ? ch->capacity() : 0;
            ct.pushed = ch ? ch->metrics().pushed : 0;
            ct.popped = ch ? ch->metrics().popped : 0;
            ct.dropped = ch ? ch->metrics().queue.dropped : 0;
            ct.backpressure = (ct.capacity > 0 && ct.queued >= ct.capacity);

            trace.totalQueued += ct.queued;
            trace.channels.push_back(std::move(ct));
        }

        for (const auto& node : graph->nodes()) {
            MediaGraphNodeTrace nt;
            nt.name = node.name;

            std::size_t inQ = 0;
            std::size_t outQ = 0;

            for (const auto& p : node.inputPorts) {
                if (auto* ch = ctx.findInputChannel(node.id, p.name)) {
                    inQ += ch->size();
                }
            }
            for (const auto& p : node.outputPorts) {
                if (auto* ch = ctx.findOutputChannel(node.id, p.name)) {
                    outQ += ch->size();
                }
            }

            nt.inputQueued = inQ;
            nt.outputQueued = outQ;
            nt.stalled = (outQ > 0 && inQ == 0);
            nt.stallReason = nt.stalled ? "output_pressure_or_idle" : "running";

            trace.nodes.push_back(std::move(nt));
        }

        return trace;
    }
};

} // namespace media::ffmpeg::graph
