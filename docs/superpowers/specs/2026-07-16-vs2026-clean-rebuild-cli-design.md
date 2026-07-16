# VS2026 Clean Rebuild CLI Design

## Goal

Provide a durable Windows CLI named `vs2026-rebuild` that removes `/showIncludes`
from CMake-generated Ninja rules, performs a full clean rebuild with Visual Studio
2026, and proves that generated rules still contain zero `/showIncludes` tokens.
The CLI must not modify `CMakeLists.txt` or store external paths in project CMake.

## Ownership and installation

- Track CLI source and tests under `tools/vs2026-rebuild/`.
- Track the companion skill source under
  `tools/vs2026-rebuild/skill/vs2026-clean-rebuild/`.
- Install the command into a user PATH directory and install the skill into
  `~/.codex/skills/vs2026-clean-rebuild/`.
- Use Windows PowerShell so the tool has no new runtime dependency.

## Command contract

- `vs2026-rebuild doctor`: inspect PowerShell, CMake, Ninja rules, VS2026,
  repository/build paths, and current `/showIncludes` count without mutation.
- `vs2026-rebuild prepare`: remove only the exact `/showIncludes` compiler token
  from `<build-dir>/CMakeFiles/rules.ninja`, atomically preserve UTF-8 content,
  and report before/after counts.
- `vs2026-rebuild rebuild`: run `prepare`, then
  `cmake --build <build-dir> --clean-first --target all --config <config> -j 1`,
  then fail unless the command succeeds and the final token count is zero.
- `vs2026-rebuild raw-build -- <args>`: explicit CMake build escape hatch; it
  does not imply preparation or successful `/showIncludes` verification.
- All commands accept `--repo`, `--build-dir`, `--vs-root`, and `--json`.
  Defaults are limited to the current repository, its existing
  `out/build/x64-debug`, and `D:\VisualStudio2026`; no value is written to CMake.

JSON success uses `{ "ok": true, "command": ..., "data": ... }`. JSON failure
uses `{ "ok": false, "command": ..., "error": { "code": ..., "message": ... } }`
and a nonzero exit code. JSON mode sends build output to a log file so stdout
remains machine-readable.

## Safety and failure behavior

- Resolve and verify repository/build paths before writing.
- Write only the generated `CMakeFiles/rules.ninja` file during `prepare`.
- Fail if the file is missing, unreadable, outside the selected build directory,
  or contains an unsupported encoding.
- Never configure CMake, modify project CMake files, copy FFmpeg dependencies,
  retry a failed build, or reinterpret a failing build as success.
- If CMake regenerates `/showIncludes` during the build, report failure and leave
  the generated rule sanitized for the next explicit rebuild; do not hide the
  first failed acceptance by automatically rebuilding twice.

## Companion skill

The `vs2026-clean-rebuild` skill tells future Codex sessions to begin with
`vs2026-rebuild --json doctor`, use `prepare` before any affected build, use
`rebuild` for accepted verification, and treat a nonzero exit or nonzero final
token count as failure. It references CLI help instead of duplicating every flag.

## Verification

- Unit-test exact token removal, idempotency, path containment, missing files,
  stable JSON envelopes, command construction, and build/regeneration failures.
- Smoke-test `--help` and `--json doctor` from outside the repository.
- Run the CLI against `out/build/x64-debug`, confirm `/showIncludes` count zero,
  and complete one real clean rebuild.
- Initialize and validate the skill with the Skill Creator scripts, then run a
  fresh-Agent baseline without the skill and a forward test with the skill.
