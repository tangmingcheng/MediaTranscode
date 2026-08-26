#include "internal/graph/planner/realtime/MediaGraphPayloadProducerRegistryCompiler.h"

#include <algorithm>
#include <charconv>
#include <new>

namespace media::ffmpeg::graph {
namespace {

bool allocatesPayload(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RawRtpInput:
    case MediaNodeKind::Demux:
    case MediaNodeKind::MpegTsDemux:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::HardwareTransfer:
    case MediaNodeKind::VideoFilter:
    case MediaNodeKind::VideoEncode:
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
    case MediaNodeKind::AudioEncode:
        return true;
    default:
        return false;
    }
}

bool runtimeIntegrated(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RawRtpInput:
    case MediaNodeKind::Demux:
    case MediaNodeKind::MpegTsDemux:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::HardwareTransfer:
    case MediaNodeKind::VideoFilter:
    case MediaNodeKind::VideoEncode:
    case MediaNodeKind::AudioEncode:
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
        return true;
    default:
        return false;
    }
}

::media::Result<std::uint64_t> maximumBytes(
    MediaNodeKind producerKind,
    const MediaEdge& edge,
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    if (producerKind == MediaNodeKind::RawRtpInput ||
        producerKind == MediaNodeKind::Demux ||
        producerKind == MediaNodeKind::MpegTsDemux ||
        producerKind == MediaNodeKind::PacketNormalize) {
        if (!ledger.preparedInputPayload ||
            !ledger.preparedInputPayload->validate()) {
            return ::media::Result<std::uint64_t>::failure(
                ::media::ErrorInfo::unsupported(
                    "producer registry lacks a prepared input allocation envelope"));
        }
        const auto expectedSource = producerKind == MediaNodeKind::RawRtpInput
            ? MediaPreparedInputPayloadSource::RawRtpAccessUnit
            : producerKind == MediaNodeKind::Demux
            ? MediaPreparedInputPayloadSource::GenericDemuxPacket
            : producerKind == MediaNodeKind::MpegTsDemux
                ? MediaPreparedInputPayloadSource::MpegTsPesPacket
                : ledger.preparedInputPayload->source;
        if (ledger.preparedInputPayload->source != expectedSource) {
            return ::media::Result<std::uint64_t>::failure(
                ::media::ErrorInfo::unsupported(
                    "producer registry input source conflicts with the final DAG"));
        }
        const auto* bound = ledger.preparedInputPayload->find(edge.streamKind);
        if (!bound) {
            return ::media::Result<std::uint64_t>::failure(
                ::media::ErrorInfo::unsupported(
                    "producer registry input envelope lacks the selected stream"));
        }
        return ::media::Result<std::uint64_t>::success(
            bound->maximumPayloadBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Packet &&
        edge.streamKind == MediaStreamKind::Video &&
        producerKind == MediaNodeKind::VideoEncode) {
        return ::media::Result<std::uint64_t>::success(
            ledger.media.videoUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Packet &&
        edge.streamKind == MediaStreamKind::Audio &&
        producerKind == MediaNodeKind::AudioEncode &&
        ledger.media.audioUnitBytes) {
        return ::media::Result<std::uint64_t>::success(
            *ledger.media.audioUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Frame &&
        edge.streamKind == MediaStreamKind::Video &&
        (producerKind == MediaNodeKind::VideoDecode ||
         producerKind == MediaNodeKind::HardwareTransfer ||
         producerKind == MediaNodeKind::VideoFilter)) {
        return ::media::Result<std::uint64_t>::success(
            ledger.videoSurfaceUnitBytes);
    }
    if (edge.payloadKind == MediaPayloadKind::Frame &&
        edge.streamKind == MediaStreamKind::Audio &&
        (producerKind == MediaNodeKind::AudioDecode ||
         producerKind == MediaNodeKind::AudioStartupTrim ||
         producerKind == MediaNodeKind::AudioResample) &&
        ledger.audioFrameUnitBytes) {
        return ::media::Result<std::uint64_t>::success(
            *ledger.audioFrameUnitBytes);
    }
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::unsupported(
            "producer registry lacks a prepared payload bound"));
}

bool sameKey(
    const MediaGraphPayloadProducerStrategy& strategy,
    const MediaEdge& edge) noexcept
{
    return strategy.nodeId == edge.from.nodeId &&
        strategy.streamKind == edge.streamKind &&
        strategy.payloadKind == edge.payloadKind;
}

::media::Result<MediaFrameCreditContract> frameCreditContract(
    const MediaNode& node,
    std::uint64_t maximumLogicalBytes)
{
    using Result = ::media::Result<MediaFrameCreditContract>;
    std::string prefix;
    if (node.kind == MediaNodeKind::VideoDecode) {
        prefix = "decoder.pipeline.output";
    } else if (node.kind == MediaNodeKind::VideoFilter) {
        prefix = "filter.pipeline.output";
    } else if (node.kind == MediaNodeKind::HardwareTransfer) {
        if (!node.options.has("transfer.direction")) {
            return Result::failure(::media::ErrorInfo::notInitialized(
                "hardware transfer frame credit lacks planner direction"));
        }
        const auto direction = node.options.value("transfer.direction");
        if (direction == "none") {
            prefix = "decoder.pipeline.output";
        } else if (direction == "download") {
            if (!node.options.has("pipeline.filter_active")) {
                return Result::failure(::media::ErrorInfo::notInitialized(
                    "hardware transfer frame credit lacks filter topology fact"));
            }
            const auto filterActive =
                node.options.value("pipeline.filter_active");
            if (filterActive != "0" && filterActive != "1") {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "hardware transfer frame credit has invalid filter topology fact"));
            }
            prefix = filterActive == "1"
                ? "filter.pipeline.input" : "encoder.pipeline.input";
        } else {
            return Result::failure(::media::ErrorInfo::unsupported(
                "hardware transfer frame credit has no admitted output contract"));
        }
    } else {
        return Result::failure(::media::ErrorInfo::unsupported(
            "frame payload producer lacks a typed frame credit resolver"));
    }

    const std::string presentKey = prefix + ".present";
    const std::string deviceKey = prefix + ".device";
    const std::string kindKey = prefix + ".frame_kind";
    const std::string pixelFormatKey = prefix + ".pixel_format";
    const std::string widthKey = prefix + ".width";
    const std::string heightKey = prefix + ".height";
    if (!node.options.has(presentKey) ||
        node.options.value(presentKey) != "1" ||
        !node.options.has(deviceKey) ||
        !node.options.has(kindKey) ||
        !node.options.has(pixelFormatKey) ||
        node.options.value(pixelFormatKey).empty() ||
        !node.options.has(widthKey) || !node.options.has(heightKey)) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "frame payload producer lacks a complete planner frame contract"));
    }
    const auto positiveDimension = [&](const std::string& key) {
        int value = 0;
        const auto text = node.options.value(key);
        const auto parsed = std::from_chars(
            text.data(), text.data() + text.size(), value);
        return parsed.ec == std::errc{} &&
            parsed.ptr == text.data() + text.size() && value > 0;
    };
    if (!positiveDimension(widthKey) || !positiveDimension(heightKey)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "frame payload producer has invalid planner dimensions"));
    }
    const auto frameKind = node.options.value(kindKey);
    const auto device = node.options.value(deviceKey);
    MediaFrameCreditAllocationScope allocationScope;
    if (frameKind == "software") {
        if (device != "software") {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "software frame credit conflicts with planner device"));
        }
        allocationScope = MediaFrameCreditAllocationScope::EngineLogicalBytes;
    } else if (frameKind == "hardware" ||
               frameKind == "hardware_mapped") {
        if (device.empty() || device == "software" || device == "unknown") {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "device frame credit conflicts with planner device"));
        }
        allocationScope =
            MediaFrameCreditAllocationScope::ExternalDeviceObservedOnly;
    } else {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "frame payload producer has unsupported planner frame kind"));
    }
    return Result::success(MediaFrameCreditContract{
        allocationScope, maximumLogicalBytes, 1,
        prefix + "+opened-frame-readback+device=" + device});
}

