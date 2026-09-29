# 构建产物与部署入口

本文说明现有交付边界；构建成功不代表真实媒体链路验收通过。发布前按 [AGENTS.md](AGENTS.md) 完成对应平台、协议与规格的验收，并记录源码版本、产物哈希和运行时依赖。当前合屏功能仍在实施，状态见 [合屏进度](docs/realtime-video-composition-progress.md)，不能据历史包记录认定可部署。

## Windows 本机构建

使用 [VS2026 构建技能](.agents/skills/building-with-vs2026/SKILL.md) 的唯一入口，复用 `out/build/x64-debug` 或 `out/build/x64-release`，全量重新生成。该流程用于本机构建，不是安装包生成流程。

[CMakeLists.txt](CMakeLists.txt) 定义 core、realtime application、Beta 三个静态库，以及启用 `MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS` 时的 local/realtime 两个 CLI。FFmpeg 头文件来自 `3rds/ffmpeg/include`，Debug 链接库来自 `3rds/ffmpeg/debug/lib`，其他配置来自 `3rds/ffmpeg/lib`。运行时需配套 FFmpeg DLL、其依赖和目标硬件驱动；现有 CMake 没有安装或 DLL 自动收集规则，不能仅复制 CLI 就认定交付完整。更换依赖不属于常规构建的隐含步骤。

## Linux / RKMPP 交付

Linux 使用 pkg-config 查找 FFmpeg；aarch64 额外通过 [FFmpeg 发现逻辑](cmake/MediaTranscodeFFmpegDiscovery.cmake) 比对 PATH 中 FFmpeg 报告的六个库版本与开发包版本。项目关闭 RPATH，运行库选择由部署环境负责。版本匹配不代替实际加载路径和硬件链路验证；交付时核对 `ldd` 与运行进程的 `/proc/<pid>/maps`，确认 FFmpeg、MPP、RGA 动态库来自预期环境；头文件来源另通过构建缓存与实际编译命令中的包含路径核对。

现有 [Beta 打包脚本](tools/packaging/package_media_transcode_beta.sh) 接收 `<release-build-dir> <package-dir>`；目标目录必须尚不存在。它读取构建缓存中的源码目录取得匹配头文件，并需要三个已构建 archive 和 realtime CLI：

| 产物 | 内容 |
|---|---|
| `include/media_transcode_beta/realtime.h` | 同一构建源码的 C API 头文件 |
| `lib/libmedia_transcode_beta.a` | 合并 Beta facade、realtime application、core |
| `internal/` | 三个原始 archive，其中 facade 重命名保存 |
| `bin/media_transcode_realtime_video_cli` | 实时 CLI；不含 local CLI |
| `SHA256SUMS` | 脚本生成时 `include/lib/bin/internal` 内文件的校验清单 |

该脚本不编译项目、不打包 FFmpeg/MPP/RGA、不部署到系统，也不生成示例、README 或 VERSION 内容。若按交付要求补入这些文件，需重新生成覆盖最终文件集的校验清单并验证；Beta 调用方使用同包头文件重新编译，不能混用其他版本的头文件和库。这是依赖目标运行环境的静态业务库包，不是包含全部依赖的系统包。

## 已有证据与适用范围

- [RKMPP Beta 交付记录](docs/dynamic-video-beta-package.md)：记录 2026-09-10 的具体源码版本、包内容、示例与哈希；不是当前分支自动生成的交付清单。
- [RKMPP 依赖部署证据](docs/dynamic-video-rkmpp-dependency-deployment.md)：记录当时独立 FFmpeg 安装及 MPP/RGA 加载路径；其历史修复不授权当前任务修改或重建依赖。
- 实际 CLI/FFmpeg/VLC 命令及结果随各次验收记录维护，新的产物或部署环境必须重新取得对应证据，不能沿用旧包的通过结论。
