#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t RetainLatestItemCount = 1;

::media::Result<std::uint64_t> residenceUnits(
    std::uint64_t numerator,
    std::uint64_t denominator,
    MediaRunningTime residence,
    const char* fact)
{
    if (residence.nanoseconds() <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "graph resource residence must be positive"));
    }
    auto scaledDenominator = MediaCheckedArithmetic::multiply(
        denominator, 1'000'000'000ULL, fact);
    auto inWindow = scaledDenominator
        ? MediaCheckedArithmetic::ceilScale(
              static_cast<std::uint64_t>(residence.nanoseconds()), numerator,
              scaledDenominator.value(), fact)
        : scaledDenominator;
    return inWindow
        ? MediaCheckedArithmetic::add(
              inWindow.value(), 1U, fact)
        : inWindow;
}

::media::Result<std::size_t> size(std::uint64_t value, const char* fact)
{
    if (value > static_cast<std::uint64_t>(
            (std::numeric_limits<std::size_t>::max)())) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " exceeds platform size_t"));
    }
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(value));
}

} // namespace

::media::Result<MediaRealtimeGraphResourceLedgerPlan>
MediaRealtimeGraphResourceLedgerPlanner::plan(
    MediaRealtimeDeploymentLatencyBudget latency,
    const MediaPreparedRealtimeEmissionSet& emission,
    const std::vector<MediaRealtimeVideoSurfaceFootprintFact>& videoSurfaces,
    bool hardwareSurface)
{
    using Result = ::media::Result<MediaRealtimeGraphResourceLedgerPlan>;
    const auto resourceScope = MediaRealtimeGraphResourceBudgetScope::
        EngineManagedPayloadAndReservedStorage;
    if (emission.hardwareMemory) {
        if (auto status = emission.hardwareMemory->validate(); !status) {
            return Result::failure(status.error());
        }
    }
    const auto residence = latency.maximumResidence;
    auto encodedVideoUnits = residenceUnits(
        emission.video.accessUnitsPerSecondNumerator,
        emission.video.accessUnitsPerSecondDenominator,
        residence, "encoded video residence units");
    if (videoSurfaces.empty()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "graph resource ledger requires prepared video surface footprints"));
    }
    std::uint64_t maximumSurfaceUnitBytes = 0;
    std::uint64_t maximumSurfaceUnits = 0;
    for (const auto& surface : videoSurfaces) {
        if (surface.authority.empty() || surface.pixelFormat.empty() ||
            !surface.productionRate.isKnown() ||
            surface.productionRate.num <= 0 ||
            surface.productionRate.den <= 0) {
            return Result::failure(::media::ErrorInfo::notInitialized(
                "graph resource ledger requires authoritative video surface and production-rate facts"));
        }
        auto bytes = MediaVideoSurfaceFootprint::logicalBytes(
            surface.width, surface.height, surface.pixelFormat);
        auto units = residenceUnits(
            static_cast<std::uint64_t>(surface.productionRate.num),
            static_cast<std::uint64_t>(surface.productionRate.den),
            residence, surface.authority.c_str());
        if (!bytes || !units) {
            return Result::failure(!bytes ? bytes.error() : units.error());
        }
        maximumSurfaceUnitBytes = (std::max)(
            maximumSurfaceUnitBytes, bytes.value());
        maximumSurfaceUnits = (std::max)(
            maximumSurfaceUnits, units.value());
    }
    auto surfaceUnitBytes = ::media::Result<std::uint64_t>::success(
        maximumSurfaceUnitBytes);
    auto videoPacketBytes = encodedVideoUnits
        ? MediaCheckedArithmetic::multiply(
              encodedVideoUnits.value(),
              emission.video.maximumAccessUnitPayloadBytes,
              "encoded video ledger bytes")
        : encodedVideoUnits;
    auto surfaceBytes = surfaceUnitBytes
        ? MediaCheckedArithmetic::multiply(
              maximumSurfaceUnits, surfaceUnitBytes.value(),
              "video surface ledger bytes")
        : surfaceUnitBytes;
    if (!encodedVideoUnits || !surfaceUnitBytes || !videoPacketBytes ||
        !surfaceBytes || emission.video.maximumAccessUnitPayloadBytes == 0) {
        return Result::failure(
            !encodedVideoUnits ? encodedVideoUnits.error() :
            !surfaceUnitBytes ? surfaceUnitBytes.error() :
            !videoPacketBytes ? videoPacketBytes.error() :
            !surfaceBytes ? surfaceBytes.error() :
            ::media::ErrorInfo::notInitialized(
                "prepared video emission lacks a maximum access unit"));
    }

    std::uint64_t audioUnitCount = 0;
    std::uint64_t audioBytes = 0;
    if (emission.audio) {
        if (!emission.audioFrames || !emission.audioFrames->valid()) {
            return Result::failure(::media::ErrorInfo::notInitialized(
                "audio resource planning requires authoritative prepared frame footprints"));
        }
        auto units = residenceUnits(
            emission.audio->accessUnitsPerSecondNumerator,
            emission.audio->accessUnitsPerSecondDenominator,
            residence, "audio residence units");
        auto bytes = units
            ? MediaCheckedArithmetic::multiply(
                  units.value(),
                  emission.audio->maximumAccessUnitPayloadBytes,
                  "encoded audio ledger bytes")
            : units;
        if (!units || !bytes ||
            emission.audio->maximumAccessUnitPayloadBytes == 0) {
            return Result::failure(
                !units ? units.error() : !bytes ? bytes.error() :
                ::media::ErrorInfo::notInitialized(
                    "prepared audio emission lacks a maximum access unit"));
        }
        audioUnitCount = units.value();
        audioBytes = bytes.value();
    }

    auto packetItems = MediaCheckedArithmetic::add(
        encodedVideoUnits.value(), audioUnitCount, "aggregate packet items");
    auto ownedPayload = MediaCheckedArithmetic::add(
        videoPacketBytes.value(), audioBytes, "shared encoded payload bytes");
    auto withSurfaces = ownedPayload
        ? MediaCheckedArithmetic::add(
              ownedPayload.value(), surfaceBytes.value(),
              "media payload and surface bytes")
        : ownedPayload;
    auto videoCount = size(
        encodedVideoUnits.value(), "encoded video queue items");
    auto surfaceCount = size(
        maximumSurfaceUnits, "video surface queue items");
    auto packetCount = packetItems
        ? size(packetItems.value(), "packet queue items")
        : ::media::Result<std::size_t>::failure(packetItems.error());
    auto audioCount = emission.audio
        ? size(audioUnitCount, "audio queue items")
        : ::media::Result<std::size_t>::success(0);
    if (!withSurfaces || !videoCount || !surfaceCount || !packetCount ||
        !audioCount) {
        return Result::failure(
            !withSurfaces ? withSurfaces.error() : !videoCount ? videoCount.error() :
            !surfaceCount ? surfaceCount.error() :
            !packetCount ? packetCount.error() : audioCount.error());
    }
    const auto frameCount = (std::max)(
        surfaceCount.value(), audioCount.value());

    try {
        std::vector<MediaRealtimeGraphResourceLedgerEntry> entries;
        entries.reserve(emission.audio ? 5U : 4U);
        entries.push_back({
            MediaRealtimeResourceAccountingGroup::EncodedVideoPacket,
            MediaRealtimeQueueRetentionSemantics::BoundedFifo,
            encodedVideoUnits.value(), videoPacketBytes.value(),
            emission.video.authority + "+shared-packet-ownership"});
        entries.push_back({
            MediaRealtimeResourceAccountingGroup::DecodedVideoSurface,
            MediaRealtimeQueueRetentionSemantics::BoundedFifo,
            maximumSurfaceUnits, surfaceBytes.value(),
            hardwareSurface
                ? "selected-pipeline-maximum-hardware-surface-logical-bytes"
                : "selected-pipeline-maximum-software-frame-logical-bytes"});
        if (emission.audio) {
            entries.push_back({
                MediaRealtimeResourceAccountingGroup::EncodedAudioPacket,
                MediaRealtimeQueueRetentionSemantics::BoundedFifo,
                audioUnitCount, audioBytes,
                emission.audio->authority + "+shared-packet-ownership"});
        }
        entries.push_back({
            MediaRealtimeResourceAccountingGroup::MuxDescriptor,
            MediaRealtimeQueueRetentionSemantics::BoundedFifo,
            packetItems.value(), 0,
            "count-only-mux-descriptor-retention"});
        entries.push_back({
            MediaRealtimeResourceAccountingGroup::RetainLatestMetadata,
            MediaRealtimeQueueRetentionSemantics::RetainLatest,
            RetainLatestItemCount, 0,
            "count-only-retain-latest-protocol-state"});

        MediaRealtimeMediaCapacityPlan media{
            videoCount.value(), emission.video.maximumAccessUnitPayloadBytes,
            videoPacketBytes.value(),
            emission.audio ? std::optional<std::size_t>(audioCount.value())
                           : std::nullopt,
            emission.audio
                ? std::optional<std::uint64_t>(
                      emission.audio->maximumAccessUnitPayloadBytes)
                : std::nullopt,
            emission.audio ? std::optional<std::uint64_t>(audioBytes)
                           : std::nullopt,
            residence};
        return Result::success(MediaRealtimeGraphResourceLedgerPlan{
            MediaGraphQueueParameters{
                static_cast<std::size_t>(RetainLatestItemCount),
                packetCount.value(), frameCount, packetCount.value()},
            std::move(media), resourceScope,
            withSurfaces.value(),
            surfaceUnitBytes.value(),
            emission.audioFrames
                ? std::optional<std::uint64_t>(
                      emission.audioFrames->maximumFrameBytes)
                : std::nullopt,
            emission.hardwareMemory, emission.video.maximumEncoderRetainedFrames,
            hardwareSurface,
            std::nullopt,
            std::move(entries)});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "realtime graph resource ledger"));
    }
}

