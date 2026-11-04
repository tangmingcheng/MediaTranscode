#pragma once

#include "internal/graph/runtime/resource/MediaStorageBudget.h"

namespace media::ffmpeg::graph {

struct MediaPreparationStorageDomain final {};
using MediaPreparationStorageBudget = MediaStorageBudget<MediaPreparationStorageDomain>;
using MediaPreparationStorageLease = MediaStorageLease<MediaPreparationStorageDomain>;
using MediaStandalonePreparationStorage = MediaStandaloneStorage<MediaPreparationStorageDomain>;
using MediaSharedPreparationStorage = MediaSharedStorage<MediaPreparationStorageDomain>;
using MediaPreparationStorageScope = MediaStorageScope<MediaPreparationStorageDomain>;
using MediaPreparationStorageSnapshot = MediaStorageSnapshot<MediaPreparationStorageDomain>;

} // namespace media::ffmpeg::graph
