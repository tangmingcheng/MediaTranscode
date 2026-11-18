#!/usr/bin/env python3
"""Assemble one RKMPP Release build into a versioned SDK release directory."""
import argparse
import json
from pathlib import Path
import re
import shlex
import shutil
import tempfile
import sys

sys.dont_write_bytecode = True
from _package_support import create_output, digest, entry, finish, require_elf, require_host, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("release_dir", type=Path)
    parser.add_argument("--source-revision", required=True,
                        help="Full source Git commit verified against this build")
    parser.add_argument("--validation-record", required=True, type=Path,
                        help="Completed validation record for this exact build")
    args = parser.parse_args()
    require_host()
    if not re.fullmatch(r"[0-9a-f]{40}", args.source_revision):
        raise ValueError("source-revision must be a full Git SHA-1")
    build = args.build_dir.resolve(strict=True)
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        match = re.match(r"([^:#]+):[^=]+=(.*)", line)
        if match:
            cache[match[1]] = match[2]
    if cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise ValueError("Expected an existing Release CMake build")
    source = Path(cache["CMAKE_HOME_DIRECTORY"]).resolve(strict=True)
    # Use the scripts that belong to the same source tree as the build.
    if Path(__file__).resolve().parent != source / "tools/packaging":
        raise ValueError("Run the packager from the CMake source checkout")
    archives = ["libmedia_transcode_beta.a", "libmedia_transcode_realtime_application.a",
                "libmedia_transcode_core.a"]
    inputs = [build / name for name in archives]
    inputs += [build / "media_transcode_realtime_video_cli",
               source / "include/media_transcode_beta/realtime.h",
               source / "examples/rkmpp_composition.c", source / "examples/rkmpp_composition.md",
               source / "LICENSE.txt", args.validation_record.resolve(strict=True)]
    for path in inputs:
        if not path.is_file():
            raise ValueError("Missing build/source input: " + str(path))
    require_elf(inputs[3])
    for key in ("CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "CMAKE_AR", "CMAKE_RANLIB"):
        if not Path(cache[key]).is_file():
            raise ValueError("Tool from CMake cache is unavailable: " + key)
    rga = Path(cache["MEDIA_RGA_LIBRARY"]).resolve(strict=True)
    require_elf(rga)
    inputs += [rga, build / "CMakeCache.txt"]
    packages = ["libavfilter", "libavcodec", "libavformat", "libavutil", "libswscale", "libswresample"]
    link_flags = shlex.split(run(["pkg-config", "--libs", *packages]))
    out = create_output(args.release_dir)
    for name in ("bin", "lib", "internal", "include/media_transcode_beta", "examples", "tools/packaging"):
        (out / name).mkdir(parents=True)
    for name in archives:
        destination = "libmedia_transcode_beta_facade.a" if name == archives[0] else name
        shutil.copy2(build / name, out / "internal" / destination)
    # Relative fixed MRI names avoid treating spaces in build paths as commands.
    with tempfile.TemporaryDirectory(prefix="mt-archive-") as temporary:
        work = Path(temporary)
        for index, path in enumerate(inputs[:3]):
            shutil.copy2(path, work / (str(index) + ".a"))
        run([cache["CMAKE_AR"], "-M"], cwd=work,
            input="CREATE merged.a\nADDLIB 0.a\nADDLIB 1.a\nADDLIB 2.a\nSAVE\nEND\n")
        run([cache["CMAKE_RANLIB"], work / "merged.a"])
        shutil.copy2(work / "merged.a", out / "lib/libmedia_transcode_beta.a")
    shutil.copy2(inputs[3], out / "bin" / inputs[3].name)
    shutil.copy2(inputs[4], out / "include/media_transcode_beta/realtime.h")
    for path in inputs[5:7]:
        shutil.copy2(path, out / "examples" / path.name)
    shutil.copy2(inputs[7], out / "LICENSE.txt")
    shutil.copy2(inputs[8], out / "VALIDATION.md")
    for name in ("package_release.py", "package_portable.py", "_package_support.py"):
        shutil.copy2(source / "tools/packaging" / name, out / "tools/packaging" / name)
    with tempfile.TemporaryDirectory(prefix="mt-example-") as temporary:
        obj = Path(temporary) / "composition.o"
        run([cache["CMAKE_C_COMPILER"], "-std=c11", "-Wall", "-Wextra", "-Werror",
             "-I" + str(out / "include"), "-c", out / "examples/rkmpp_composition.c", "-o", obj])
        run([cache["CMAKE_CXX_COMPILER"], obj, out / "lib/libmedia_transcode_beta.a",
             *link_flags, rga, "-pthread", "-ldl", "-o", out / "bin/rkmpp_composition"])
    require_elf(out / "bin/rkmpp_composition")
    metadata = {"format": 1, "kind": "rkmpp-sdk", "declared_source_revision": args.source_revision,
                "build_dir": str(build), "source_dir": str(source), "architecture": "aarch64",
                "input_sha256": {str(p): digest(p) for p in inputs},
                "pkg_config_versions": {p: run(["pkg-config", "--modversion", p]) for p in packages},
                "compilers": {k: cache[k] for k in ("CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER")}}
    (out / "VERSION.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    (out / "README.md").write_text(
        "# RKMPP SDK 发布目录\n\n使用同包 include 和 lib；参数与调用见 examples/rkmpp_composition.md，"
        "实际验收见 VALIDATION.md。bin/rkmpp_composition 是由同包示例编译的程序。\n\n"
        "本目录尚依赖构建机的媒体运行库。使用 python3 tools/packaging/package_portable.py "
        "<本目录> <新部署目录> 生成portable目录；不会安装到系统。"
        "源码版本为调用方核实声明，输入哈希见VERSION.json；脚本不替代功能验收。\n",
        encoding="utf-8")
    finish(out)


if __name__ == "__main__":
    entry(main)