::media::Result<MediaRealtimeGraphResourceLedgerPlan>
MediaRealtimeGraphResourceLedgerPlanner::admitPreparedInput(
    MediaRealtimeGraphResourceLedgerPlan ledger,
    MediaPreparedInputPayloadEnvelope payload,
    std::uint64_t reservedStorageBytes,
    std::string reservedStorageAuthority)
{
    using Result = ::media::Result<MediaRealtimeGraphResourceLedgerPlan>;
    if (ledger.preparedInputPayload) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "graph resource ledger already contains a prepared input envelope"));
    }
    if (auto status = payload.validate(); !status) {
        return Result::failure(status.error());
    }
    if ((reservedStorageBytes == 0) != reservedStorageAuthority.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "prepared input reserved storage requires matching bytes and authority"));
    }

    std::uint64_t payloadCreditBytes = 0;
    std::uint64_t payloadCreditObjects = 0;
    for (const auto& stream : payload.streams) {
        auto streamBytes = MediaCheckedArithmetic::multiply(
            stream.maximumPayloadBytes,
            payload.maximumPayloadsPerInputCompletion,
            "prepared input payload credit bytes");
        auto totalBytes = streamBytes
            ? MediaCheckedArithmetic::add(
                  payloadCreditBytes, streamBytes.value(),
                  "aggregate prepared input payload credits")
            : streamBytes;
        auto totalObjects = MediaCheckedArithmetic::add(
            payloadCreditObjects,
            payload.maximumPayloadsPerInputCompletion,
            "aggregate prepared input payload credit objects");
        if (!streamBytes || !totalBytes || !totalObjects) {
            return Result::failure(
                !streamBytes ? streamBytes.error() :
                !totalBytes ? totalBytes.error() : totalObjects.error());
        }
        payloadCreditBytes = totalBytes.value();
        payloadCreditObjects = totalObjects.value();
    }
    auto withPayloadCredits = MediaCheckedArithmetic::add(
        ledger.maximumGraphPayloadAndReservedStorageBytes,
        payloadCreditBytes,
        "graph budget with prepared input payload credits");
    auto withReservedStorage = withPayloadCredits
        ? MediaCheckedArithmetic::add(
              withPayloadCredits.value(), reservedStorageBytes,
              "graph budget with prepared input reserved storage")
        : withPayloadCredits;
    if (!withPayloadCredits || !withReservedStorage) {
        return Result::failure(
            !withPayloadCredits ? withPayloadCredits.error() :
            withReservedStorage.error());
    }

    try {
        ledger.entries.push_back({
            MediaRealtimeResourceAccountingGroup::PreparedInputPacket,
            MediaRealtimeQueueRetentionSemantics::BoundedFifo,
            payloadCreditObjects,
            payloadCreditBytes,
            payload.completionAuthority +
                "+global-payload-credit-hard-bound"});
        if (reservedStorageBytes > 0) {
            ledger.entries.push_back({
                MediaRealtimeResourceAccountingGroup::
                    PreparedInputReservedStorage,
                MediaRealtimeQueueRetentionSemantics::BoundedFifo,
                1,
                reservedStorageBytes,
                std::move(reservedStorageAuthority)});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "prepared input graph resource ledger"));
    }
    ledger.maximumGraphPayloadAndReservedStorageBytes =
        withReservedStorage.value();
    ledger.preparedInputPayload = std::move(payload);
    return Result::success(std::move(ledger));
}

