#include "internal/graph/runtime/resource/MediaObservedRootStorageGeometry.h"
#include "internal/graph/runtime/resource/MediaStorageReleaseNotification.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/resource/MediaPmrContainerGeometry.h"
#include <limits>
namespace media::ffmpeg::graph {
::media::Result<MediaObservedRootStorageGeometry> MediaObservedRootStorageGeometry::plan(std::size_t targets)
{
    using R = ::media::Result<MediaObservedRootStorageGeometry>;
    if (!targets) return R::failure(::media::ErrorInfo::invalidArgument("Observed root requires producer targets"));
    auto wakeup = probeMediaMakeSharedAllocationGeometry<MediaNodeWakeup>();
    auto control = probeMediaSeparateSharedControlGeometry<MediaStorageReleaseNotification>();
    auto weak = MediaStandardContainerGeometry::vector<std::weak_ptr<MediaNodeWakeup>>(targets, false);
    auto strong = MediaStandardContainerGeometry::vector<std::shared_ptr<MediaNodeWakeup>>(targets, false);
    if (!wakeup) return R::failure(wakeup.error());
    if (!control) return R::failure(control.error());
    if (!weak) return R::failure(weak.error());
    if (!strong) return R::failure(strong.error());
    auto permanent = MediaCheckedArithmetic::multiply(targets, wakeup.value().bytes, "Observed root wakeups");
    for (auto bytes : {sizeof(MediaStorageReleaseNotification), control.value().bytes, weak.value().peakBytes}) {
        if (!permanent) return R::failure(permanent.error());
        permanent = MediaCheckedArithmetic::add(permanent.value(), bytes, "Observed root owners");
    }
    auto temporary = MediaCheckedArithmetic::add(sizeof(std::vector<std::shared_ptr<MediaNodeWakeup>>),
        strong.value().peakBytes, "Observed root exported directory");
    if (!permanent) return R::failure(permanent.error());
    if (!temporary) return R::failure(temporary.error());
    auto total = MediaCheckedArithmetic::add(permanent.value(), temporary.value(), "Observed root allocation");
    if (!total) return R::failure(total.error());
    if (total.value() > (std::numeric_limits<std::size_t>::max)())
        return R::failure(::media::ErrorInfo::allocationFailed("Observed root storage exceeds address space"));
    return R::success({static_cast<std::size_t>(permanent.value()), static_cast<std::size_t>(temporary.value()),
        static_cast<std::size_t>(total.value())});
}
}
