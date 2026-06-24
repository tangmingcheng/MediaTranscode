#pragma once

/**
 * @file MediaTranscode.h
 * @brief Umbrella header for third-party applications.
 *
 * Include this file when using MediaTranscode as a library. It exposes the
 * stable public API and hides internal FFmpeg pipeline headers.
 */

#include "media_transcode/Result.h"
#include "media_transcode/MediaTranscodeTypes.h"
#include "media_transcode/ITranscoder.h"
#include "media_transcode/FFmpegTranscoder.h"
#include "media_transcode/TranscodeSession.h"
