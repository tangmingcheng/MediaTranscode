# RKMPP 增量构建：236b73d5

状态：增量构建成功，realtime CLI 加载检查成功；不是媒体链路验收。日期：2026-11-05。

远端目录 `/home/tang/MediaTranscode-composition-09093919`，复用 `out/build/rk-release`。同步前逐项校验 1,925 个源码和构建文件，均与 `6d983c7c` 的 LF 规范化 SHA256 一致，新增路径无冲突；仅更新 36 个变化文件。同步后 1,937 个文件均与 `236b73d5c76cbcbd66d25a7f592f41a6cf403158` 一致。未修改外部 FFmpeg、MPP 或 RGA。

第一次执行只启用了 mtenv，CMake 因 PATH 指向 `/usr/bin/ffmpeg` 而重新配置失败，exit 1；缺少匹配系统旧 libavcodec 58.54.100 等开发包。没有为此安装或修改依赖。随后启用既有 ffenv，确认运行时及 pkg-config 均选择 `/home/tang/ffmpeg-ab1e61a`，重新配置成功。

实际重试命令：

```bash
source /etc/profile.d/ffenv.sh && ffenv on && source /opt/mt-tools/mtenv.sh && mtenv on && command -v ffmpeg && cmake --build out/build/rk-release --target all --parallel 8
```

本轮不执行 clean、不删除构建目录；Ninja 增量计划 457 个任务。没有运行 local CLI 或 realtime 媒体测试。Windows 同提交已有 Release 构建通过，Windows SourcePresentation 实流仍因 solver 未接线而未通过；本轮只验证 GCC/RKMPP 编译兼容性，不越过 Windows→RKMPP 的媒体验收顺序。

本轮临时同步文件清单：本机 `out/rk-incremental-236b73d5.json`、`.tar`；远端同项目 `out/` 下同名文件。构建结果记录后按清单删除，保留源码、构建缓存和用户指定媒体源。

## 结果

增量构建完成457/457，退出码0，原缓存保留。编译仍有AVCodec::sample_fmts废弃告警，未当作错误或屏蔽。实际执行 /home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --help，退出码0；ldd确认六个FFmpeg动态库均来自/home/tang/ffmpeg-ab1e61a/lib，没有not found。help启动时出现原样诊断 mpp[422494]: mpp_platform: client 12 driver is not ready!；未启动解码，不据此推断所有RKMPP功能不可用或已验证硬件链路。

realtime产物12,233,456字节，mtime 2026-11-05 11:51:41 +0800，SHA256 f935df67f65b44d2f52f14f003cd0f5bc9667e41ab645e3973112e0e9d3717e3。pgrep未发现cmake --build、ninja、cc1plus或realtime CLI残留。未执行local CLI，未产生抓包、截图或临时媒体。

构建后再次验证全部1,937文件：哈希无差异、源码/构建区域无额外文件。以上结果记录后，本机及远端各2个同步临时文件已按清单删除，均核对剩余0；源码、缓存与指定120秒媒体源保留。
