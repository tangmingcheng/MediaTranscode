# CI FFmpeg Pin Fix Report

## Scope

- Replaced the deleted mutable release asset ID with an immutable BtbN release tag and asset name.
- Preserved fail-closed SHA256 and extracted SDK layout validation.
- Added a focused PowerShell contract test without downloading the 80 MB SDK archive.

## TDD evidence

- RED 1: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/ci/test_prepare_ffmpeg_sdk_contract.ps1` failed with `Workflow must pin the immutable FFmpeg release tag.`
- GREEN 1: the same command passed after changing the workflow and preparation script contract.
- RED 2: the test failed with `Workflow must execute the FFmpeg SDK contract test.`
- GREEN 2: the same command passed after adding the deterministic-tier contract-test step.

The test statically verifies the approved tag, asset, and checksum; rejects asset-ID/latest references; injects a small local download payload; verifies the exact immutable URI; verifies required-environment validation occurs before download; and verifies checksum mismatch failure.

## Files

- `.github/scripts/prepare-ffmpeg-sdk.ps1`
- `.github/workflows/test-tiers.yml`
- `tests/ci/test_prepare_ffmpeg_sdk_contract.ps1`
- `.superpowers/sdd/ci-ffmpeg-pin-fix-report.md`

## Verification

- Focused PowerShell contract test: PASS.
- PowerShell parser for the production script and contract test: PASS.
- YAML parse with PyYAML: PASS.
- Mutable `latest` / asset-ID scan of production CI files: PASS.
- GitHub release API: pinned asset exists, state `uploaded`, size `79687255`, digest `sha256:3cd74234a1c9acf2546143bcc68b3ffe84e16f32e3be32f5096d61844739b17d`.
- Asset URL HEAD request: HTTP 200 with content length `79687255`.
- `git diff --check`: PASS.

## Residual risks

- BtbN controls retention of the external immutable release; deletion would require intentionally selecting and reviewing a new pinned asset.
- PowerShell 7 is not installed locally, so runtime verification used Windows PowerShell 5.1; both scripts parse cleanly and use syntax supported by the GitHub Actions `pwsh` environment.
- The full archive was not downloaded locally; integrity is supported by GitHub's release-asset digest metadata and remains enforced by the CI script after download.
