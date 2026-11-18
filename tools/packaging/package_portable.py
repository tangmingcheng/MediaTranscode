#!/usr/bin/env python3
"""Copy an RKMPP release's executables and resolved ELF closure into a portable directory."""
import argparse
import fnmatch
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tarfile

sys.dont_write_bytecode = True
from _package_support import (create_output, digest, entry, files, finish,
                              require_elf, require_host, run, verify_manifest)


def system_library(name):
    # glibc and its loader belong to the target OS, never to this bundle.
    return any(fnmatch.fnmatchcase(name, pattern) for pattern in (
        "ld-linux-aarch64.so.*", "libc.so.*", "libm.so.*", "libpthread.so.*",
        "libdl.so.*", "librt.so.*", "libresolv.so.*", "libutil.so.*",
        "libanl.so.*", "libBrokenLocale.so.*", "libthread_db.so.*",
        "libnss_dns.so.*", "libnss_files.so.*", "libnss_compat.so.*", "libnss_hesiod.so.*"))


def dependencies(elf, clean=False):
    # Only use this on trusted, locally built ELF files. ldd may invoke a loader.
    env = {"LD_LIBRARY_PATH": None} if clean else {}
    output = run(["ldd", elf], environment=env)
    result = {}
    for line in output.splitlines():
        if "not found" in line:
            raise ValueError("Unresolved dependency: " + line.strip())
        matched = re.fullmatch(r"\s*(\S+) => (/.+) \(0x[0-9a-fA-F]+\)\s*", line)
        if matched:
            name, path = matched.groups()
            result[name] = Path(path).resolve(strict=True)
        elif re.fullmatch(r"\s*/.+ \(0x[0-9a-fA-F]+\)\s*", line):
            path = Path(line.strip().rsplit(" (", 1)[0])
            if not system_library(path.name):
                raise ValueError("Absolute DT_NEEDED or unexpected loader: " + str(path))
        elif not re.fullmatch(r"\s*linux-vdso\.so\.\d+ \(0x[0-9a-fA-F]+\)\s*", line):
            raise ValueError("Unsupported ldd result: " + line)
    return result


