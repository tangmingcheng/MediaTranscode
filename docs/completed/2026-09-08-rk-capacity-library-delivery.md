# 2026-09-08 RKMPP 实际源容量修复库交付

按用户“先出一版库，然后再调查优化”指令交付生产提交 `73ad5fbd`。此包修复合法后续RTP超过启动样本峰值导致退出；run48持续转码和默认VLC硬解超过三分钟，完整无突发门禁仍FAIL，不标记完整验收成功。

- 目标机目录：`/home/tang/packages/media-transcode-beta-rkmpp-73ad5fbd-source14001-20260908`
- 压缩包：上述目录加 `.tar.gz`，19102541 B。
- 压缩包 SHA-256：`02c76a964be273bbf1b520a0137eff945d6800cf0aefd44e00da94fa0b5de00e`
- 合并静态库：`lib/libmedia_transcode_beta.a`；头文件：`include/media_transcode_beta/realtime.h`。
- C 使用示例：`examples/beta_minimal.c`，对应可执行文件 `bin/beta_minimal`。基于既有beta_minimal示例，C11严格编译和链接成功；没有宣称示例自身通过完整媒体验收。
- 使用用户指定 `/home/tang/package_media_transcode_beta.sh` 出包；CLI SHA与run48完全一致。20个包内文件SHA256SUMS校验全部成功，两个可执行文件的动态依赖无not found。未下载库至本机，旧库保留。
- 包内README、RUN48、VERSION明确局限，evidence含收发、输入、内核突发、包长和VLC GPU分析。详细真实命令与结果见 [run48](2026-09-08-rk-a559-source14001-run48.md)。

剩余工作：定位内核入队后聚集的具体阻塞/唤醒原因；64槽深乱序与输入丢包回归。此前1024槽版本矩阵不替代本版覆盖。PR保持Draft，源码容量修复的双独立PASS不扩大为整体验收PASS。