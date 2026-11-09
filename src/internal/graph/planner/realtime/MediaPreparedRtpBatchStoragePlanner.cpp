#include "internal/graph/planner/realtime/MediaPreparedRtpBatchStoragePlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>
#include <mutex>
#include <utility>

namespace media::ffmpeg::graph {
class MediaPreparedRtpSourceSession;
namespace {
template<class T> ::media::Result<std::size_t> request(std::size_t count)
{
    using Result = ::media::Result<std::size_t>;
    auto bytes = MediaCheckedArithmetic::multiply(count, sizeof(T), "Prepared batch array storage");
    if (!bytes) return Result::failure(bytes.error());
    if (bytes.value() > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::allocationFailed("Prepared batch array exceeds address space"));
    return MediaPreparationMemoryResource::allocationFootprint(static_cast<std::size_t>(bytes.value()), alignof(T));
}
::media::Status add(std::size_t& total, std::size_t bytes)
{
    auto sum = MediaCheckedArithmetic::add(total, bytes, "Prepared batch storage total");
    if (!sum) return ::media::Status::failure(sum.error());
    if (sum.value() > (std::numeric_limits<std::size_t>::max)())
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Prepared batch storage exceeds address space"));
    total = static_cast<std::size_t>(sum.value());
    return ::media::Status::success();
}
} // namespace

::media::Result<MediaPreparedRtpBatchStoragePlan> MediaPreparedRtpBatchStoragePlanner::plan(
    std::size_t count, std::size_t childCount, std::shared_ptr<MediaPreparationMemoryResource> owner)
{
    using Result = ::media::Result<MediaPreparedRtpBatchStoragePlan>;
    if (!count || !childCount || !owner)
        return Result::failure(::media::ErrorInfo::invalidArgument("Prepared batch storage requires count and owner"));
    MediaPreparedRtpBatchStoragePlan result{count, childCount, std::move(owner), 0, 0, 0};
    // freezeBatch uses five distinct exact-capacity arrays. Each request has
    // its own resource header/alignment, even though they share one owner.
    for (auto bytes : {request<MediaPreparedRtpSourceSession*>(count),
             request<std::unique_lock<std::mutex>>(count),
             request<MediaPreparedRtpStorageSourceSnapshot>(count),
             request<std::shared_ptr<MediaPreparationStorageBudget>>(childCount),
             request<MediaPreparationStorageSnapshot>(childCount)}) {
        if (!bytes) return Result::failure(bytes.error());
        if (auto status = add(result.scratchBytes, bytes.value()); !status) return Result::failure(status.error());
    }
    auto returned = request<MediaPreparedRtpStorageAdmission>(count);
    if (!returned) return Result::failure(returned.error());
    result.resultBytes = returned.value();
    result.totalBytes = result.scratchBytes;
    if (auto status = add(result.totalBytes, result.resultBytes); !status) return Result::failure(status.error());
    return Result::success(std::move(result));
}
} // namespace media::ffmpeg::graph
