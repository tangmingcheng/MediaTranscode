# RKMPP 四源合屏库交付记录（开发验证版）

已在RKMPP目标机生成库包，并取回本机。不是完整生产门禁通过，不安装或替换外部依赖。

## 产物与来源

- 包：`out/delivery/media-transcode-composition-rkmpp-21fc6bb8-20261118.tar.gz`，33550915字节。
- SHA256：`e89d9295a3aa410084fb3be25bef5ddc04ae948eb90a9d39189f1c5a2a6bd5df`。
- RKMPP路径：`/home/tang/MediaTranscode-composition-09093919/out/delivery/media-transcode-composition-rkmpp-21fc6bb8-20261118.tar.gz`。
- 实现源码21fc6bb8；示例源码6ed50e3f；中文说明更新14de18c3。本机工作分支codex/realtime-composition-progress。
- 包内13个文件的SHA256SUMS在RKMPP及本机解压后均逐项通过；同时核对完整压缩包SHA256一致。
- 头文件SHA256：`cb1007049e518513a209e0fe2729b1fddde156f757e419229dffab55cea6aa75`。
- 合并库SHA256：`03eca4357bc7bc4ef974bfd48ea76c4469786cd1daeec73d64aeebeb9afa7217`。
- 示例可执行文件SHA256：`1dceb525c7241fff092d132e5a8e289403c7dd8bbb6242b59ee3b52ce8f822bc`。

## 使用入口

`include/media_transcode_beta/realtime.h`与`lib/libmedia_transcode_beta.a`配套使用；internal原始archive仅用于追溯，不重复链接。包附C示例源码/可执行文件、实时CLI、许可证、README、VERSION和VALIDATION。

[调用示例](../examples/rkmpp_composition.c)直接调用mt_beta_realtime_start_composition，并演示事件回调、snapshot、request_stop与release；[逐字段中文参数说明](../examples/rkmpp_composition.md)列明每源RTP/RTCP事实、所选音源、等待期限、准备存储、画布、编码目标和部署出口预算。输入是实际本机单播地址，不接受0.0.0.0；其他数值是可编辑示例配置，不是库默认值。

README给出C11编译、C++链接和实际依赖路径。该包是aarch64静态业务库，仍依赖FFmpeg/MPP/RGA/设备与C++运行时；示例无RPATH/RUNPATH。ldd及rk512实际/proc/PID/maps已确认匹配运行库。

## 实际验证

- [rk510](realtime-composition-rk510-validation.md)：独立音频失源低电平、重入非静音、视频先结束后音频晚BYE、caller_stop exit0；双独立有限PASS。pressureFailures4仍保留。
- [rk511](realtime-composition-rk511-validation.md)：错误使用通配地址被拒绝，明确记录失败；未绕过校验。
- [rk512](realtime-composition-rk512-validation.md)：本包C示例四源完整120秒、四格/全黑、持续输出、stop/release exit0；限定PASS。
- [rk513](realtime-composition-rk513-validation.md)：相同真实四源链路，原始TS视频4549/音频6531包DTS回退0，音视频全解码无warning；相同AAC唯一匹配证明本轮录像启动DTS回退位于VLC接收录制/重封装路径。两名独立审查限定PASS，支持开发验证版库。
- 输出规格均为H264 VBR8–12Mbps、1280×720/30、AAC目标192kbps、MPEG-TS/RTP。实际CLI/FFmpeg/VLC命令、PID、结果和清理清单在各轮记录。
- Windows共享修复Release全量构建exit0；本轮未运行local CLI，未改本机FFmpeg。

未完成：内容级音画同步、长期内存/重复恢复、NIC出线保证、历史Windows发送期限问题、最新共享修复的Windows实流回归及完整矩阵。抓包仅覆盖rk513约151.6秒输出，不能将重复静音hash扩大成停止尾部验证。不得据此称全部合屏任务完成。

最终包经两名未参与实现的独立审查者复审均明确PASS（开发验证版），各自重算13项哈希一致；文档围栏和非静音措辞已修正后重新封包并核对上列最终hash。