std::string allocationAuthority(
    MediaNodeKind producerKind,
    const MediaEdge& edge,
    const MediaRealtimeGraphResourceLedgerPlan& ledger,
    bool deviceBacked)
{
    if ((producerKind == MediaNodeKind::RawRtpInput ||
         producerKind == MediaNodeKind::Demux ||
         producerKind == MediaNodeKind::MpegTsDemux ||
         producerKind == MediaNodeKind::PacketNormalize) &&
        ledger.preparedInputPayload) {
        const auto* bound = ledger.preparedInputPayload->find(edge.streamKind);
        if (bound) return bound->authority;
    }
    return deviceBacked
        ? "prepared-logical-frame-bound+device-bytes-observed-only"
        : "prepared-encoder-emission-or-frame-footprint-bound";
}

bool sameKey(
    const MediaGraphPayloadProducerRequirement& requirement,
    const MediaEdge& edge) noexcept
{
    return requirement.nodeId == edge.from.nodeId &&
        requirement.streamKind == edge.streamKind &&
        requirement.payloadKind == edge.payloadKind;
}

} // namespace

::media::Result<MediaGraphPayloadCreditPlan>
MediaGraphPayloadProducerRegistryCompiler::compile(
    const MediaGraph& graph,
    const MediaRealtimeGraphResourceLedgerPlan& planningLedger,
    std::uint64_t availablePayloadBytes,
    std::uint64_t maximumPayloadObjects)
{
    using Result = ::media::Result<MediaGraphPayloadCreditPlan>;
    if (availablePayloadBytes == 0 || maximumPayloadObjects == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "payload producer registry requires positive global credits"));
    }
    MediaGraphPayloadCreditPlan plan;
    plan.maximumBytes = availablePayloadBytes;
    plan.maximumObjects = maximumPayloadObjects;
    plan.producerStrategyVersion = 1;
    plan.integration = MediaGraphPayloadCreditIntegration::Complete;
    plan.authority =
        "final-dag-producer-registry+prepared-emission+global-payload-budget";
    try {
        for (const auto& node : graph.nodes()) {
            if (!allocatesPayload(node.kind)) continue;
            bool foundProducerEdge = false;
            for (const auto& edge : graph.edges()) {
                if (edge.from.nodeId != node.id ||
                    (edge.payloadKind != MediaPayloadKind::Packet &&
                     edge.payloadKind != MediaPayloadKind::Frame)) {
                    continue;
                }
                foundProducerEdge = true;
                if (std::any_of(
                        plan.producers.begin(), plan.producers.end(),
                        [&](const auto& strategy) {
                            return sameKey(strategy, edge);
                        })) {
                    continue;
                }
                auto bound = maximumBytes(node.kind, edge, planningLedger);
                if (!bound) {
                    if (!std::any_of(
                            plan.missingProducers.begin(),
                            plan.missingProducers.end(),
                            [&](const auto& requirement) {
                                return sameKey(requirement, edge);
                            })) {
                        plan.missingProducers.push_back(
                            MediaGraphPayloadProducerRequirement{
                                node.id, edge.streamKind, edge.payloadKind,
                                bound.error().message});
                    }
                    continue;
                }
                if (bound.value() == 0) {
                    return Result::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "single producer payload bound is zero"));
                }
                const bool isFramePayload = edge.payloadKind ==
                    MediaPayloadKind::Frame;
                std::optional<MediaFrameCreditContract> frameCredit;
                if (isFramePayload) {
                    auto contract = frameCreditContract(node, bound.value());
                    if (!contract) return Result::failure(contract.error());
                    frameCredit = std::move(contract).value();
                }
                const bool externalDeviceFrame = frameCredit &&
                    frameCredit->allocationScope ==
                        MediaFrameCreditAllocationScope::
                            ExternalDeviceObservedOnly;
                if (!externalDeviceFrame &&
                    bound.value() > availablePayloadBytes) {
                    return Result::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "single producer payload exceeds the global credit pool"));
                }
                plan.producers.push_back(MediaGraphPayloadProducerStrategy{
                    node.id, edge.streamKind, edge.payloadKind,
                    externalDeviceFrame
                        ? MediaGraphPayloadAllocationAccounting::
                              ObservedOnlyExternalBytesAndEngineManagedObject
                        : MediaGraphPayloadAllocationAccounting::
                              EngineManagedBytesAndObject,
                    std::move(frameCredit),
                    bound.value(),
                    runtimeIntegrated(node.kind),
                    allocationAuthority(
                        node.kind, edge, planningLedger,
                        externalDeviceFrame)});
                plan.maximumUnitBytes =
                    (std::max)(plan.maximumUnitBytes, bound.value());
            }
            if (!foundProducerEdge) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "final DAG payload producer has no typed packet/frame output"));
            }
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "payload producer registry"));
    }
    if (!plan.missingProducers.empty() || std::any_of(
            plan.producers.begin(), plan.producers.end(),
            [](const auto& strategy) {
                return !strategy.runtimeIntegrated;
            })) {
        plan.integration = MediaGraphPayloadCreditIntegration::Incomplete;
    }
    if (!plan.isStructurallyValid()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "payload producer registry is structurally incomplete"));
    }
    return Result::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
