# MediaTranscode

MediaTranscode is a C++ media transcoding library built around a capability-oriented public API and FFmpeg-backed internal implementations.

## Public API layout

```text
include/media_transcode/
    MediaTranscode.h          # Umbrella public header
    MediaTypes.h              # Shared public enum values
    LocalVideoTranscode.h     # Local video transcode capability API
    Result.h                  # Result<T> and ErrorInfo
```

For new code, prefer including the narrowest header you need:

```cpp
#include "media_transcode/LocalVideoTranscode.h"
```

Use `MediaTranscode.h` when you want the umbrella public API entry point.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Useful CMake options:

```text
MEDIA_TRANSCODE_BUILD_CLI=ON
MEDIA_TRANSCODE_BUILD_EXAMPLES=ON
MEDIA_TRANSCODE_BUILD_TESTS=ON
MEDIA_TRANSCODE_ENABLE_INTEGRATION_TESTS=ON
MEDIA_TRANSCODE_ENABLE_HARDWARE_TESTS=OFF
MEDIA_TRANSCODE_TEST_SAMPLES_DIR=<path>
```

Hardware-dependent tests are intentionally disabled by default. The default examples and integration tests force software mode so they remain portable across developer machines and CI environments.

## Examples

Small public API examples live in:

```text
examples/api/
```

Current examples:

```text
local_transcode_sync.cpp            # Synchronous local video transcode
local_transcode_async.cpp           # Async transcode with opaque job handle
local_transcode_cancel.cpp          # Stop/wait job lifecycle
local_transcode_error_handling.cpp  # Result<T> / ErrorInfo handling
```

The CLI-oriented demo remains separate:

```text
examples/transcode_cli/
```

## Tests

Enable tests at configure time:

```bash
cmake -S . -B build -DMEDIA_TRANSCODE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Test groups:

```text
tests/compile/      # Public header compile/include tests
tests/unit/         # Fast public API and validation tests
tests/integration/  # FFmpeg-backed smoke tests
tests/samples/      # Small media samples used by integration/regression tests
```

The integration smoke test uses:

```text
tests/samples/sample_h264_aac_320x240.mp4
```

You can override the sample directory:

```bash
cmake -S . -B build -DMEDIA_TRANSCODE_TEST_SAMPLES_DIR=/path/to/samples
```

If a required integration sample is missing, the test returns `77`, and CTest marks it as skipped.

## Current public API pattern

Local video transcoding uses an opaque job handle plus free functions:

```cpp
media::LocalVideoTranscodeConfig config;
config.inputPath = "input.mp4";
config.outputPath = "output.mp4";
config.videoCodec = media::VideoCodec::H264;
config.disableHardware = true;

const auto result = media::startLocalVideoTranscodeSync(config);
if (!result) {
    // result.error().describe()
}
```

Async usage returns `LocalVideoTranscodeJobHandle`; callers operate on it through `waitLocalVideoTranscode`, `stopLocalVideoTranscode`, and query functions.
