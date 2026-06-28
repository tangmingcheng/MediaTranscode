#pragma once

#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaGraphChannelTrace {
    std::string label;
    std::size_t queued = 0;
    std::size_t capacity = 0;
    std::uint64_t pushed = 0;
    std::uint64_t popped = 0;
    std::uint64_t dropped = 0;
    std::uint64_t blockedPushes = 0;
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
            << " totalQueued=" << totalQueued << '\n';

        for (const auto& channel : channels) {
            oss << "[graph][runtime.channel] " << channel.label
                << " qsize=" << channel.queued << '/' << channel.capacity
                << " pushed=" << channel.pushed
                << " popped=" << channel.popped
                << " dropped=" << channel.dropped
                << " blocked=" << channel.blockedPushes
                << " backpressure=" << (channel.backpressure ? "true" : "false")
                << '\n';
        }

        for (const auto& node : nodes) {
            oss << "[graph][runtime.node] " << node.name
                << " input_qsize=" << node.inputQueued
                << " output_qsize=" << node.outputQueued
                << " stalled=" << (node.stalled ? "true" : "false")
                << " reason=" << node.stallReason
                << '\n';
        }

        return oss.str();
    }
};

class MediaGraphRuntimeTracer final {
public:
    static MediaGraphRuntimeTrace capture(const MediaGraphRuntime& runtime)
    {
        MediaGraphRuntimeTrace trace;

        const MediaGraphExecutionContext& context = runtime.context();
        const MediaGraph* graph = context.graph();
        if (!graph) {
            return trace;
        }

        for (const MediaEdge& edge : graph->edges()) {
            const MediaChannel* channel = context.channels().findByEdge(edge.id);

            MediaGraphChannelTrace channelTrace;
            channelTrace.label = edge.name.empty()
                ? "edge#" + std::to_string(edge.id.value)
                : edge.name;

            if (channel) {
                const auto& metrics = channel->metrics();
                channelTrace.queued = channel->size();
                channelTrace.capacity = channel->capacity();
                channelTrace.pushed = metrics.pushed;
                channelTrace.popped = metrics.popped;
                channelTrace.dropped = metrics.queue.dropped;
                channelTrace.blockedPushes = metrics.queue.blockedPushes;
                channelTrace.backpressure = channelTrace.capacity > 0 &&
                    channelTrace.queued >= channelTrace.capacity;
            }

            trace.totalQueued += channelTrace.queued;
            trace.channels.push_back(std::move(channelTrace));
        }

        for (const MediaNode& node : graph->nodes()) {
            MediaGraphNodeTrace nodeTrace;
            nodeTrace.name = node.diagnosticName.empty() ? node.name : node.diagnosticName;

            for (const MediaChannel* channel : context.inputChannels(node.id)) {
                if (channel) {
                    nodeTrace.inputQueued += channel->size();
                }
            }
            for (const MediaChannel* channel : context.outputChannels(node.id)) {
                if (channel) {
                    nodeTrace.outputQueued += channel->size();
                }
            }

            nodeTrace.stalled = nodeTrace.outputQueued > 0 && nodeTrace.inputQueued == 0;
            nodeTrace.stallReason = nodeTrace.stalled ? "output_pressure_or_idle" : "running";

            trace.nodes.push_back(std::move(nodeTrace));
        }

        return trace;
    }
};

} // namespace media::ffmpeg::graph
