# rk386：RKMPP 基本合屏准备

2026-11-04。按用户最新要求，先推进 RKMPP 基本功能，再集中进行 Windows/RKMPP 细致可靠性测试；Windows r385 仍为 FAIL。

源码检查点 `09093919`，独立远端目录 `/home/tang/MediaTranscode-composition-09093919`。保留 `/home/tang/MediaTranscode` 原目录；不修改 FFmpeg、MPP、RGA 依赖。当前尚未启动媒体测试。

## 环境与构建状态

- SSH 密码认证成功；aarch64，Linux 5.10.110；启用 `/opt/mt-tools/mtenv.sh` 后 GCC 12.4.0、CMake 3.31.7。
- 启用 `/etc/profile.d/ffenv.sh` 的 ffenv 后使用 `/home/tang/ffmpeg-ab1e61a`；CMake 核验运行库与 pkg-config 六库版本一致。
- 首次全量构建退出1：源码包漏带仓库内 `3rds/include`，在 MediaGraphRuntimeTrace.cpp 报缺少 spdlog/spdlog.h。不是 FFmpeg 依赖故障。
- 已补传同一提交的 `3rds/include`；再次 clean-first/all-target 构建已启动，SSH 会话 11380，终态标记 `RK386_REBUILD_EXIT`，结果待回收。不要重启尚在运行的构建。
- CMAKE_C_COMPILER 未使用的配置告警源于项目仅启用 CXX；另有 FFmpeg pix_fmts 弃用告警，均不记为构建通过或媒体失败。

## 输入与临时文件清单

本机指定连续源 SHA256：`6E0DE760672A6B8386B59B22F824DE442E2A5EF7594D7B872C6851B64CABCA9B`；远端现有同名源尚待哈希比对，不以同名或文件长度代替一致性验证。

本轮临时传输文件：

| 本机 out/acceptance 文件 | 远端独立目录文件 | SHA256 |
| --- | --- | --- |
| composition-rk386-source.tar | source.tar | FCD6394B354F8C7E976AFE6C9C22FFE1C697128E6C98FDF37AC03A4FBD688015 |
| composition-rk386-includes.tar | includes.tar | 9296422982CEF0D68FCAE7E8ACEF77FD290C29682C66D65D6A4EE5C0A97B92AD |

源代码包远端哈希已匹配；补充头文件包已传输并成功解包，远端哈希仍待比对。记录最终结果后按此清单清理临时包，不清空旧验收目录或删除复用媒体。

## 后续

取得构建终态后核对真实加载库、源哈希和输出接收网络路径，执行与 Windows r385 相同媒体规格的三源 realtime 合屏。源码预查指出输出网络账本的 GCC reservation allocator geometry 尚未支持；须以实际启动证据确认并修复项目内能力 adapter，不用默认容量或跳过账本。此前 dormant SourcePresentation 容器能力不能当作当前 CanonicalIntervals 链路的首阻塞。

用户要求全 CPU 核心编译后，已明确中止原 4 并行构建（Ninja 报 interrupted by user，非编译故障或通过）。目标 nproc=8，内存7.7GiB、当时可用6.2GiB。当前同一会话11380执行 clean-first/all-target、--parallel 8，终态标记改为 RK386_ALL_CORES_EXIT；已清理72个旧构建文件并开始重建，结果待回收。

本轮补充核验：远端复用120秒源与本机 SHA256 一致，includes.tar 的远端哈希也与表中一致。远端到本机192.168.96.122的路由经192.168.130.254/eth0；路由存在不等于UDP已成功接收。实际 Ninja PID1777165，参数 -j 8 all，构建仍在进行。

只读源码调查进一步确认：远端 /home/tang/ffmpeg-rkmpp-close-ownership HEAD 为 ab1e61adaa21ff129caa8e01a1198156044567ed；rkmppdec.c 的 rkmpp_export_frame 将 MppFrame 和 DRM 描述分别封装为只读 AVBuffer，释放回调调用 mpp_frame_deinit，未在此路径生成独立像素副本。它与 CUDA safe-copy 的独立输出合同不同，不能在 adapter 中仅按设备名返回同一能力。当前合屏 source preparer 的 CUDA-only allocation/readiness 门禁可能早于网络几何失败；以首次实际CLI错误为准。尚未修改依赖或放宽门禁。
