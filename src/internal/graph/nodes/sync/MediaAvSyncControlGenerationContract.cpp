#include "internal/graph/nodes/sync/MediaAvSyncControlGenerationContract.h"

#include "internal/graph/nodes/sync/MediaLockedPacketGateClassification.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaAvSyncControlClassification> invalid(
    const char* message)
{
    return ::media::Result<MediaAvSyncControlClassification>::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

} // namespace

std::string_view mediaControlGenerationPolicyOption(
    MediaControlGenerationPolicy policy) noexcept
{
    switch (policy) {
    case MediaControlGenerationPolicy::OptionalExactWhenPresent:
        return "optional_exact_when_present";
    case MediaControlGenerationPolicy::RequiredExact:
        return "required_exact";
    }
    return {};
}

::media::Result<MediaControlGenerationPolicy>
decodeMediaControlGenerationPolicy(std::string_view option)
{
    using Result = ::media::Result<MediaControlGenerationPolicy>;
    if (option == "optional_exact_when_present") {
        return Result::success(
            MediaControlGenerationPolicy::OptionalExactWhenPresent);
    }
    if (option == "required_exact") {
        return Result::success(
            MediaControlGenerationPolicy::RequiredExact);
    }
    return Result::failure(::media::ErrorInfo::invalidArgument(
        "A/V sync control generation policy is invalid"));
}

::media::Result<MediaAvSyncControlClassification>
classifyMediaAvSyncControl(
    const MediaBufferRef& buffer,
    MediaControlGenerationPolicy policy,
    MediaAvSyncGroupRuntime& syncGroup,
    std::uint64_t plannedInitialGeneration)
{
    auto control = classifyMediaControlBuffer(buffer);
    if (!control) {
        return ::media::Result<
            MediaAvSyncControlClassification>::failure(
                control.error());
    }
    const auto generation = control.value().control->generation();
    if (!generation) {
        if (policy == MediaControlGenerationPolicy::RequiredExact) {
            return invalid(
                "Strict A/V sync control requires explicit generation");
        }
        return ::media::Result<
            MediaAvSyncControlClassification>::success(
                MediaAvSyncControlClassification{
                    control.value(),
                    MediaControlGenerationDisposition::Forward,
                    std::nullopt});
    }
    if (*generation == 0) {
        return invalid(
            "A/V sync control generation must be nonzero");
    }
    auto arbitration = syncGroup.reserveGenerationArbitration();
    if (!arbitration) {
        return ::media::Result<
            MediaAvSyncControlClassification>::failure(
                arbitration.error());
    }
    auto reservation = std::move(arbitration).value();
    auto classified = classifyLockedPacketGateGeneration(
        reservation.reacquisition(),
        reservation.epoch(),
        *generation,
        plannedInitialGeneration);
    if (!classified) {
        return ::media::Result<
            MediaAvSyncControlClassification>::failure(
                classified.error());
    }
    MediaControlGenerationDisposition disposition;
    switch (classified.value()) {
    case MediaLockedPacketGateDisposition::Pass:
    case MediaLockedPacketGateDisposition::PassToInitialAcquisition:
    case MediaLockedPacketGateDisposition::PassToReacquisition:
        disposition = MediaControlGenerationDisposition::Forward;
        break;
    case MediaLockedPacketGateDisposition::WithholdForReacquisition:
        disposition = MediaControlGenerationDisposition::Withhold;
        break;
    case MediaLockedPacketGateDisposition::DropOldGeneration:
        disposition = MediaControlGenerationDisposition::DropOld;
        break;
    }
    return ::media::Result<
        MediaAvSyncControlClassification>::success(
            MediaAvSyncControlClassification{
                control.value(), disposition,
                std::optional<MediaAvGenerationPublicationReservation>(
                    std::move(reservation).
                        retainPublicationAuthority())});
}

::media::Result<MediaOutputCommitReservation>
reserveMediaAvSyncControlPublication(
    MediaAvSyncControlClassification classification,
    std::optional<std::uint64_t> exactConsumerGeneration,
    MediaControlConsumerGenerationRequirement consumerRequirement)
{
    using Result = ::media::Result<MediaOutputCommitReservation>;
    if (classification.generation !=
        MediaControlGenerationDisposition::Forward) {
        return Result::failure(::media::ErrorInfo::cancelled(
            "A/V sync control publication is no longer current"));
    }
    const auto generation =
        classification.control.control->generation();
    if (generation &&
        consumerRequirement ==
            MediaControlConsumerGenerationRequirement::ExactWhenPresent &&
        exactConsumerGeneration != generation) {
        return Result::failure(::media::ErrorInfo::cancelled(
            "A/V sync control differs from consumer generation"));
    }
    if (!classification.publicationAuthority) {
        return Result::success(MediaOutputCommitReservation{});
    }
    return Result::success(MediaOutputCommitReservation::hold(
        std::move(*classification.publicationAuthority)));
}

} // namespace media::ffmpeg::graph