def verify_runtime(root, building=False):
    if not building and (root / ".INCOMPLETE").exists():
        raise ValueError("Incomplete package")
    verify_manifest(root)
    for directory, rpath in (("bin", "$ORIGIN/../lib"), ("lib", "$ORIGIN")):
        for elf in sorted((root / directory).iterdir()):
            require_elf(elf)
            if run(["patchelf", "--print-rpath", elf]) != rpath:
                raise ValueError("Unexpected runtime path: " + str(elf))
            for name, actual in dependencies(elf, clean=True).items():
                if not system_library(name) and actual != (root / "lib" / name).resolve():
                    raise ValueError("Dependency escaped package: " + name + " -> " + str(actual))
    print("Runtime closure and all file checksums verified: " + str(root))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("release_dir", nargs="?", type=Path)
    parser.add_argument("portable_dir", nargs="?", type=Path)
    parser.add_argument("--runtime-library", action="append", default=[], metavar="LOAD_NAME=PATH",
                        help="Exact dlopen basename and absolute library path; repeat as needed")
    parser.add_argument("--archive", action="store_true", help="Also create adjacent tar.gz and sha256")
    parser.add_argument("--check", type=Path, help="Check an existing portable directory without packaging")
    args = parser.parse_args()
    require_host()
    if args.check:
        if args.release_dir or args.portable_dir or args.runtime_library or args.archive:
            parser.error("--check cannot be combined with packaging arguments")
        verify_runtime(args.check.resolve(strict=True))
        return
    if not args.release_dir or not args.portable_dir:
        parser.error("release_dir and portable_dir are required")
    release = args.release_dir.resolve(strict=True)
    if (release / ".INCOMPLETE").exists():
        raise ValueError("Release directory is incomplete")
    verify_manifest(release)
    metadata = json.loads((release / "VERSION.json").read_text())
    if metadata.get("kind") != "rkmpp-sdk" or metadata.get("architecture") != "aarch64":
        raise ValueError("Expected a release produced by package_release.py")
    programs = sorted((release / "bin").iterdir())
    if not programs:
        raise ValueError("Release contains no executables")
    libraries = {}

    def include(name, path):
        if Path(name).name != name or name in (".", ".."):
            raise ValueError("Invalid library name: " + name)
        if system_library(name):
            return
        path = path.resolve(strict=True)
        require_elf(path)
        if name in libraries and digest(libraries[name]) != digest(path):
            raise ValueError("Conflicting libraries for SONAME " + name)
        libraries[name] = path

    extra = []
    for specification in args.runtime_library:
        load_name, separator, location = specification.partition("=")
        if not separator or not Path(location).is_absolute() or system_library(load_name):
            raise ValueError("Expected non-system dlopen LOAD_NAME=/absolute/path")
        origin = Path(location).resolve(strict=True)
        include(load_name, origin)
        extra.append(origin)
    roots = programs + extra
    for elf in roots:
        require_elf(elf)
        if elf not in programs:
            name = run(["patchelf", "--print-soname", elf])
            if not name:
                raise ValueError("Explicit runtime library has no SONAME: " + str(elf))
            include(name, elf)
        for name, path in dependencies(elf).items():
            include(name, path)
    destination = args.portable_dir.absolute()
    if destination.resolve() == release or release in destination.resolve().parents:
        raise ValueError("Portable output must be outside the immutable release directory")
    archive = destination.with_name(destination.name + ".tar.gz")
    checksum = archive.with_name(archive.name + ".sha256")
    if args.archive and (archive.exists() or archive.is_symlink() or checksum.exists() or checksum.is_symlink()):
        raise ValueError("Archive/checksum destination already exists")
    out = create_output(destination)
    for name in ("bin", "lib", "tools/packaging", "examples"):
        (out / name).mkdir(parents=True)
    for program in programs:
        shutil.copy2(program, out / "bin" / program.name)
    for name, origin in libraries.items():
        # Materialize each needed SONAME; no dangling/external symlink can ship.
        shutil.copy2(origin, out / "lib" / name)
    for directory, rpath in (("bin", "$ORIGIN/../lib"), ("lib", "$ORIGIN")):
        for elf in sorted((out / directory).iterdir()):
            run(["patchelf", "--set-rpath", rpath, elf])
    for directory in ("bin", "lib"):
        for elf in sorted((out / directory).iterdir()):
            for name, actual in dependencies(elf, clean=True).items():
                if not system_library(name) and actual != (out / "lib" / name).resolve():
                    raise ValueError("Dependency escaped package: " + name + " -> " + str(actual))
    shutil.copy2(release / "VERSION.json", out / "RELEASE_VERSION.json")
    for name in ("VALIDATION.md", "LICENSE.txt"):
        shutil.copy2(release / name, out / name)
    shutil.copy2(release / "examples/rkmpp_composition.md", out / "examples/rkmpp_composition.md")
    for name in ("package_portable.py", "_package_support.py"):
        shutil.copy2(Path(__file__).resolve().parent / name, out / "tools/packaging" / name)
    (out / "VERSION.json").write_text(json.dumps({
        "format": 1, "kind": "rkmpp-portable", "architecture": "aarch64",
        "release_manifest_sha256": digest(release / "SHA256SUMS"),
        "declared_source_revision": metadata["declared_source_revision"],
        "runtime_sources": {name: {"path": str(p), "sha256": digest(p)} for name, p in libraries.items()},
        "loader": run(["patchelf", "--print-interpreter", programs[0]]),
        "host_glibc": run(["getconf", "GNU_LIBC_VERSION"]),
    }, indent=2) + "\n", encoding="utf-8")
    (out / "README.md").write_text(
        "# RKMPP 可移植程序目录\n\nbin内程序通过包内RUNPATH加载lib，"
        "无需启用ffenv。目标仍须匹配aarch64 Linux、glibc、内核及RKMPP/RGA驱动/设备权限。\n\n"
        "先清除LD_LIBRARY_PATH、LD_PRELOAD、LD_AUDIT，运行：\n"
        "`python3 tools/packaging/package_portable.py --check .`\n\n"
        "再按VALIDATION.md复验真实realtime链路；调用参数见examples/rkmpp_composition.md。"
        "本目录不提供SDK编译库，SDK留在第一阶段发布目录。静态依赖检查不替代媒体验收。\n",
        encoding="utf-8")
    finish(out, lambda root: verify_runtime(root, building=True))
    if args.archive:
        pending_archive = archive.with_name(archive.name + ".INCOMPLETE")
        pending_checksum = checksum.with_name(checksum.name + ".INCOMPLETE")
        with pending_archive.open("xb") as stream:
            with tarfile.open(fileobj=stream, mode="w:gz") as bundle:
                bundle.add(out, arcname=out.name)
        with pending_checksum.open("x", encoding="utf-8") as stream:
            stream.write(digest(pending_archive) + "  " + archive.name + "\n")
        # Publish only complete files, atomically and without replacing a race winner.
        os.link(pending_archive, archive)
        os.link(pending_checksum, checksum)
        pending_archive.unlink()
        pending_checksum.unlink()
        print(archive)


if __name__ == "__main__":
    entry(main)
