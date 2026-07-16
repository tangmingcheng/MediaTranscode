#include "common/TestAssert.h"

#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpSourceClockMapper.h"

#include <cstdint>
#include <limits>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

constexpr std::int64_t Second = 1'000'000'000;

MediaRtcpClockEvidence evidence(std::uint32_t ssrc,
                                std::uint32_t ntpSeconds,
                                std::uint32_t ntpFraction,
                                std::uint32_t rtpTimestamp,
                                const char* cname,
                                std::int64_t observedAtNs,
                                std::uint64_t generation)
{
    std::vector<std::uint8_t> cnameBytes;
    for (const char* cursor = cname; *cursor != '\0'; ++cursor) {
        cnameBytes.push_back(static_cast<std::uint8_t>(*cursor));
    }
    return MediaRtcpClockEvidence{ssrc,
                                  ssrc,
                                  ssrc,
                                  {ntpSeconds, ntpFraction},
                                  rtpTimestamp,
                                  std::move(cnameBytes),
                                  observedAtNs,
                                  observedAtNs,
                                  generation};
}

MediaRtpSourceClockMapper mapper(int clockRate = 90'000,
                                 std::uint64_t generation = 4)
{
    auto created = MediaRtpSourceClockMapper::create(
        MediaRtpSourceClockMapperConfig{
            clockRate, 3 * Second, 5 * Second, 250'000'000, 1'000},
        generation);
    return std::move(created).value();
}

void testMapsMatchingSenderClockAndWrap(TestContext& ctx)
{
    auto video = mapper();
    EXPECT_TRUE(ctx, video.observeSenderReport(
        evidence(11, 100, 0, 0xFFFFFF00u, "camera", 10, 4)));
    auto beforeWrap = video.map(0xFFFFFF90u, 20);
    auto afterWrap = video.map(0x00000080u, 30);
    EXPECT_TRUE(ctx, beforeWrap && afterWrap);
    if (beforeWrap && afterWrap) {
        EXPECT_TRUE(ctx, afterWrap.value().sourceTime > beforeWrap.value().sourceTime);
        EXPECT_EQ(ctx, afterWrap.value().generation, static_cast<std::uint64_t>(4));
        EXPECT_EQ(ctx, afterWrap.value().confidence, MediaRtpSourceClockConfidence::Locked);
    }
}

void testRefinesRateWithoutPublishedJump(TestContext& ctx)
{
    auto video = mapper(90'000, 1);
    EXPECT_TRUE(ctx, video.observeSenderReport(evidence(11, 10, 0, 0, "camera", 0, 1)));
    auto prior = video.map(90'000, Second);
    EXPECT_TRUE(ctx, prior);
    EXPECT_TRUE(ctx, video.observeSenderReport(
        evidence(11, 11, 4'294'967, 90'000, "camera", Second, 1)));
    auto sameTimestamp = video.map(90'000, Second + 1);
    auto later = video.map(180'000, 2 * Second);
    EXPECT_TRUE(ctx, sameTimestamp && later);
    if (prior && sameTimestamp) {
        EXPECT_EQ(ctx, sameTimestamp.value().sourceTime, prior.value().sourceTime);
    }
    if (later) {
        EXPECT_NEAR(ctx, later.value().sourceTime.nanoseconds(), 12'002'000'000LL, 2'000'000LL);
    }
}

void testRejectsInvalidSenderReportMovement(TestContext& ctx)
{
    auto ntpRegression = mapper();
    EXPECT_TRUE(ctx, ntpRegression.observeSenderReport(evidence(1, 20, 0, 100, "c", 0, 4)));
    EXPECT_FALSE(ctx, ntpRegression.observeSenderReport(evidence(1, 19, 0, 200, "c", 1, 4)));

    auto invalidSlope = mapper();
    EXPECT_TRUE(ctx, invalidSlope.observeSenderReport(evidence(1, 20, 0, 100, "c", 0, 4)));
    EXPECT_FALSE(ctx, invalidSlope.observeSenderReport(evidence(1, 21, 0, 100, "c", 1, 4)));

    auto excessiveResidual = mapper();
    EXPECT_TRUE(ctx, excessiveResidual.observeSenderReport(evidence(1, 20, 0, 0, "c", 0, 4)));
    EXPECT_FALSE(ctx, excessiveResidual.observeSenderReport(
        evidence(1, 22, 0, 90'000, "c", 1, 4)));

    auto excessiveRate = mapper();
    EXPECT_TRUE(ctx, excessiveRate.observeSenderReport(evidence(1, 20, 0, 0, "c", 0, 4)));
    EXPECT_FALSE(ctx, excessiveRate.observeSenderReport(
        evidence(1, 20, 429'496'729, 1, "c", 1, 4)));

    auto negativeSrAge = evidence(1, 20, 0, 0, "c", -1, 4);
    auto negativeSrMapper = mapper();
    EXPECT_FALSE(ctx, negativeSrMapper.observeSenderReport(negativeSrAge));
    auto negativeCnameAge = evidence(1, 20, 0, 0, "c", 1, 4);
    negativeCnameAge.cnameObservedAtNs = -1;
    auto negativeCnameMapper = mapper();
    EXPECT_FALSE(ctx, negativeCnameMapper.observeSenderReport(negativeCnameAge));
}

void testDuplicateSenderReportRefreshesAgeWithoutChangingCalibration(TestContext& ctx)
{
    auto video = mapper();
    const auto first = evidence(1, 20, 0, 100, "c", 0, 4);
    EXPECT_TRUE(ctx, video.observeSenderReport(first));
    auto before = video.calibration(0);
    auto duplicate = first;
    duplicate.senderReportObservedAtNs = Second;
    duplicate.cnameObservedAtNs = Second;
    EXPECT_TRUE(ctx, video.observeSenderReport(duplicate));
    auto after = video.calibration(Second);
    EXPECT_TRUE(ctx, before && after);
    if (before && after) {
        EXPECT_EQ(ctx, before.value().continuousSourceAnchor,
                  after.value().continuousSourceAnchor);
        EXPECT_EQ(ctx, after.value().senderReportObservedAtNs, Second);
    }
}

void testTimeoutDegradesThenExpiresAndReacquires(TestContext& ctx)
{
    auto video = mapper();
    EXPECT_TRUE(ctx, video.observeSenderReport(evidence(1, 20, 0, 0, "c", 100, 4)));
    auto locked = video.map(90'000, 100 + 3 * Second);
    auto degraded = video.map(180'000, 100 + 3 * Second + 1);
    auto expired = video.map(270'000, 100 + 5 * Second + 1);
    EXPECT_TRUE(ctx, locked && degraded);
    if (degraded) {
        EXPECT_EQ(ctx, degraded.value().confidence, MediaRtpSourceClockConfidence::Degraded);
    }
    EXPECT_FALSE(ctx, expired);

    video.reset(5);
    EXPECT_FALSE(ctx, video.map(0, 0));
    EXPECT_TRUE(ctx, video.observeSenderReport(evidence(2, 30, 0, 0, "new", 0, 5)));
    auto reacquired = video.map(90'000, 1);
    EXPECT_TRUE(ctx, reacquired);
    if (reacquired) EXPECT_EQ(ctx, reacquired.value().generation, static_cast<std::uint64_t>(5));
}

void testIdentityAndGenerationChangesRequireReacquisition(TestContext& ctx)
{
    auto video = mapper();
    EXPECT_TRUE(ctx, video.observeSenderReport(evidence(1, 20, 0, 0, "old", 0, 4)));
    EXPECT_FALSE(ctx, video.observeSenderReport(evidence(2, 21, 0, 90'000, "old", 1, 4)));
    EXPECT_FALSE(ctx, video.map(90'000, 1));

    video.reset(5);
    EXPECT_TRUE(ctx, video.observeSenderReport(evidence(2, 30, 0, 0, "new", 2, 5)));
    EXPECT_FALSE(ctx, video.observeSenderReport(evidence(2, 31, 0, 90'000, "changed", 3, 5)));
    EXPECT_FALSE(ctx, video.observeSenderReport(evidence(2, 31, 0, 90'000, "new", 3, 4)));
}

MediaRtpSourceClockCalibration calibration(const MediaRtcpClockEvidence& value,
                                           int clockRate)
{
    auto streamMapper = mapper(clockRate, value.generation);
    streamMapper.observeSenderReport(value);
    return streamMapper.calibration(value.senderReportObservedAtNs).value();
}

void testGroupRequiresExactCommonIdentityAndCurrentEvidence(TestContext& ctx)
{
    auto validatorResult = MediaRtpClockGroupValidator::create(
        MediaRtpClockGroupValidatorConfig{3 * Second, 5 * Second, 50'000'000,
                                          5 * Second, 5 * Second});
    EXPECT_TRUE(ctx, validatorResult);
    if (!validatorResult) return;
    auto validator = std::move(validatorResult).value();

    const auto videoEvidence = evidence(11, 100, 0, 9'000, "camera", 100, 2);
    const auto audioEvidence = evidence(22, 100, 0, 4'800, "camera", 110, 7);
    EXPECT_TRUE(ctx, validator.observe(MediaStreamKind::Video,
                                       videoEvidence,
                                       calibration(videoEvidence, 90'000)));
    auto missing = validator.snapshot(120);
    EXPECT_EQ(ctx, missing.state, MediaRtpClockGroupState::Acquiring);
    EXPECT_TRUE(ctx, validator.observe(MediaStreamKind::Audio,
                                       audioEvidence,
                                       calibration(audioEvidence, 48'000)));
    auto ready = validator.snapshot(120);
    EXPECT_EQ(ctx, ready.state, MediaRtpClockGroupState::Locked);
    EXPECT_TRUE(ctx, ready.locked.has_value());
    if (ready.locked) {
        EXPECT_EQ(ctx, ready.locked->cname,
                  std::vector<std::uint8_t>({'c','a','m','e','r','a'}));
        EXPECT_EQ(ctx, ready.locked->commonSourceEpoch,
                  ready.locked->video.actualSenderReportSourceTime);
    }
    EXPECT_TRUE(ctx, ready.groupGeneration > 0);

    const auto oldGroupGeneration = ready.groupGeneration;
    const auto changedGeneration = evidence(11, 101, 0, 99'000, "camera", 200, 3);
    EXPECT_TRUE(ctx, validator.observe(MediaStreamKind::Video,
                                       changedGeneration,
                                       calibration(changedGeneration, 90'000)));
    auto isolated = validator.snapshot(200);
    EXPECT_EQ(ctx, isolated.state, MediaRtpClockGroupState::Acquiring);
    EXPECT_TRUE(ctx, isolated.groupGeneration > oldGroupGeneration);
    EXPECT_FALSE(ctx, isolated.locked.has_value());
}

void testGroupRejectsMismatchSkewAndHasNoFallback(TestContext& ctx)
{
    auto makeValidator = [] {
        return MediaRtpClockGroupValidator::create(
            MediaRtpClockGroupValidatorConfig{3 * Second, 5 * Second, 50'000'000,
                                              5 * Second, 5 * Second}).value();
    };
    const auto video = evidence(11, 100, 0, 0, "camera-a", 0, 1);

    auto forgedIdentity = makeValidator();
    auto mismatchedSsrc = video;
    mismatchedSsrc.senderReportSsrc = 99;
    EXPECT_FALSE(ctx, forgedIdentity.observe(MediaStreamKind::Video,
                                             mismatchedSsrc,
                                             calibration(video, 90'000)));
    EXPECT_EQ(ctx, forgedIdentity.snapshot(0).state,
              MediaRtpClockGroupState::ReacquireRequired);

    auto mismatch = makeValidator();
    EXPECT_TRUE(ctx, mismatch.observe(MediaStreamKind::Video, video, calibration(video, 90'000)));
    const auto otherCname = evidence(22, 100, 0, 0, "camera-b", 0, 1);
    EXPECT_FALSE(ctx, mismatch.observe(MediaStreamKind::Audio,
                                       otherCname,
                                       calibration(otherCname, 48'000)));
    EXPECT_EQ(ctx, mismatch.snapshot(0).state, MediaRtpClockGroupState::ReacquireRequired);

    auto skewed = makeValidator();
    EXPECT_TRUE(ctx, skewed.observe(MediaStreamKind::Video, video, calibration(video, 90'000)));
    const auto lateAudio = evidence(22, 101, 0, 0, "camera-a", 0, 1);
    EXPECT_FALSE(ctx, skewed.observe(MediaStreamKind::Audio,
                                    lateAudio,
                                    calibration(lateAudio, 48'000)));

    auto missing = makeValidator();
    EXPECT_EQ(ctx, missing.snapshot(999 * Second).state, MediaRtpClockGroupState::Acquiring);
    EXPECT_FALSE(ctx, missing.snapshot(999 * Second).locked.has_value());
}

void testGroupDegradesExpiresAndInvalidatesOnIngressDiscontinuity(TestContext& ctx)
{
    auto validator = MediaRtpClockGroupValidator::create(
        MediaRtpClockGroupValidatorConfig{3 * Second, 5 * Second, 50'000'000,
                                          5 * Second, 5 * Second}).value();
    const auto video = evidence(11, 100, 0, 0, "camera", 0, 1);
    const auto audio = evidence(22, 100, 0, 0, "camera", 0, 1);
    EXPECT_TRUE(ctx, validator.observe(MediaStreamKind::Video, video, calibration(video, 90'000)));
    EXPECT_TRUE(ctx, validator.observe(MediaStreamKind::Audio, audio, calibration(audio, 48'000)));
    EXPECT_EQ(ctx, validator.snapshot(3 * Second + 1).state, MediaRtpClockGroupState::Degraded);
    EXPECT_EQ(ctx, validator.snapshot(5 * Second + 1).state, MediaRtpClockGroupState::ReacquireRequired);
    const auto generation = validator.snapshot(5 * Second + 1).groupGeneration;
    validator.invalidate();
    EXPECT_TRUE(ctx, validator.snapshot(0).groupGeneration > generation);
}

void testClockObservationDeadlinesCapLongTransportWait(TestContext& ctx)
{
    auto scheduleResult = MediaRtpClockObservationSchedule::create(
        3 * Second, 5 * Second, 5 * Second);
    EXPECT_TRUE(ctx, scheduleResult);
    if (!scheduleResult) return;
    auto schedule = std::move(scheduleResult).value();
    EXPECT_TRUE(ctx, schedule.observeEvidence(100, 100));
    EXPECT_EQ(ctx, schedule.nextDeadlineNs(), std::optional<std::int64_t>(100 + 3 * Second));
    auto initialTimeout = schedule.receiveTimeoutMs(100, 8'000);
    EXPECT_TRUE(ctx, initialTimeout);
    if (initialTimeout) EXPECT_EQ(ctx, initialTimeout.value(), 3'000);
    auto beforeDeadline = schedule.transition(100 + 3 * Second - 1);
    EXPECT_TRUE(ctx, beforeDeadline);
    if (beforeDeadline) EXPECT_FALSE(ctx, beforeDeadline.value().has_value());
    auto atDegradedBoundary = schedule.transition(100 + 3 * Second);
    EXPECT_TRUE(ctx, atDegradedBoundary);
    if (atDegradedBoundary) EXPECT_FALSE(ctx, atDegradedBoundary.value().has_value());
    auto degraded = schedule.transition(100 + 3 * Second + 1);
    EXPECT_TRUE(ctx, degraded);
    if (degraded && degraded.value()) {
        EXPECT_EQ(ctx, *degraded.value(), MediaRtpClockAgeTransition::Degraded);
    }
    EXPECT_EQ(ctx, schedule.nextDeadlineNs(), std::optional<std::int64_t>(100 + 5 * Second));
    auto degradedTimeout = schedule.receiveTimeoutMs(100 + 3 * Second + 1, 8'000);
    EXPECT_TRUE(ctx, degradedTimeout);
    if (degradedTimeout) EXPECT_EQ(ctx, degradedTimeout.value(), 2'000);
    auto atExpiryBoundary = schedule.transition(100 + 5 * Second);
    EXPECT_TRUE(ctx, atExpiryBoundary);
    if (atExpiryBoundary) EXPECT_FALSE(ctx, atExpiryBoundary.value().has_value());
    auto expired = schedule.transition(100 + 5 * Second + 1);
    EXPECT_TRUE(ctx, expired);
    if (expired && expired.value()) {
        EXPECT_EQ(ctx, *expired.value(), MediaRtpClockAgeTransition::Expired);
    }
    EXPECT_FALSE(ctx, schedule.nextDeadlineNs().has_value());
}

void testCnameFreshnessExpiresGroupAndCapsReceiveDeadline(TestContext& ctx)
{
    auto validator = MediaRtpClockGroupValidator::create(
        MediaRtpClockGroupValidatorConfig{3 * Second, 5 * Second, 50'000'000,
                                          4 * Second, 4 * Second}).value();
    auto video = evidence(11, 100, 0, 0, "camera", 4 * Second, 1);
    auto audio = evidence(22, 100, 0, 0, "camera", 4 * Second, 1);
    video.cnameObservedAtNs = 0;
    audio.cnameObservedAtNs = 0;
    EXPECT_TRUE(ctx, validator.observe(MediaStreamKind::Video, video, calibration(video, 90'000)));
    EXPECT_TRUE(ctx, validator.observe(MediaStreamKind::Audio, audio, calibration(audio, 48'000)));
    EXPECT_EQ(ctx, validator.snapshot(4 * Second).state, MediaRtpClockGroupState::Locked);
    EXPECT_EQ(ctx, validator.snapshot(4 * Second + 1).state,
              MediaRtpClockGroupState::ReacquireRequired);

    auto schedule = MediaRtpClockObservationSchedule::create(
        3 * Second, 5 * Second, 4 * Second).value();
    EXPECT_TRUE(ctx, schedule.observeEvidence(3 * Second, 0));
    EXPECT_EQ(ctx, schedule.nextDeadlineNs(), std::optional<std::int64_t>(4 * Second));
    auto receiveTimeout = schedule.receiveTimeoutMs(3 * Second, 8'000);
    EXPECT_TRUE(ctx, receiveTimeout);
    if (receiveTimeout) EXPECT_EQ(ctx, receiveTimeout.value(), 1'000);
    auto atCnameBoundary = schedule.transition(4 * Second);
    EXPECT_TRUE(ctx, atCnameBoundary);
    if (atCnameBoundary) EXPECT_FALSE(ctx, atCnameBoundary.value().has_value());
    auto expired = schedule.transition(4 * Second + 1);
    EXPECT_TRUE(ctx, expired);
    if (expired && expired.value()) {
        EXPECT_EQ(ctx, *expired.value(), MediaRtpClockAgeTransition::Expired);
    }
}

void testClockObservationDeadlineArithmeticSaturates(TestContext& ctx)
{
    auto schedule = MediaRtpClockObservationSchedule::create(10, 20, 30).value();
    const auto nearMaximum = std::numeric_limits<std::int64_t>::max() - 5;
    EXPECT_TRUE(ctx, schedule.observeEvidence(nearMaximum, nearMaximum));
    EXPECT_EQ(ctx, schedule.nextDeadlineNs(),
              std::optional<std::int64_t>(std::numeric_limits<std::int64_t>::max()));
    auto saturatedTimeout = schedule.receiveTimeoutMs(nearMaximum, 8'000);
    EXPECT_TRUE(ctx, saturatedTimeout);
    if (saturatedTimeout) EXPECT_EQ(ctx, saturatedTimeout.value(), 1);
    auto beforeSaturation = schedule.transition(std::numeric_limits<std::int64_t>::max() - 1);
    EXPECT_TRUE(ctx, beforeSaturation);
    if (beforeSaturation) EXPECT_FALSE(ctx, beforeSaturation.value().has_value());
    auto atSaturation = schedule.transition(std::numeric_limits<std::int64_t>::max());
    EXPECT_TRUE(ctx, atSaturation);
    if (atSaturation) EXPECT_FALSE(ctx, atSaturation.value().has_value());

    auto invalid = MediaRtpClockObservationSchedule::create(10, 20, 30).value();
    EXPECT_FALSE(ctx, invalid.observeEvidence(-1, 0));
    EXPECT_FALSE(ctx, invalid.observeEvidence(0, -1));
    EXPECT_FALSE(ctx, invalid.receiveTimeoutMs(-1, 8'000));
    EXPECT_FALSE(ctx, invalid.receiveTimeoutMs(0, 0));
    EXPECT_FALSE(ctx, invalid.transition(-1));
    EXPECT_FALSE(ctx, invalid.nextDeadlineNs().has_value());
}

} // namespace

void runRtpSourceClockTests(TestContext& ctx)
{
    testMapsMatchingSenderClockAndWrap(ctx);
    testRefinesRateWithoutPublishedJump(ctx);
    testRejectsInvalidSenderReportMovement(ctx);
    testDuplicateSenderReportRefreshesAgeWithoutChangingCalibration(ctx);
    testTimeoutDegradesThenExpiresAndReacquires(ctx);
    testIdentityAndGenerationChangesRequireReacquisition(ctx);
    testGroupRequiresExactCommonIdentityAndCurrentEvidence(ctx);
    testGroupRejectsMismatchSkewAndHasNoFallback(ctx);
    testGroupDegradesExpiresAndInvalidatesOnIngressDiscontinuity(ctx);
    testClockObservationDeadlinesCapLongTransportWait(ctx);
    testCnameFreshnessExpiresGroupAndCapsReceiveDeadline(ctx);
    testClockObservationDeadlineArithmeticSaturates(ctx);
}
