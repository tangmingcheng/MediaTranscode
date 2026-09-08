# run53 输入丢包恢复验证库交付

生产73ad5fbd、验证提交e923e4b8、标记rk-source14001-loss20-cbr6m-run53。

- 目标机目录：`/home/tang/packages/media-transcode-beta-rkmpp-73ad5fbd-run53-20260908`
- 压缩包：上述目录加`.tar.gz`，19106651 B，SHA-256 `4f53890e8d0650264043c50d7b80ac1cce945f1ff55f384d7954fba6bb8ecd26`。
- 静态库：`lib/libmedia_transcode_beta.a`，SHA-256 `c14882d4c319f89cb3f6b8a465740bdfcb2bb323fb998e032d3b6e729edd757f`，与此前73ad5fbd包逐字节一致。
- C示例：`examples/beta_minimal.c`及`bin/beta_minimal`，C11严格编译链接成功，未独立媒体验收。
- 使用用户指定`/home/tang/package_media_transcode_beta.sh`。19个包内文件全部SHA校验成功、动态依赖无缺失；库未下载本机，旧包保留。

README/RUN53/evidence明确本项真实源H.264→HEVC CBR20%输入丢包恢复PASS，以及源提前结束、末10.80秒缓存解码、剩余三项矩阵待覆盖和历史内核出口聚集的边界。详见[run53报告](2026-09-08-rk-a559-source14001-run53.md)。打包曾因SSH断开未执行，恢复连接后确认无残留产物才重新完成。