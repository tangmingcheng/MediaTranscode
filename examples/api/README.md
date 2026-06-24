# MediaTranscode public API examples

These examples are intentionally small and focused. They demonstrate the public C++ API directly, without CLI parsing infrastructure or application-specific wrappers.

## Examples

- `local_transcode_sync.cpp` — synchronous local file transcode.
- `local_transcode_async.cpp` — asynchronous local file transcode with a job handle.
- `local_transcode_cancel.cpp` — asynchronous transcode cancellation through the opaque job handle.
- `local_transcode_error_handling.cpp` — public `Result<T>` / `ErrorInfo` error handling.

The examples default to `disableHardware = true` so they stay portable across developer machines and CI environments. Hardware-oriented examples should be added separately once hardware test coverage is explicit.
