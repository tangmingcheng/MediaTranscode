# A/V Sync Task 5 Completion

Task 5 adds one protocol-neutral boundary from mapped source time to graph running time.

## Result

- `MediaCanonicalTimeMapper` maps presentation and decode time independently with checked signed-nanosecond arithmetic.
- Every request and result carries explicit source identity, confidence, and generation.
- Missing presentation evidence is rejected; decode time is never substituted for presentation time.
- Empty or mismatched source identity, negative duration, overflow, and old or future generation data fail with structured synchronization context.
- Mapper failures retain a typed `MediaAvSyncErrorCode`, topology, lifecycle state, identities, generations, observed source time, epochs, and signed running-time bounds; callers explicitly choose when to translate them to `ErrorInfo`.
- The existing `Result<T>` abstraction now supports an explicit error type while preserving `ErrorInfo` as its default contract.
- Reset accepts only an explicit higher generation and leaves the active mapping unchanged on failure.
- `MediaMappedTimestamp` can only be constructed by the canonical mapper.
- Protocol clock estimation, pacing, dropping, discontinuity decisions, and output time-base serialization remain outside this component.
- The graph time audit found no second canonical source-epoch-to-running-time mapper: RTP/MPEG-TS mappers retain protocol timestamp estimation, while `MediaRunningTime` retains primitive checked arithmetic and time-base conversion.

## Verification

Commands were executed from the repository root in PowerShell. Build commands are omitted.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R '^media_transcode_core_tests$'
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration
```

Result after the final required clean rebuild: focused core passed 1/1 in 3.33 seconds, deterministic passed 6/6 in 4.04 seconds, and integration passed 3/3 in 149.16 seconds.

The first independent review found an intermediate-overflow defect and insufficiently typed error context. The implementation now evaluates the complete affine expression before range validation, covers positive, negative, and three-term cancellation at signed 64-bit boundaries, and exposes machine-readable synchronization errors without flattening them in the mapper.

The final review's two non-blocking test suggestions were closed before push: core coverage now asserts a typed error when the complete affine result is below `INT64_MIN`, and directly fixes the default `Result<T>` and `Result<void>` success/error normalization contract. The focused core rerun passed 1/1 in 3.34 seconds and deterministic rerun passed 6/6 in 3.86 seconds.
