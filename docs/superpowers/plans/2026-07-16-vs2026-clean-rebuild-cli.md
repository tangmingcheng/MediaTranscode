# VS2026 Clean Rebuild CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build, install, and validate `vs2026-rebuild`, plus a companion Codex skill that reliably performs VS2026 clean rebuilds with `/showIncludes` absent from generated Ninja rules.

**Architecture:** A thin PowerShell entrypoint parses commands and emits stable text/JSON; a focused core module owns path validation, exact-token sanitization, command construction, and subprocess results. A separate installer copies the command wrapper to a user PATH directory and installs a concise companion skill from versioned project source.

**Tech Stack:** Windows PowerShell 5.1+, CMake, Ninja-generated `rules.ninja`, standard PowerShell test harness, Codex Skill Creator scripts.

## Global Constraints

- Do not modify `CMakeLists.txt` or any project CMake source.
- Do not write `D:\VisualStudio2026` or any external dependency path into CMake.
- The accepted build command is `cmake --build <build-dir> --clean-first --target all --config <config> -j 1`.
- `/showIncludes` must be removed from generated `CMakeFiles/rules.ninja` before build and its final count must be zero.
- Never retry a failed build automatically or report a failed build as success.
- Project files are UTF-8 without BOM and CRLF.
- CLI JSON stdout contains one stable envelope; diagnostics and progress use stderr or a returned log path.

---

### Task 1: PowerShell CLI core and command surface

**Files:**
- Create: `tools/vs2026-rebuild/Vs2026Rebuild.Core.psm1`
- Create: `tools/vs2026-rebuild/vs2026-rebuild.ps1`
- Create: `tools/vs2026-rebuild/vs2026-rebuild.cmd`
- Create: `tools/vs2026-rebuild/tests/Vs2026Rebuild.Tests.ps1`
- Create: `tools/vs2026-rebuild/README.md`

**Interfaces:**
- Consumes: repository root, build directory, VS root, config, JSON flag, raw CMake arguments.
- Produces: `doctor`, `prepare`, `rebuild`, and `raw-build`; stable success/error envelopes; exit code 0 only for complete success.

- [ ] **Step 1: Write the failing core tests**

  Add fixture-backed tests that import `Vs2026Rebuild.Core.psm1` and assert:
  `Resolve-RebuildPaths` rejects a rules file outside the selected build directory;
  `Remove-ShowIncludesToken` changes only standalone `/showIncludes`, is idempotent,
  and preserves UTF-8 without BOM; `New-CMakeBuildArguments` returns exactly
  `--build`, build path, `--clean-first`, `--target`, `all`, `--config`, config,
  `-j`, `1`; JSON helpers return the documented envelope; a nonzero subprocess
  result and a post-build nonzero token count both fail.

