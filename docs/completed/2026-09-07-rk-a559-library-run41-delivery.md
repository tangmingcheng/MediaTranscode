# RKMPP 丢包恢复库与 C 示例交付

按用户指定，交付 run41 对应 `53c5c0d0c12eb674cce0b4c7f0134e122f545560`，不包含后加且尚未实测的 `2d597ba6` 背压修复。已推送标签 `rk-input-recovery-run41-53c5c0d0`；该标签表示本次库快照，不表示完整无突发门禁通过。

目标机重新构建成功，1,350 个源码及构建配置文件统一行尾后的 SHA-256 全部匹配该提交。CLI SHA-256 与 run41 实测二进制完全一致：`c00f9c26dd3b6fe0b05c525634ec190fca753f0f4e1189ceeead5e19f7416bea`。

实际执行用户指定脚本，exit 0：

```bash
/home/tang/package_media_transcode_beta.sh /home/tang/MediaTranscode/out/build/rk-release /home/tang/packages/media-transcode-beta-rkmpp-53c5c0d0-20260907
```

库目录：`/home/tang/packages/media-transcode-beta-rkmpp-53c5c0d0-20260907`。压缩包为同路径加 `.tar.gz`。按用户要求不下载到本机；SCP 在密码输入前取消，本机没有该压缩包。

压缩包大小 19,170,331 B，SHA-256：`d154b96613309d1ed48ab39f0ef755961fd3b85cd41f0901a90d584e45318b45`。

目录内 `lib/libmedia_transcode_beta.a` 为指定脚本合并的静态库，公开头文件为 `include/media_transcode_beta/realtime.h`；保留原始三个库、CLI、源码清单、依赖版本和校验和。`sha256sum -c SHA256SUMS` 全部成功。

## C 使用示例

包内 `examples/beta_minimal.c` 基于用户指定的 `/home/tang/mt-beta-package-rk-datagram-v3/examples/beta_minimal.c`，仓库副本为 [rkmpp_beta_minimal.c](../../examples/rkmpp_beta_minimal.c)。保留事件回调、每秒 snapshot 和会话 release；移除未启用的 80 秒停止代码，终止失败返回非零。输入地址 192.168.130.229:61884，输出 192.168.96.122:6200；H.264 → HEVC 1080p25 CBR 6 Mbps。参数是示例调用配置，库内未新增默认值或对外参数。

目标机 C11 严格编译及链接成功，exit 0；已提供 `bin/beta_minimal`。可直接运行：

```bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
cd /home/tang/packages/media-transcode-beta-rkmpp-53c5c0d0-20260907
./bin/beta_minimal
```

自行修改示例后，重新编译、链接：

```bash
gcc -std=c11 -Wall -Wextra -Werror -I./include -c examples/beta_minimal.c -o examples/beta_minimal.o
g++ examples/beta_minimal.o -L./lib -lmedia_transcode_beta $(pkg-config --libs libavfilter libavcodec libavformat libavutil libswscale libswresample) -pthread -o bin/beta_minimal
```

C 文件由 gcc 编译，最后由 g++ 链接 C++ 库。先启动播放器与示例，再按 [run41 报告](2026-09-07-rk-a559-input-recovery-run41.md) 启动源流；播放 URL 为 `rtp://@192.168.96.122:6200`。实际 FFmpeg、VLC、输入丢包及恢复命令均已附在库包 `RUN41.md`。源自然结束后等待原无输入窗口，不以主动终止示例代替源驱动结束。

## 验证边界与剩余项

run41 已验证同会话丢包恢复及 190.64 秒连续硬解，捕获范围内发送 RTP 全部匹配接收端；内核队列突发仍导致完整门禁 FAIL。C 示例仅验证编译与链接，未另外运行完整媒体链路，不能冒称它已完成 run41 验收。该库还保留审查发现的背压尾部所有权问题；修复版本必须另行实测后交付。两个独立审查者因额度限制中断，没有最终双 PASS；完整矩阵回归与最终审查继续保留。PR #32 保持 Draft。
