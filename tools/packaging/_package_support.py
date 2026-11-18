"""Shared filesystem, integrity and ELF checks for the two RKMPP packagers."""
import hashlib
import os
from pathlib import Path
import platform
import subprocess


def run(args, **kwargs):
    env = dict(os.environ, LC_ALL="C")
    env.pop("LD_PRELOAD", None)
    env.pop("LD_AUDIT", None)
    for key, value in kwargs.pop("environment", {}).items():
        if value is None:
            env.pop(key, None)
        else:
            env[key] = value
    return subprocess.check_output([str(a) for a in args], env=env,
                                   text=True, **kwargs).strip()


def require_host():
    if platform.system() != "Linux" or platform.machine() != "aarch64":
        raise ValueError("Packaging requires the validated Linux aarch64 RKMPP host")


def digest(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def files(root):
    result = []
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root).as_posix()
        if any(c in relative for c in "\n\r\\"):
            raise ValueError("Unsupported manifest filename: " + repr(relative))
        if path.is_symlink():
            raise ValueError("Package files must be materialized, not symlinks: " + relative)
        if path.is_file() and relative not in ("SHA256SUMS", ".INCOMPLETE"):
            result.append(path)
        elif not path.is_file() and not path.is_dir():
            raise ValueError("Unsupported package member: " + relative)
    return result


def write_manifest(root):
    (root / "SHA256SUMS").write_text("".join(
        digest(p) + "  " + p.relative_to(root).as_posix() + "\n" for p in files(root)),
        encoding="utf-8")


def verify_manifest(root):
    expected = {}
    for line in (root / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
        checksum, name = line.split("  ", 1)
        if name in expected:
            raise ValueError("Duplicate manifest member: " + name)
        expected[name] = checksum
    actual = {p.relative_to(root).as_posix(): digest(p) for p in files(root)}
    if not actual or expected != actual:
        raise ValueError("Package manifest mismatch (missing, changed or extra files)")
    run(["sha256sum", "-c", "SHA256SUMS"], cwd=root)


def create_output(path):
    # mkdir is exclusive even if another invocation races this one.
    path = Path(path).absolute()
    path.parent.resolve(strict=True)
    path.mkdir()
    (path / ".INCOMPLETE").write_text("Packaging has not completed.\n", encoding="utf-8")
    return path


def finish(root, validate=None):
    write_manifest(root)
    verify_manifest(root)
    if validate is not None:
        validate(root)
    (root / ".INCOMPLETE").unlink()
    print(root)


def require_elf(path):
    header = run(["readelf", "-h", path])
    if "AArch64" not in header or "ELF64" not in header:
        raise ValueError("Expected AArch64 ELF64: " + str(path))


def entry(main):
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit("Packaging failed; any .INCOMPLETE directory is not a release: " + str(error))
