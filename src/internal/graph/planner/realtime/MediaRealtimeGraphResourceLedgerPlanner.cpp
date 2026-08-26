#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"
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

::media::Result<std::uint64_t> logicalSurfaceBytes(
    int width, int height, std::string_view pixelFormat)
{
    if (width <= 0 || height <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "graph resource ledger requires prepared output dimensions"));
    }
    auto pixels = MediaCheckedArithmetic::multiply(
        static_cast<std::uint64_t>(width),
        static_cast<std::uint64_t>(height), "video surface pixels");
    if (!pixels) return pixels;
    if (pixelFormat == "nv12" || pixelFormat == "yuv420p") {
        return MediaCheckedArithmetic::ceilScale(
            pixels.value(), 3U, 2U, "8-bit 4:2:0 surface bytes");
    }
    if (pixelFormat == "p010le" || pixelFormat == "yuv420p10le") {
        return MediaCheckedArithmetic::multiply(
            pixels.value(), 3U, "10-bit 4:2:0 surface bytes");
    }
    return ::media::Result<std::uint64_t>::failure(
        ::media::ErrorInfo::unsupported(
            "graph resource ledger requires an authoritative supported surface pixel format"));
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
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaPreparedRealtimeEmissionSet& emission,
    int outputWidth,
    int outputHeight,
    std::string_view surfacePixelFormat,
    bool hardwareSurface)
{
    using Result = ::media::Result<MediaRealtimeGraphResourceLedgerPlan>;
    const auto resourceScope =
        deployment.encode().resources.graphResourceScope;
    if (resourceScope == MediaRealtimeGraphResourceBudgetScope::
            EngineManagedPayloadAndReservedStoragePlusDevice &&
        !emission.hardwareMemory) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "engine-managed-plus-device graph memory requires an authoritative prepared hardware memory envelope"));
    }
    if (emission.hardwareMemory) {
        if (auto status = emission.hardwareMemory->validate(); !status) {
            return Result::failure(status.error());
        }
    }
    const auto residence = deployment.encode().latency.maximumResidence;
    auto videoUnits = residenceUnits(
        emission.video.accessUnitsPerSecondNumerator,
        emission.video.accessUnitsPerSecondDenominator,
        residence, "video residence units");
    auto surfaceUnitBytes = logicalSurfaceBytes(
        outputWidth, outputHeight, surfacePixelFormat);
    auto videoPacketBytes = videoUnits
        ? MediaCheckedArithmetic::multiply(
              videoUnits.value(), emission.video.maximumAccessUnitPayloadBytes,
              "encoded video ledger bytes")
        : videoUnits;
    auto surfaceBytes = videoUnits && surfaceUnitBytes
        ? MediaCheckedArithmetic::multiply(
              videoUnits.value(), surfaceUnitBytes.value(),
              "video surface ledger bytes")
        : (!videoUnits ? videoUnits : surfaceUnitBytes);
    if (!videoUnits || !surfaceUnitBytes || !videoPacketBytes ||
        !surfaceBytes || emission.video.maximumAccessUnitPayloadBytes == 0) {
        return Result::failure(
            !videoUnits ? videoUnits.error() :
            !surfaceUnitBytes ? surfaceUnitBytes.error() :
            !videoPacketBytes ? videoPacketBytes.error() :
            !surfaceBytes ? surfaceBytes.error() :
            ::media::ErrorInfo::notInitialized(
                "prepared video emission lacks a maximum access unit"));
    }

    std::uint64_t audioUnitCount = 0;
    std::uint64_t audioBytes = 0;
    if (emission.audio) {
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
        videoUnits.value(), audioUnitCount, "aggregate packet items");
    auto ownedPayload = MediaCheckedArithmetic::add(
        videoPacketBytes.value(), audioBytes, "shared encoded payload bytes");
    auto withSurfaces = ownedPayload
        ? MediaCheckedArithmetic::add(
              ownedPayload.value(), surfaceBytes.value(),
              "media payload and surface bytes")
        : ownedPayload;
    auto videoCount = size(videoUnits.value(), "video queue items");
    auto packetCount = packetItems
        ? size(packetItems.value(), "packet queue items")
        : ::media::Result<std::size_t>::failure(packetItems.error());
    auto audioCount = emission.audio
        ? size(audioUnitCount, "audio queue items")
        : ::media::Result<std::size_t>::success(0);
    if (!withSurfaces || !videoCount || !packetCount || !audioCount) {
        return Result::failure(
            !withSurfaces ? withSurfaces.error() : !videoCount ? videoCount.error() :
            !packetCount ? packetCount.error() : audioCount.error());
    }

    try {
        std::vector<MediaRealtimeGraphResourceLedgerEntry> entries;
        entries.reserve(emission.audio ? 5U : 4U);
        entries.push_back({
            MediaRealtimeResourceAccountingGroup::EncodedVideoPacket,
            MediaRealtimeQueueRetentionSemantics::BoundedFifo,
            videoUnits.value(), videoPacketBytes.value(),
            emission.video.authority + "+shared-packet-ownership"});
        entries.push_back({
            MediaRealtimeResourceAccountingGroup::DecodedVideoSurface,
            MediaRealtimeQueueRetentionSemantics::BoundedFifo,
            videoUnits.value(), surfaceBytes.value(),
            hardwareSurface
                ? "prepared-hardware-surface-logical-bytes"
                : "prepared-software-frame-logical-bytes"});
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
                packetCount.value(), videoCount.value(), packetCount.value()},
            std::move(media), resourceScope,
            deployment.encode().resources
                .maximumGraphPayloadAndReservedStorageBytes,
            emission.hardwareMemory, emission.video.maximumEncoderRetainedFrames,
            hardwareSurface,
            std::move(entries)});
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "realtime graph resource ledger"));
    }
}

::media::Status MediaRealtimeGraphResourceLedgerPlanner::validate(
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    if (ledger.resourceScope ==
            MediaRealtimeGraphResourceBudgetScope::Unknown ||
        ledger.maximumGraphPayloadAndReservedStorageBytes == 0 ||
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