::media::Status MediaRealtimeGraphResourceLedgerPlanner::validate(
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    if (ledger.resourceScope ==
            MediaRealtimeGraphResourceBudgetScope::Unknown ||
        ledger.maximumGraphPayloadAndReservedStorageBytes == 0 ||
        ledger.videoSurfaceUnitBytes == 0 ||
        ledger.entries.empty() ||
        ledger.maximumEncoderRetainedFrames == 0 ||
        ledger.queues.metadata != RetainLatestItemCount ||
        ledger.queues.packet == 0 || ledger.queues.frame == 0 ||
        ledger.queues.mux == 0 || ledger.media.videoUnits == 0 ||
        ledger.media.videoUnitBytes == 0 || ledger.media.videoBytes == 0 ||
        ledger.media.maximumGap.nanoseconds() <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "realtime graph resource ledger is incomplete"));
    }
    if (ledger.media.audioUnits &&
        (!ledger.audioFrameUnitBytes || *ledger.audioFrameUnitBytes == 0)) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "audio graph resource ledger lacks prepared frame footprint"));
    }
    if (ledger.preparedInputPayload) {
        if (auto status = ledger.preparedInputPayload->validate(); !status) {
            return status;
        }
    }
    bool retainLatest = false;
    for (const auto& entry : ledger.entries) {
        if (entry.itemCount == 0 || entry.authority.empty()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "graph resource ledger entry lacks a retention count or authority"));
        }
        retainLatest = retainLatest ||
            (entry.accountingGroup ==
                 MediaRealtimeResourceAccountingGroup::RetainLatestMetadata &&
             entry.retention ==
                 MediaRealtimeQueueRetentionSemantics::RetainLatest &&
             entry.itemCount == RetainLatestItemCount);
    }
    if (ledger.resourceScope == MediaRealtimeGraphResourceBudgetScope::
            EngineManagedPayloadAndReservedStoragePlusDevice &&
        (!ledger.hardwareMemory || !ledger.hardwareMemory->validate())) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "engine-managed-plus-device ledger lacks authoritative hardware allocation evidence"));
    }
    return retainLatest
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::invalidArgument(
              "graph resource ledger lacks RetainLatest metadata semantics"));
}

} // namespace media::ffmpeg::graph
