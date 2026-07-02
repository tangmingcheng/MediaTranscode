#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph::MediaGraphBuildSupport {

MediaEdgePolicy blockingQueuePolicy(std::size_t capacity) noexcept;
MediaFormatDescriptor streamIndexDescriptor(MediaStreamKind streamKind, int streamIndex) noexcept;

::media::Result<void> setNodeOptionChecked(MediaGraph& graph,
                                           std::string_view owner,
                                           MediaNodeId nodeId,
                                           std::string_view key,
                                           std::string_view value);

::media::Result<void> setPacketStreamOptions(MediaGraph& graph,
                                             std::string_view owner,
                                             MediaNodeId nodeId,
                                             MediaStreamKind streamKind,
                                             int sourceStreamIndex);

::media::Result<void> requirePort(MediaPortId portId,
                                  std::string_view owner,
                                  std::string_view name);

::media::Result<void> requireEdge(MediaEdgeId edgeId,
                                  std::string_view owner,
                                  std::string_view name);

::media::Result<void> addInputPortChecked(MediaGraph& graph,
                                          std::string_view owner,
                                          MediaNodeId nodeId,
                                          std::string name,
                                          MediaStreamKind streamKind,
                                          MediaEdgeKind edgeKind,
                                          MediaPayloadKind payloadKind,
                                          bool required,
                                          bool multiple);

::media::Result<void> addOutputPortChecked(MediaGraph& graph,
                                           std::string_view owner,
                                           MediaNodeId nodeId,
                                           std::string name,
                                           MediaStreamKind streamKind,
                                           MediaEdgeKind edgeKind,
                                           MediaPayloadKind payloadKind,
                                           bool required,
                                           bool multiple);

::media::Result<void> connectChecked(MediaGraph& graph,
                                     std::string_view owner,
                                     MediaNodeId fromNode,
                                     const std::string& fromPort,
                                     MediaNodeId toNode,
                                     const std::string& toPort,
                                     std::string label,
                                     const MediaEdgePolicy& policy,
                                     bool required = true);

} // namespace media::ffmpeg::graph::MediaGraphBuildSupport
