#include "internal/graph/sync/lineage/MediaCanonicalAudioSourceTimeline.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

using namespace media::ffmpeg::graph;

namespace {

void preservesExactTimelineAcrossQuantizedCanonicalTimestamps()
{
    auto created = MediaCanonicalAudioSourceTimeline::create(44'100);
    assert(created);
    auto timeline = std::move(created).value();
    const auto anchor = MediaRunningTime::fromNanoseconds(123'456'789);
    std::int64_t expectedBegin = 5'444;
    for (std::uint64_t sequence = 1; sequence <= 600; ++sequence) {
        auto interval = timeline.append(
            sequence == 1
                ? anchor
                : MediaRunningTime::fromNanoseconds(
                      anchor.nanoseconds() +
                      static_cast<std::int64_t>(sequence - 1) * 23'219'955 +
                      static_cast<std::int64_t>(sequence % 3)),
            1'024, 7, sequence);
        assert(interval);
        assert(interval.value().begin == expectedBegin);
        assert(interval.value().end == expectedBegin + 1'024);
        assert(interval.value().sampleRate == 44'100);
        expectedBegin += 1'024;
    }
}

void rejectsInvalidTimelineContract()
{
    assert(!MediaCanonicalAudioSourceTimeline::create(0));

    auto created = MediaCanonicalAudioSourceTimeline::create(44'100);
    assert(created);
    auto timeline = std::move(created).value();
    const auto anchor = MediaRunningTime::fromNanoseconds(123'456'789);
    assert(!timeline.append(anchor, 0, 7, 1));
    assert(!timeline.append(anchor, 1'024, 7, 0));
    assert(!timeline.append(anchor, 1'024, 0, 1));
    assert(timeline.append(anchor, 1'024, 7, 1));
    assert(!timeline.append(anchor, 1'024, 7, 3));
    assert(!timeline.append(anchor, 1'024, 7, 1));
    assert(!timeline.append(anchor, 1'024, 8, 2));
    assert(timeline.append(anchor, 1'024, 7, 2));

    auto overflowCreated = MediaCanonicalAudioSourceTimeline::create(1'000'000'000);
    assert(overflowCreated);
    auto overflowTimeline = std::move(overflowCreated).value();
    assert(!overflowTimeline.append(
        MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max()),
        1, 9, 1));
}

void resetPermitsFreshGenerationAndAnchor()
{
    auto created = MediaCanonicalAudioSourceTimeline::create(44'100);
    assert(created);
    auto timeline = std::move(created).value();
    assert(timeline.append(
        MediaRunningTime::fromNanoseconds(123'456'789), 1'024, 7, 1));

    timeline.reset();

    auto interval = timeline.append(
        MediaRunningTime::fromNanoseconds(50'000'000), 1'024, 8, 1);
    assert(interval);
    assert(interval.value().begin == 2'205);
    assert(interval.value().end == 3'229);
    assert(interval.value().sampleRate == 44'100);
}

} // namespace

int main()
{
    preservesExactTimelineAcrossQuantizedCanonicalTimestamps();
    rejectsInvalidTimelineContract();
    resetPermitsFreshGenerationAndAnchor();
    std::cout << "canonical audio source timeline tests passed\n";
    return 0;
}
