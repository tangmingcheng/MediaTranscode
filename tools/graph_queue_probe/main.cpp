#include "internal/graph/model/MediaQueuePolicy.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/queue/MediaBlockingQueue.h"
#include "internal/graph/runtime/queue/MediaSpscRingQueue.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

using namespace media::ffmpeg::graph;

class ProbeBuffer final : public MediaBuffer {
public:
    explicit ProbeBuffer(std::string name, MediaTimeValue pts, bool keyFrame)
    {
        setPayloadKind(MediaPayloadKind::Frame);
        setStreamKind(MediaStreamKind::Video);
        setTimestamps(pts, pts, 1);
        setDiagnosticName(std::move(name));
        if (keyFrame) {
            addFlags(MediaBufferFlag::KeyFrame);
        }
    }

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::Frame;
    }
};

MediaBufferRef makeBuffer(const std::string& name, MediaTimeValue pts, bool keyFrame = false)
{
    return std::make_shared<ProbeBuffer>(name, pts, keyFrame);
}

bool require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "graph queue probe failed: " << message << '\n';
        return false;
    }
    return true;
}

bool testBlockingQueue()
{
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::Blocking;
    policy.bounded = true;
    policy.capacity = 4;
    policy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;

    MediaBlockingQueue queue(policy);
    if (!queue.push(makeBuffer("blocking-1", 10))) {
        return require(false, "blocking push 1 failed");
    }
    if (!queue.push(makeBuffer("blocking-2", 20))) {
        return require(false, "blocking push 2 failed");
    }
    if (!queue.push(makeBuffer("blocking-3", 30))) {
        return require(false, "blocking push 3 failed");
    }

    MediaBufferRef out;
    if (!queue.pop(out) || !require(out && out->pts() == 10, "blocking pop 1 order mismatch")) {
        return false;
    }
    if (!queue.pop(out) || !require(out && out->pts() == 20, "blocking pop 2 order mismatch")) {
        return false;
    }
    if (!queue.pop(out) || !require(out && out->pts() == 30, "blocking pop 3 order mismatch")) {
        return false;
    }

    const auto& metrics = queue.metrics();
    return require(metrics.pushed == 3, "blocking pushed metric mismatch") &&
           require(metrics.popped == 3, "blocking popped metric mismatch") &&
           require(metrics.dropped == 0, "blocking dropped metric mismatch") &&
           require(queue.empty(), "blocking queue not empty");
}

bool testSpscRingQueue()
{
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::SpscRing;
    policy.bounded = true;
    policy.capacity = 4;
    policy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;

    MediaSpscRingQueue queue(policy);
    if (!require(queue.tryPush(makeBuffer("spsc-1", 100)), "spsc tryPush 1 failed")) {
        return false;
    }
    if (!require(queue.tryPush(makeBuffer("spsc-2", 200)), "spsc tryPush 2 failed")) {
        return false;
    }
    if (!require(queue.tryPush(makeBuffer("spsc-3", 300)), "spsc tryPush 3 failed")) {
        return false;
    }

    MediaBufferRef out;
    if (!require(queue.tryPop(out) && out && out->pts() == 100, "spsc pop 1 order mismatch")) {
        return false;
    }
    if (!require(queue.tryPop(out) && out && out->pts() == 200, "spsc pop 2 order mismatch")) {
        return false;
    }
    if (!require(queue.tryPop(out) && out && out->pts() == 300, "spsc pop 3 order mismatch")) {
        return false;
    }

    const auto& metrics = queue.metrics();
    return require(metrics.pushed == 3, "spsc pushed metric mismatch") &&
           require(metrics.popped == 3, "spsc popped metric mismatch") &&
           require(metrics.dropped == 0, "spsc dropped metric mismatch") &&
           require(queue.empty(), "spsc queue not empty");
}

bool testDropNonKeyOverflow()
{
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::Blocking;
    policy.bounded = true;
    policy.capacity = 2;
    policy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;

    MediaBlockingQueue queue(policy);
    if (!require(queue.tryPush(makeBuffer("key-1", 1000, true)), "overflow push key-1 failed")) {
        return false;
    }
    if (!require(queue.tryPush(makeBuffer("delta-1", 1010, false)), "overflow push delta-1 failed")) {
        return false;
    }
    if (!require(queue.tryPush(makeBuffer("key-2", 1020, true)), "overflow push key-2 failed")) {
        return false;
    }

    const auto& metrics = queue.metrics();
    if (!require(metrics.pushed == 3, "overflow pushed metric mismatch")) {
        return false;
    }
    if (!require(metrics.dropped == 1, "overflow dropped metric mismatch")) {
        return false;
    }
    if (!require(queue.size() == 2, "overflow queue size mismatch")) {
        return false;
    }

    MediaBufferRef first;
    MediaBufferRef second;
    if (!require(queue.tryPop(first), "overflow pop first failed")) {
        return false;
    }
    if (!require(queue.tryPop(second), "overflow pop second failed")) {
        return false;
    }

    return require(first && first->isKeyFrame(), "overflow first buffer is not key frame") &&
           require(second && second->isKeyFrame(), "overflow second buffer is not key frame") &&
           require(queue.empty(), "overflow queue not empty");
}

} // namespace

int main()
{
    if (!testBlockingQueue()) {
        return 1;
    }
    if (!testSpscRingQueue()) {
        return 1;
    }
    if (!testDropNonKeyOverflow()) {
        return 1;
    }

    std::cout << "graph queue probe ok: "
              << "blocking pushed=3 popped=3 dropped=0; "
              << "spsc pushed=3 popped=3 dropped=0; "
              << "overflow dropped=1" << '\n';
    return 0;
}
