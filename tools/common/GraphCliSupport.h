#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "media_transcode/Result.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace media::ffmpeg::graph::cli {

inline std::string argValue(int argc, char** argv, const std::string& key)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return argv[i + 1];
        }
    }
    return {};
}

inline std::string argValue(int argc, char** argv, const std::string& key, const std::string& missingValue)
{
    const std::string value = argValue(argc, argv, key);
    return value.empty() ? missingValue : value;
}

inline bool hasArg(int argc, char** argv, const std::string& key)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return true;
        }
    }
    return false;
}

inline void rejectRemovedArg(int argc, char** argv, const std::string& key)
{
    if (hasArg(argc, argv, key)) {
        throw std::invalid_argument(key + " was removed; encoder selection is planner-owned");
    }
}

inline std::optional<int> optionalIntArg(int argc, char** argv, const std::string& key)
{
    const std::string value = argValue(argc, argv, key);
    if (value.empty()) {
        return std::nullopt;
    }

    std::size_t parsed = 0;
    const int result = std::stoi(value, &parsed, 10);
    if (parsed != value.size()) {
        throw std::invalid_argument("invalid integer value for " + key + ": " + value);
    }
    return result;
}

inline int requiredIntArg(int argc, char** argv, const std::string& key)
{
    auto value = optionalIntArg(argc, argv, key);
    if (!value) {
        throw std::invalid_argument("missing required integer argument: " + key);
    }
    return *value;
}

inline std::size_t requiredSizeArg(int argc, char** argv, const std::string& key)
{
    const int value = requiredIntArg(argc, argv, key);
    if (value <= 0) {
        throw std::invalid_argument(key + " must be positive");
    }
    return static_cast<std::size_t>(value);
}

inline std::string requiredArg(int argc, char** argv, const std::string& key)
{
    const std::string value = argValue(argc, argv, key);
    if (value.empty()) {
        throw std::invalid_argument("missing required argument: " + key);
    }
    return value;
}

inline bool requiredExclusiveBoolArg(int argc,
                                     char** argv,
                                     const std::string& trueKey,
                                     const std::string& falseKey)
{
    const bool trueArg = hasArg(argc, argv, trueKey);
    const bool falseArg = hasArg(argc, argv, falseKey);
    if (trueArg == falseArg) {
        throw std::invalid_argument("specify exactly one of " + trueKey + " or " + falseKey);
    }
    return trueArg;
}

inline bool enabledByDefaultBoolArg(int argc,
                                    char** argv,
                                    const std::string& redundantEnableKey,
                                    const std::string& disableKey,
                                    const std::string& settingName)
{
    if (hasArg(argc, argv, redundantEnableKey)) {
        throw std::invalid_argument(
            redundantEnableKey + " is redundant; " + settingName +
            " is enabled by default, use " + disableKey + " to disable it");
    }
    return !hasArg(argc, argv, disableKey);
}

inline MediaRateControlMode requiredRateControlArg(int argc, char** argv, const std::string& key)
{
    const std::string value = requiredArg(argc, argv, key);
    MediaRateControlMode mode = MediaRateControlMode::Auto;
    if (!parseMediaRateControlMode(value, mode)) {
        throw std::invalid_argument("unsupported rate control mode for " + key + ": " + value);
    }
    return mode;
}

inline MediaRateControlMode rateControlArg(int argc, char** argv, const std::string& key)
{
    MediaRateControlMode mode = MediaRateControlMode::Auto;
    const std::string value = argValue(argc, argv, key);
    if (!parseMediaRateControlMode(value, mode)) {
        throw std::invalid_argument("unsupported rate control mode for " + key + ": " + value);
    }
    return mode;
}

inline int failStatus(const char* action, const ::media::Status& status)
{
    std::cerr << "[CLI] " << action << " failed: " << status.error().describe() << '\n';
    return 1;
}

template <typename T>
int failResult(const char* action, const ::media::Result<T>& result)
{
    std::cerr << "[CLI] " << action << " failed: " << result.error().describe() << '\n';
    return 1;
}

inline const MediaNode* findNodeByKind(const MediaGraph& graph, MediaNodeKind kind)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == kind) {
            return &node;
        }
    }
    return nullptr;
}

inline ::media::Status printRealtimePlanSummary(const MediaGraph& graph)
{
    const MediaNode* encoder = findNodeByKind(graph, MediaNodeKind::VideoEncode);
    if (!encoder) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("realtime graph plan summary requires VideoEncode node"));
    }

    auto chain = requiredNodeOption(&encoder->options, "graph CLI realtime plan summary", "pipeline.chain");
    if (!chain) {
        return ::media::Status::failure(chain.error());
    }
    auto score = requiredNodeOption(&encoder->options, "graph CLI realtime plan summary", "pipeline.score");
    if (!score) {
        return ::media::Status::failure(score.error());
    }
    auto decoder = requiredNodeOption(&encoder->options, "graph CLI realtime plan summary", "decoder.pipeline.ffmpeg");
    if (!decoder) {
        return ::media::Status::failure(decoder.error());
    }
    auto filter = requiredNodeOption(&encoder->options, "graph CLI realtime plan summary", "filter.pipeline.filter");
    if (!filter) {
        return ::media::Status::failure(filter.error());
    }
    auto encoderName = requiredNodeOption(&encoder->options, "graph CLI realtime plan summary", "encoder");
    if (!encoderName) {
        return ::media::Status::failure(encoderName.error());
    }

    std::cout << "[CLI] selected_chain=" << chain.value()
              << " score=" << score.value()
              << " decoder=" << decoder.value()
              << " filter=" << filter.value()
              << " encoder=" << encoderName.value()
              << '\n';
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph::cli