- [ ] **Step 2: Run the tests and verify RED**

  Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/vs2026-rebuild/tests/Vs2026Rebuild.Tests.ps1`

  Expected: nonzero exit because `Vs2026Rebuild.Core.psm1` and its exported functions do not exist.

- [ ] **Step 3: Implement the minimal core module**

  Export only `Resolve-RebuildPaths`, `Get-ShowIncludesCount`,
  `Remove-ShowIncludesToken`, `New-CMakeBuildArguments`, `Invoke-ExternalCommand`,
  `New-SuccessEnvelope`, and `New-ErrorEnvelope`. Use canonical full paths,
  directory containment checks, atomic same-directory replacement, exact
  case-insensitive token matching, and explicit error codes.

- [ ] **Step 4: Verify core GREEN and refactor**

  Run the Task 1 test command until every fixture passes. Refactor duplicate
  path/JSON logic only while the suite remains green.

- [ ] **Step 5: Add CLI behavior tests before the entrypoint**

  Add subprocess tests for `--help`, `--json doctor`, missing `rules.ninja`,
  idempotent `prepare`, `rebuild` command construction with a fixture executable,
  failed build preservation, final-token failure, and `raw-build -- <args>`.
  Confirm the tests fail because the command surface is absent.

- [ ] **Step 6: Implement entrypoint, wrapper, and CLI documentation**

  Parse global `--json`, `--repo`, `--build-dir`, `--vs-root`, and `--config`
  before one named command. `doctor` is read-only. `prepare` writes only the
  resolved generated rules file. `rebuild` prepares once, invokes the exact
  clean command once, verifies final count, and returns the log path in JSON.
  `raw-build` performs no implied preparation. Document all commands, JSON
  shapes, exit codes, installation, and examples in the README.

- [ ] **Step 7: Run all CLI tests and commit**

  Run the Task 1 test command and `vs2026-rebuild.ps1 --help`; expect zero exit.
  Commit only Task 1 files with message `feat: add vs2026 clean rebuild cli`.

### Task 2: Installer and real command smoke

**Files:**
- Create: `tools/vs2026-rebuild/install.ps1`
- Modify: `tools/vs2026-rebuild/tests/Vs2026Rebuild.Tests.ps1`
- Modify: `tools/vs2026-rebuild/README.md`

**Interfaces:**
- Consumes: CLI source directory, optional install bin, optional skill target.
- Produces: callable `vs2026-rebuild` command from any working directory; idempotent installation report.

- [ ] **Step 1: Write installer RED tests**

  Use temporary bin/skill targets and assert the installer copies the wrapper,
  script, and module without writing the repository or CMake, is idempotent,
  and reports a missing PATH entry without silently changing the machine PATH.

- [ ] **Step 2: Verify installer RED**

  Run the Task 1 test command; expect failure because `install.ps1` is absent.

- [ ] **Step 3: Implement and verify installer GREEN**

  Install versioned files atomically into the selected user bin support folder,
  install a small command wrapper in the bin root, and copy the skill only after
  Task 3 creates it. Run tests twice and expect the same installed file set.

- [ ] **Step 4: Install and smoke outside the repository**

  Install to the user's local bin, ensure that directory is on the current
  process PATH, then from another directory run:
  `vs2026-rebuild --help` and
  `vs2026-rebuild --json doctor --repo D:\Code\MyCode\MediaTranscode --build-dir out/build/x64-debug`.
  Expect one JSON envelope and an explicit current token count.

- [ ] **Step 5: Commit installer**

  Commit Task 2 files with message `feat: install vs2026 rebuild command`.

### Task 3: Companion skill, skill TDD, and acceptance

**Files:**
- Create: `tools/vs2026-rebuild/skill/vs2026-clean-rebuild/SKILL.md`
- Create: `tools/vs2026-rebuild/skill/vs2026-clean-rebuild/agents/openai.yaml`
- Modify: `tools/vs2026-rebuild/install.ps1`

**Interfaces:**
- Consumes: installed `vs2026-rebuild` CLI.
- Produces: auto-discoverable personal skill `vs2026-clean-rebuild`.

- [ ] **Step 1: Run a fresh-Agent baseline without the skill**

  Ask a fresh Agent to rebuild this repository with VS2026 while permanently
  suppressing `/showIncludes`, without naming the CLI or skill. Record whether
  it edits CMake, skips clean-first, omits the final count, or uses an incremental
  result. This is the skill RED evidence.

- [ ] **Step 2: Initialize the skill with Skill Creator**

  Run `init_skill.py vs2026-clean-rebuild --path tools/vs2026-rebuild/skill`
  with interface values: display name `VS2026 Clean Rebuild`, short description
  `Run verified clean VS2026 builds`, and default prompt beginning
  `Use $vs2026-clean-rebuild`.

- [ ] **Step 3: Write the minimal companion skill**

  The skill must tell future Codex sessions to locate the installed command,
  begin with `vs2026-rebuild --json doctor`, use `prepare` before affected
  direct builds, prefer `rebuild` for accepted verification, treat nonzero exit
  or token count as failure, and avoid CMake edits or automatic retry. Include
  three copy-pasteable examples and refer to `--help` for full flags.

- [ ] **Step 4: Validate and forward-test the skill**

  Run `quick_validate.py` on the skill source. Install the skill, then ask a new
  Agent `Use $vs2026-clean-rebuild to verify this repository can be rebuilt with
  VS2026 without /showIncludes.` Confirm it starts with doctor, uses the CLI,
  requests no CMake edit, and requires clean-first plus final zero-count evidence.

- [ ] **Step 5: Run real acceptance**

  From outside the repository run installed `vs2026-rebuild prepare`, verify
  `/showIncludes` count zero, then run installed `vs2026-rebuild rebuild` against
  `out/build/x64-debug`. Expect one clean build, exit zero, and final count zero.

- [ ] **Step 6: Review, commit, install, and push**

  Re-run CLI tests, `quick_validate.py`, outside-repo help/doctor, and real
  acceptance. Check UTF-8/CRLF and `git diff --check`. Commit with message
  `feat: add vs2026 rebuild skill`, install the final command and skill, push the
  branch, and obtain an independent Agent review with no open Critical/Important findings.
