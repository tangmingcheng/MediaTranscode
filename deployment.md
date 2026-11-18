# RKMPP 两阶段打包与部署

仅针对Linux aarch64/RKMPP。**先完成合屏功能验收，再执行打包**；当前只实现脚本并进行静态/入口检查，不生成正式目录或部署包。功能状态见[合屏计划](docs/realtime-video-composition-plan.md)。

参考旧包`/home/tang/packages/media-transcode-beta-rkmpp-475eb8e2-20260910`，统一为下面两个Python 3入口。旧`package_media_transcode_beta.sh`已替换，不再手工补文档/示例/校验清单。`_package_support.py`是共享校验模块，不是第三个打包阶段。

## 第一阶段：构建结果 → 发布目录

[package_release.py](tools/packaging/package_release.py)读取现有CMake Release构建，不重新编译媒体引擎；使用缓存中的源目录及C/C++编译器编译同包调用示例，不执行程序。

```bash
source /opt/mt-tools/mtenv.sh
mtenv on
# 启用与验收构建完全一致的FFmpeg、pkg-config和运行库环境。
python3 tools/packaging/package_release.py \
  /absolute/build/rk-release /absolute/releases/media-transcode-REVISION \
  --source-revision FULL_40_CHARACTER_GIT_COMMIT \
  --validation-record /absolute/completed-validation.md
```

从CMake缓存指定的源码目录运行脚本。完整提交号及验收记录须由交付者核实；脚本记录声明及输入哈希，不以文档存在代替验收，不自动证明旧构建与提交一致。

| 发布内容 | 来源/用途 |
|---|---|
| `lib/libmedia_transcode_beta.a` | GNU ar MRI合并Beta facade、realtime application、core |
| `internal/*.a` | 保留三个原始archive，facade单独命名 |
| `include/media_transcode_beta/realtime.h` | 构建源码目录的匹配头文件 |
| `bin/media_transcode_realtime_video_cli` | 已构建实时CLI，不混入历史二进制 |
| `examples/rkmpp_composition.c/.md` | C调用示例及逐参数说明 |
| `bin/rkmpp_composition` | 用同包头、合并库和CMake编译器生成的示例 |
| `README.md / VALIDATION.md / VERSION.json / LICENSE.txt` | 使用说明、验收、版本/编译器/pkg-config/输入哈希、项目许可 |
| `tools/packaging/` | 两个入口和共享模块的同源副本 |
| `SHA256SUMS` | 覆盖全部正式文件，清单自身除外 |

需要Python 3、readelf、sha256sum、pkg-config及缓存指定的GNU ar/ranlib、编译器和RGA库。示例显式链接构建实际使用的RGA库，FFmpeg链接使用当前已验收pkg-config环境；不能混用不同版本头文件和库。此阶段是SDK，仍依赖现场动态库。

## 第二阶段：发布目录 → 可移植程序目录

[package_portable.py](tools/packaging/package_portable.py)先核验发布目录完整清单，再收集`bin/`全部AArch64 ELF程序及其真实动态依赖。输出必须是发布目录以外的新目录。

```bash
python3 tools/packaging/package_portable.py \
  /absolute/releases/media-transcode-REVISION \
  /absolute/deploy/media-transcode-REVISION-portable \
  --archive
```

通过ldd收集实际FFmpeg、MPP、RGA、C++运行库等依赖闭包，不硬编码旧版本SONAME。仅对可信本机构建运行ldd，并保持与验收相同的运行库环境。通过dlopen加载的库以可重复的`--runtime-library libfoo.so=/absolute/libfoo.so.1.2`加入，脚本实体保留加载名和SONAME并收集依赖。maps仅提供实际文件，加载名须另从调用/平台配置核实；绝对路径dlopen不能仅靠搬迁库解决。缺库、非AArch64 ELF或同名不同内容库立即失败。

输出是纯运行目录：`bin/`、`lib/`、参数文档、验收记录、版本与依赖来源/原始哈希、许可和检查入口。SDK静态库、头文件和源示例留在第一阶段。动态库以所需SONAME实体复制，不产生悬空或指向包外的符号链接。

只修改包内复制品：bin RUNPATH=`$ORIGIN/../lib`，共享库RUNPATH=`$ORIGIN`。移除LD_LIBRARY_PATH、LD_PRELOAD、LD_AUDIT后核对非系统依赖全部来自包内。glibc及loader不打包；目标仍需匹配aarch64 Linux、glibc、内核、RKMPP/RGA驱动及设备权限。不会修改系统安装或外部FFmpeg。

额外需要ldd、patchelf、getconf。包内RUNPATH替代旧env/run-in-sdk环境脚本；统一使用下面的检查入口，避免多套重复逻辑。正常运行程序不依赖Python或检查工具。

## 完整性与交付验证

- 两阶段均拒绝覆盖现有目标，保留旧包回退。创建目录时写入`.INCOMPLETE`，失败目录仅供诊断，不得交付；重试使用新目录。
- 所有正式文件写入SHA256SUMS并执行`sha256sum -c`，同时比对完整文件集合，额外文件也会失败。发布目录禁止符号链接，部署库实体化后校验。
- `--archive`可选生成同级tar.gz及独立sha256，拒绝覆盖现有归档。归档先写带`.INCOMPLETE`后缀的文件，完整写入后才发布；归档失败不能凭目录存在宣称归档交付完成。
- 脚本不安装系统、不切换运行服务。实际打包、解包及媒体复验均等待合屏功能验证完成。

正式交付必须从新目录解包，然后执行：

```bash
env -u LD_LIBRARY_PATH -u LD_PRELOAD -u LD_AUDIT \
  python3 tools/packaging/package_portable.py --check /absolute/unpacked/program-directory
```

检查需要Python 3、readelf、patchelf、ldd、sha256sum。通过后按VALIDATION.md执行真实源→realtime CLI/C调用→VLC链路，核对进程maps、画面/音频、退出和清理；SDK示例须用同包头与库重新编译。哈希、ELF检查和示例编译不能替代功能验收。

## 边界与依据

本轮不实现Windows打包；Windows构建继续使用[VS2026技能](.agents/skills/building-with-vs2026/SKILL.md)，本机FFmpeg保持不变。

[旧Beta交付](docs/dynamic-video-beta-package.md)与[旧依赖部署](docs/dynamic-video-rkmpp-dependency-deployment.md)仅代表历史产物。脚本复用[GNU ar MRI](https://sourceware.org/binutils/docs/binutils/ar-scripts.html)和[patchelf](https://github.com/NixOS/patchelf)机制，不改变媒体DAG或时序策略。
