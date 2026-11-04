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

## 构建结果

8核全量构建完成，792/792，RK386_ALL_CORES_EXIT=0。realtime CLI 为12125384字节，core archive32700112字节、realtime application1813986字节、Beta304034字节，远端时间2026-11-04 14:43附近。未运行local CLI。

ldd 核验 FFmpeg 来自 /home/tang/ffmpeg-ab1e61a/lib，RGA 为 /usr/local/lib/aarch64-linux-gnu/librga.so.2，MPP 为 /usr/local/lib/librockchip_mpp.so.1；未出现not found。ffmpeg枚举确认 hevc_rkmpp、AAC、scale_rkrga、vpp_rkrga 可见；枚举不替代生产链路功能验收。

构建所需临时包已完成使用，按上表四个确切本机/远端文件路径清理；源码目录和构建产物保留供后续实流。媒体验收仍待进行，不能标记本轮合屏PASS。

## 实流结果：FAIL

CLI PID2289190 在预检退出1：Composition source lacks authoritative decoder readiness and direct transfer facts。h264_rkmpp 已打开，首帧路径已进入；MPP client 12 driver is not ready 警告一并保留，不将它替换为首个CLI错误。未产生合屏输出，未取得VLC画面，A/V漂移不适用。监控仅采到启动时CPU0%、RSS12472KiB，不能代表稳态。

三源PID2289187/2289188/2289189在失败确认后核对exe并发送SIGINT，约45.53秒停止、退出255；没有完成120秒验收。CLI自然失败退出，不是清理退出。监控PID2289196随CLI退出结束；VLC PID36260核对RC监听后quit，已不存在。未运行重入。

实际远端执行：bash /home/tang/MediaTranscode-composition-09093919/acceptance/rk386/run.sh；完整脚本及原始日志如下，后续按清单删除临时文件。

### cli-error.log（311字节）

```
mpp[2289190]: mpp_platform: client 12 driver is not ready!
mpp[2289190]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[CLI] realtime video graph preflight failed: Unsupported: Composition source lacks authoritative decoder readiness and direct transfer facts

```

### cli.log（3362字节）

```
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-04 14:46:17.385] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 14:46:17.388] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 14:46:17.389] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 14:46:17.393] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-04 14:46:17.396] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-04 14:46:17.397] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-04 14:46:17.403] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264_rkmpp planned=h264_rkmpp hardware=true hwaccel=rkmpp hw_pix_fmt=drm_prime hw_device_ctx=none hw_frames_contract=internal pkt_tb=1/90000
[2026-11-04 14:46:17.404] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264_rkmpp hwaccel_flags=1
[2026-11-04 14:46:17.404] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-rk386.source.0 decoder=h264_rkmpp
[2026-11-04 14:46:17.415] [info] [graph][planner.capability] composition_source_candidate_rejected source=0 candidate=rkmpp error=Unsupported: Composition source lacks authoritative decoder readiness and direct transfer facts
[2026-11-04 14:46:17.415] [info] [graph][planner.capability] composition_output_candidate_rejected encoder=hevc_rkmpp error=Unsupported: Composition source lacks authoritative decoder readiness and direct transfer facts
[2026-11-04 14:46:17.415] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=linux_receive_multiple_messages socket_capacity_bytes=4194304 batch_capacity_bytes=4192448 descriptor_capacity=64 maximum_datagram_bytes=65507 receive_calls=19 receive_syscalls=18 completed_datagrams=325 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 14:46:17.416] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=linux_receive_multiple_messages socket_capacity_bytes=4194304 batch_capacity_bytes=4192448 descriptor_capacity=64 maximum_datagram_bytes=65507 receive_calls=8 receive_syscalls=7 completed_datagrams=201 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 14:46:17.416] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=linux_receive_multiple_messages socket_capacity_bytes=4194304 batch_capacity_bytes=4192448 descriptor_capacity=64 maximum_datagram_bytes=65507 receive_calls=2 receive_syscalls=1 completed_datagrams=1 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 14:46:17.416] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=linux_receive_multiple_messages socket_capacity_bytes=4194304 batch_capacity_bytes=4192448 descriptor_capacity=64 maximum_datagram_bytes=65507 receive_calls=4 receive_syscalls=3 completed_datagrams=188 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none

```

### monitor.log（70字节）

```
2026-11-04T14:46:17,298442920+08:00
2289190       0  0.0 12472 374732

```

### pids.txt（76字节）

```
SOURCE0=2289187 SOURCE1=2289188 SOURCE2=2289189 CLI=2289190
MONITOR=2289196

```

### result.txt（62字节）

```
CLI_EXIT=1
SOURCE0_EXIT=255
SOURCE1_EXIT=255
SOURCE2_EXIT=255

```

### run.sh（3325字节）

```
#!/usr/bin/env bash
set -u
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk386
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk386 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
printf 'SOURCE0=%s SOURCE1=%s SOURCE2=%s CLI=%s\n' "$s0" "$s1" "$s2" "$cli" | tee pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" | tee -a pids.txt
wait "$cli"; code=$?
printf 'CLI_EXIT=%s\n' "$code" | tee result.txt
wait "$mon"
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" | tee -a result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" | tee -a result.txt
wait "$s2"; printf 'SOURCE2_EXIT=%s\n' "$?" | tee -a result.txt
printf 'RK386_RUN_FINISHED=1\n'

```

### source0.log（12195字节）

```
mpp[2289187]: mpp_platform: client 12 driver is not ready!
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from '/home/tang/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.12.102
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
frame=   30 fps=0.0 q=-1.0 size=    1080KiB time=00:00:01.00 bitrate=8849.0kbits/s speed=   2x elapsed=0:00:00.50    frame=   46 fps= 46 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.53x elapsed=0:00:01.00    frame=   61 fps= 41 q=-1.0 size=    2521KiB time=00:00:02.03 bitrate=10157.2kbits/s speed=1.35x elapsed=0:00:01.50    frame=   76 fps= 38 q=-1.0 size=    2882KiB time=00:00:02.53 bitrate=9318.7kbits/s speed=1.27x elapsed=0:00:02.00    frame=   91 fps= 36 q=-1.0 size=    3314KiB time=00:00:03.03 bitrate=8950.2kbits/s speed=1.21x elapsed=0:00:02.50    frame=  106 fps= 35 q=-1.0 size=    3790KiB time=00:00:03.53 bitrate=8786.8kbits/s speed=1.18x elapsed=0:00:03.00    frame=  121 fps= 35 q=-1.0 size=    4197KiB time=00:00:04.03 bitrate=8525.4kbits/s speed=1.15x elapsed=0:00:03.50    frame=  136 fps= 34 q=-1.0 size=    4674KiB time=00:00:04.53 bitrate=8445.4kbits/s speed=1.13x elapsed=0:00:04.00    frame=  151 fps= 34 q=-1.0 size=    5276KiB time=00:00:05.03 bitrate=8587.1kbits/s speed=1.12x elapsed=0:00:04.50    frame=  166 fps= 33 q=-1.0 size=    5819KiB time=00:00:05.53 bitrate=8615.1kbits/s speed=1.11x elapsed=0:00:05.00    frame=  181 fps= 33 q=-1.0 size=    6327KiB time=00:00:06.03 bitrate=8590.6kbits/s speed= 1.1x elapsed=0:00:05.50    frame=  196 fps= 33 q=-1.0 size=    6784KiB time=00:00:06.53 bitrate=8505.9kbits/s speed=1.09x elapsed=0:00:06.00    frame=  211 fps= 32 q=-1.0 size=    7103KiB time=00:00:07.03 bitrate=8273.6kbits/s speed=1.08x elapsed=0:00:06.50    frame=  226 fps= 32 q=-1.0 size=    7652KiB time=00:00:07.53 bitrate=8321.5kbits/s speed=1.08x elapsed=0:00:07.00    frame=  241 fps= 32 q=-1.0 size=    8060KiB time=00:00:08.03 bitrate=8219.7kbits/s speed=1.07x elapsed=0:00:07.50    frame=  256 fps= 32 q=-1.0 size=    8692KiB time=00:00:08.53 bitrate=8344.4kbits/s speed=1.07x elapsed=0:00:08.00    frame=  271 fps= 32 q=-1.0 size=    8998KiB time=00:00:09.03 bitrate=8159.8kbits/s speed=1.06x elapsed=0:00:08.50    frame=  286 fps= 32 q=-1.0 size=   10113KiB time=00:00:09.53 bitrate=8689.9kbits/s speed=1.06x elapsed=0:00:09.00    frame=  301 fps= 32 q=-1.0 size=   10609KiB time=00:00:10.03 bitrate=8661.8kbits/s speed=1.06x elapsed=0:00:09.50    frame=  316 fps= 32 q=-1.0 size=   10928KiB time=00:00:10.53 bitrate=8499.3kbits/s speed=1.05x elapsed=0:00:10.00    frame=  331 fps= 32 q=-1.0 size=   11174KiB time=00:00:11.03 bitrate=8296.7kbits/s speed=1.05x elapsed=0:00:10.50    frame=  346 fps= 31 q=-1.0 size=   11763KiB time=00:00:11.53 bitrate=8355.5kbits/s speed=1.05x elapsed=0:00:11.00    frame=  361 fps= 31 q=-1.0 size=   12212KiB time=00:00:12.03 bitrate=8313.3kbits/s speed=1.05x elapsed=0:00:11.50    frame=  376 fps= 31 q=-1.0 size=   12648KiB time=00:00:12.53 bitrate=8266.7kbits/s speed=1.04x elapsed=0:00:12.00    frame=  391 fps= 31 q=-1.0 size=   13021KiB time=00:00:13.03 bitrate=8184.5kbits/s speed=1.04x elapsed=0:00:12.50    frame=  406 fps= 31 q=-1.0 size=   13766KiB time=00:00:13.53 bitrate=8332.6kbits/s speed=1.04x elapsed=0:00:13.00    frame=  421 fps= 31 q=-1.0 size=   14251KiB time=00:00:14.03 bitrate=8319.2kbits/s speed=1.04x elapsed=0:00:13.50    frame=  436 fps= 31 q=-1.0 size=   14840KiB time=00:00:14.53 bitrate=8364.6kbits/s speed=1.04x elapsed=0:00:14.00    frame=  451 fps= 31 q=-1.0 size=   15108KiB time=00:00:15.03 bitrate=8232.6kbits/s speed=1.04x elapsed=0:00:14.50    frame=  466 fps= 31 q=-1.0 size=   15625KiB time=00:00:15.53 bitrate=8240.2kbits/s speed=1.04x elapsed=0:00:15.00    frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.50    frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.00    frame=  511 fps= 31 q=-1.0 size=   16979KiB time=00:00:17.03 bitrate=8165.7kbits/s speed=1.03x elapsed=0:00:16.50    frame=  526 fps= 31 q=-1.0 size=   17773KiB time=00:00:17.53 bitrate=8303.8kbits/s speed=1.03x elapsed=0:00:17.00    frame=  541 fps= 31 q=-1.0 size=   18599KiB time=00:00:18.03 bitrate=8449.0kbits/s speed=1.03x elapsed=0:00:17.50    frame=  556 fps= 31 q=-1.0 size=   18883KiB time=00:00:18.53 bitrate=8346.6kbits/s speed=1.03x elapsed=0:00:18.00    frame=  571 fps= 31 q=-1.0 size=   19171KiB time=00:00:19.03 bitrate=8251.3kbits/s speed=1.03x elapsed=0:00:18.50    frame=  586 fps= 31 q=-1.0 size=   19670KiB time=00:00:19.53 bitrate=8249.1kbits/s speed=1.03x elapsed=0:00:19.00    frame=  601 fps= 31 q=-1.0 size=   20069KiB time=00:00:20.03 bitrate=8206.5kbits/s speed=1.03x elapsed=0:00:19.50    frame=  616 fps= 31 q=-1.0 size=   20616KiB time=00:00:20.53 bitrate=8225.0kbits/s speed=1.03x elapsed=0:00:20.00    frame=  631 fps= 31 q=-1.0 size=   20977KiB time=00:00:21.03 bitrate=8170.0kbits/s speed=1.03x elapsed=0:00:20.50    frame=  646 fps= 31 q=-1.0 size=   21740KiB time=00:00:21.53 bitrate=8270.7kbits/s speed=1.02x elapsed=0:00:21.00    frame=  661 fps= 31 q=-1.0 size=   22203KiB time=00:00:22.03 bitrate=8255.0kbits/s speed=1.02x elapsed=0:00:21.50    frame=  676 fps= 31 q=-1.0 size=   22732KiB time=00:00:22.53 bitrate=8264.2kbits/s speed=1.02x elapsed=0:00:22.00    frame=  691 fps= 31 q=-1.0 size=   23131KiB time=00:00:23.03 bitrate=8226.8kbits/s speed=1.02x elapsed=0:00:22.51    frame=  706 fps= 31 q=-1.0 size=   23510KiB time=00:00:23.53 bitrate=8183.8kbits/s speed=1.02x elapsed=0:00:23.01    frame=  721 fps= 31 q=-1.0 size=   23930KiB time=00:00:24.03 bitrate=8156.9kbits/s speed=1.02x elapsed=0:00:23.51    frame=  736 fps= 31 q=-1.0 size=   24514KiB time=00:00:24.53 bitrate=8185.4kbits/s speed=1.02x elapsed=0:00:24.01    frame=  751 fps= 31 q=-1.0 size=   24967KiB time=00:00:25.03 bitrate=8170.2kbits/s speed=1.02x elapsed=0:00:24.51    frame=  766 fps= 31 q=-1.0 size=   25310KiB time=00:00:25.53 bitrate=8120.4kbits/s speed=1.02x elapsed=0:00:25.01    frame=  781 fps= 31 q=-1.0 size=   26113KiB time=00:00:26.03 bitrate=8217.1kbits/s speed=1.02x elapsed=0:00:25.51    frame=  796 fps= 31 q=-1.0 size=   26825KiB time=00:00:26.53 bitrate=8282.0kbits/s speed=1.02x elapsed=0:00:26.01    frame=  811 fps= 31 q=-1.0 size=   27110KiB time=00:00:27.03 bitrate=8215.2kbits/s speed=1.02x elapsed=0:00:26.51    frame=  826 fps= 31 q=-1.0 size=   27455KiB time=00:00:27.53 bitrate=8168.6kbits/s speed=1.02x elapsed=0:00:27.01    frame=  841 fps= 31 q=-1.0 size=   27989KiB time=00:00:28.03 bitrate=8178.9kbits/s speed=1.02x elapsed=0:00:27.51    frame=  856 fps= 31 q=-1.0 size=   28560KiB time=00:00:28.53 bitrate=8199.6kbits/s speed=1.02x elapsed=0:00:28.01    frame=  871 fps= 31 q=-1.0 size=   28903KiB time=00:00:29.03 bitrate=8155.2kbits/s speed=1.02x elapsed=0:00:28.51    frame=  886 fps= 31 q=-1.0 size=   29392KiB time=00:00:29.53 bitrate=8152.9kbits/s speed=1.02x elapsed=0:00:29.01    frame=  901 fps= 31 q=-1.0 size=   30007KiB time=00:00:30.03 bitrate=8185.0kbits/s speed=1.02x elapsed=0:00:29.51    frame=  916 fps= 31 q=-1.0 size=   30602KiB time=00:00:30.53 bitrate=8210.4kbits/s speed=1.02x elapsed=0:00:30.01    frame=  931 fps= 31 q=-1.0 size=   31119KiB time=00:00:31.03 bitrate=8214.7kbits/s speed=1.02x elapsed=0:00:30.51    frame=  946 fps= 31 q=-1.0 size=   31450KiB time=00:00:31.53 bitrate=8170.5kbits/s speed=1.02x elapsed=0:00:31.01    frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.51    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.01    frame=  991 fps= 30 q=-1.0 size=   32797KiB time=00:00:33.03 bitrate=8133.3kbits/s speed=1.02x elapsed=0:00:32.51    frame= 1006 fps= 30 q=-1.0 size=   33295KiB time=00:00:33.53 bitrate=8133.9kbits/s speed=1.02x elapsed=0:00:33.01    frame= 1021 fps= 30 q=-1.0 size=   33723KiB time=00:00:34.03 bitrate=8117.3kbits/s speed=1.02x elapsed=0:00:33.51    frame= 1036 fps= 30 q=-1.0 size=   34879KiB time=00:00:34.53 bitrate=8273.9kbits/s speed=1.02x elapsed=0:00:34.01    frame= 1051 fps= 30 q=-1.0 size=   35211KiB time=00:00:35.03 bitrate=8233.7kbits/s speed=1.02x elapsed=0:00:34.51    frame= 1066 fps= 30 q=-1.0 size=   35610KiB time=00:00:35.53 bitrate=8209.7kbits/s speed=1.01x elapsed=0:00:35.01    frame= 1081 fps= 30 q=-1.0 size=   35998KiB time=00:00:36.03 bitrate=8184.0kbits/s speed=1.01x elapsed=0:00:35.51    frame= 1096 fps= 30 q=-1.0 size=   36471KiB time=00:00:36.53 bitrate=8178.0kbits/s speed=1.01x elapsed=0:00:36.01    frame= 1111 fps= 30 q=-1.0 size=   36872KiB time=00:00:37.03 bitrate=8156.3kbits/s speed=1.01x elapsed=0:00:36.51    frame= 1126 fps= 30 q=-1.0 size=   37333KiB time=00:00:37.53 bitrate=8148.2kbits/s speed=1.01x elapsed=0:00:37.01    frame= 1141 fps= 30 q=-1.0 size=   37915KiB time=00:00:38.03 bitrate=8166.4kbits/s speed=1.01x elapsed=0:00:37.51    frame= 1156 fps= 30 q=-1.0 size=   38464KiB time=00:00:38.53 bitrate=8177.2kbits/s speed=1.01x elapsed=0:00:38.01    frame= 1171 fps= 30 q=-1.0 size=   38978KiB time=00:00:39.03 bitrate=8180.4kbits/s speed=1.01x elapsed=0:00:38.51    frame= 1186 fps= 30 q=-1.0 size=   39477KiB time=00:00:39.53 bitrate=8180.2kbits/s speed=1.01x elapsed=0:00:39.01    frame= 1201 fps= 30 q=-1.0 size=   39769KiB time=00:00:40.03 bitrate=8138.0kbits/s speed=1.01x elapsed=0:00:39.51    frame= 1216 fps= 30 q=-1.0 size=   40289KiB time=00:00:40.53 bitrate=8142.6kbits/s speed=1.01x elapsed=0:00:40.01    frame= 1231 fps= 30 q=-1.0 size=   40704KiB time=00:00:41.03 bitrate=8126.3kbits/s speed=1.01x elapsed=0:00:40.51    frame= 1246 fps= 30 q=-1.0 size=   41305KiB time=00:00:41.53 bitrate=8147.0kbits/s speed=1.01x elapsed=0:00:41.01    frame= 1261 fps= 30 q=-1.0 size=   41634KiB time=00:00:42.03 bitrate=8114.1kbits/s speed=1.01x elapsed=0:00:41.51    frame= 1276 fps= 30 q=-1.0 size=   42713KiB time=00:00:42.53 bitrate=8226.6kbits/s speed=1.01x elapsed=0:00:42.01    frame= 1291 fps= 30 q=-1.0 size=   43264KiB time=00:00:43.03 bitrate=8235.9kbits/s speed=1.01x elapsed=0:00:42.51    frame= 1306 fps= 30 q=-1.0 size=   43568KiB time=00:00:43.53 bitrate=8198.6kbits/s speed=1.01x elapsed=0:00:43.01    frame= 1321 fps= 30 q=-1.0 size=   43806KiB time=00:00:44.03 bitrate=8149.6kbits/s speed=1.01x elapsed=0:00:43.52    frame= 1336 fps= 30 q=-1.0 size=   44370KiB time=00:00:44.53 bitrate=8162.0kbits/s speed=1.01x elapsed=0:00:44.02    frame= 1351 fps= 30 q=-1.0 size=   44813KiB time=00:00:45.03 bitrate=8151.9kbits/s speed=1.01x elapsed=0:00:44.52    [out#0/rtp @ 0x1d2e8740] video:44747KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.213062%
frame= 1366 fps= 30 q=-1.0 Lsize=   45290KiB time=00:00:45.53 bitrate=8148.2kbits/s speed=1.01x elapsed=0:00:45.03    
Exiting normally, received signal 2.

```

### source0.out（322字节）

```
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.12.102
m=video 61030 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


```

### source1.log（12195字节）

```
mpp[2289188]: mpp_platform: client 12 driver is not ready!
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from '/home/tang/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.12.102
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
frame=   30 fps=0.0 q=-1.0 size=    1080KiB time=00:00:01.00 bitrate=8849.0kbits/s speed=   2x elapsed=0:00:00.50    frame=   46 fps= 46 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.53x elapsed=0:00:01.00    frame=   61 fps= 41 q=-1.0 size=    2521KiB time=00:00:02.03 bitrate=10157.2kbits/s speed=1.35x elapsed=0:00:01.50    frame=   76 fps= 38 q=-1.0 size=    2882KiB time=00:00:02.53 bitrate=9318.7kbits/s speed=1.26x elapsed=0:00:02.00    frame=   91 fps= 36 q=-1.0 size=    3314KiB time=00:00:03.03 bitrate=8950.2kbits/s speed=1.21x elapsed=0:00:02.50    frame=  106 fps= 35 q=-1.0 size=    3790KiB time=00:00:03.53 bitrate=8786.8kbits/s speed=1.18x elapsed=0:00:03.00    frame=  121 fps= 35 q=-1.0 size=    4197KiB time=00:00:04.03 bitrate=8525.4kbits/s speed=1.15x elapsed=0:00:03.50    frame=  136 fps= 34 q=-1.0 size=    4674KiB time=00:00:04.53 bitrate=8445.4kbits/s speed=1.13x elapsed=0:00:04.00    frame=  151 fps= 34 q=-1.0 size=    5276KiB time=00:00:05.03 bitrate=8587.1kbits/s speed=1.12x elapsed=0:00:04.50    frame=  166 fps= 33 q=-1.0 size=    5819KiB time=00:00:05.53 bitrate=8615.1kbits/s speed=1.11x elapsed=0:00:05.00    frame=  181 fps= 33 q=-1.0 size=    6327KiB time=00:00:06.03 bitrate=8590.6kbits/s speed= 1.1x elapsed=0:00:05.50    frame=  196 fps= 33 q=-1.0 size=    6784KiB time=00:00:06.53 bitrate=8505.9kbits/s speed=1.09x elapsed=0:00:06.00    frame=  211 fps= 32 q=-1.0 size=    7103KiB time=00:00:07.03 bitrate=8273.6kbits/s speed=1.08x elapsed=0:00:06.50    frame=  226 fps= 32 q=-1.0 size=    7652KiB time=00:00:07.53 bitrate=8321.5kbits/s speed=1.08x elapsed=0:00:07.00    frame=  241 fps= 32 q=-1.0 size=    8060KiB time=00:00:08.03 bitrate=8219.7kbits/s speed=1.07x elapsed=0:00:07.50    frame=  256 fps= 32 q=-1.0 size=    8692KiB time=00:00:08.53 bitrate=8344.4kbits/s speed=1.07x elapsed=0:00:08.00    frame=  271 fps= 32 q=-1.0 size=    8998KiB time=00:00:09.03 bitrate=8159.8kbits/s speed=1.06x elapsed=0:00:08.50    frame=  286 fps= 32 q=-1.0 size=   10113KiB time=00:00:09.53 bitrate=8689.9kbits/s speed=1.06x elapsed=0:00:09.00    frame=  301 fps= 32 q=-1.0 size=   10609KiB time=00:00:10.03 bitrate=8661.8kbits/s speed=1.06x elapsed=0:00:09.50    frame=  316 fps= 32 q=-1.0 size=   10928KiB time=00:00:10.53 bitrate=8499.3kbits/s speed=1.05x elapsed=0:00:10.00    frame=  331 fps= 32 q=-1.0 size=   11174KiB time=00:00:11.03 bitrate=8296.7kbits/s speed=1.05x elapsed=0:00:10.50    frame=  346 fps= 31 q=-1.0 size=   11763KiB time=00:00:11.53 bitrate=8355.5kbits/s speed=1.05x elapsed=0:00:11.00    frame=  361 fps= 31 q=-1.0 size=   12212KiB time=00:00:12.03 bitrate=8313.3kbits/s speed=1.05x elapsed=0:00:11.50    frame=  376 fps= 31 q=-1.0 size=   12648KiB time=00:00:12.53 bitrate=8266.7kbits/s speed=1.04x elapsed=0:00:12.00    frame=  391 fps= 31 q=-1.0 size=   13021KiB time=00:00:13.03 bitrate=8184.5kbits/s speed=1.04x elapsed=0:00:12.50    frame=  406 fps= 31 q=-1.0 size=   13766KiB time=00:00:13.53 bitrate=8332.6kbits/s speed=1.04x elapsed=0:00:13.00    frame=  421 fps= 31 q=-1.0 size=   14251KiB time=00:00:14.03 bitrate=8319.2kbits/s speed=1.04x elapsed=0:00:13.50    frame=  436 fps= 31 q=-1.0 size=   14840KiB time=00:00:14.53 bitrate=8364.6kbits/s speed=1.04x elapsed=0:00:14.00    frame=  451 fps= 31 q=-1.0 size=   15108KiB time=00:00:15.03 bitrate=8232.6kbits/s speed=1.04x elapsed=0:00:14.50    frame=  466 fps= 31 q=-1.0 size=   15625KiB time=00:00:15.53 bitrate=8240.2kbits/s speed=1.03x elapsed=0:00:15.00    frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.50    frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.00    frame=  511 fps= 31 q=-1.0 size=   16979KiB time=00:00:17.03 bitrate=8165.7kbits/s speed=1.03x elapsed=0:00:16.50    frame=  526 fps= 31 q=-1.0 size=   17773KiB time=00:00:17.53 bitrate=8303.8kbits/s speed=1.03x elapsed=0:00:17.00    frame=  541 fps= 31 q=-1.0 size=   18599KiB time=00:00:18.03 bitrate=8449.0kbits/s speed=1.03x elapsed=0:00:17.51    frame=  556 fps= 31 q=-1.0 size=   18883KiB time=00:00:18.53 bitrate=8346.6kbits/s speed=1.03x elapsed=0:00:18.01    frame=  571 fps= 31 q=-1.0 size=   19171KiB time=00:00:19.03 bitrate=8251.3kbits/s speed=1.03x elapsed=0:00:18.51    frame=  586 fps= 31 q=-1.0 size=   19670KiB time=00:00:19.53 bitrate=8249.1kbits/s speed=1.03x elapsed=0:00:19.01    frame=  601 fps= 31 q=-1.0 size=   20069KiB time=00:00:20.03 bitrate=8206.5kbits/s speed=1.03x elapsed=0:00:19.51    frame=  616 fps= 31 q=-1.0 size=   20616KiB time=00:00:20.53 bitrate=8225.0kbits/s speed=1.03x elapsed=0:00:20.01    frame=  631 fps= 31 q=-1.0 size=   20977KiB time=00:00:21.03 bitrate=8170.0kbits/s speed=1.03x elapsed=0:00:20.51    frame=  646 fps= 31 q=-1.0 size=   21740KiB time=00:00:21.53 bitrate=8270.7kbits/s speed=1.02x elapsed=0:00:21.01    frame=  661 fps= 31 q=-1.0 size=   22203KiB time=00:00:22.03 bitrate=8255.0kbits/s speed=1.02x elapsed=0:00:21.51    frame=  676 fps= 31 q=-1.0 size=   22732KiB time=00:00:22.53 bitrate=8264.2kbits/s speed=1.02x elapsed=0:00:22.01    frame=  691 fps= 31 q=-1.0 size=   23131KiB time=00:00:23.03 bitrate=8226.8kbits/s speed=1.02x elapsed=0:00:22.51    frame=  706 fps= 31 q=-1.0 size=   23510KiB time=00:00:23.53 bitrate=8183.8kbits/s speed=1.02x elapsed=0:00:23.01    frame=  721 fps= 31 q=-1.0 size=   23930KiB time=00:00:24.03 bitrate=8156.9kbits/s speed=1.02x elapsed=0:00:23.51    frame=  736 fps= 31 q=-1.0 size=   24514KiB time=00:00:24.53 bitrate=8185.4kbits/s speed=1.02x elapsed=0:00:24.01    frame=  751 fps= 31 q=-1.0 size=   24967KiB time=00:00:25.03 bitrate=8170.2kbits/s speed=1.02x elapsed=0:00:24.51    frame=  766 fps= 31 q=-1.0 size=   25310KiB time=00:00:25.53 bitrate=8120.4kbits/s speed=1.02x elapsed=0:00:25.01    frame=  781 fps= 31 q=-1.0 size=   26113KiB time=00:00:26.03 bitrate=8217.1kbits/s speed=1.02x elapsed=0:00:25.51    frame=  796 fps= 31 q=-1.0 size=   26825KiB time=00:00:26.53 bitrate=8282.0kbits/s speed=1.02x elapsed=0:00:26.01    frame=  811 fps= 31 q=-1.0 size=   27110KiB time=00:00:27.03 bitrate=8215.2kbits/s speed=1.02x elapsed=0:00:26.51    frame=  826 fps= 31 q=-1.0 size=   27455KiB time=00:00:27.53 bitrate=8168.6kbits/s speed=1.02x elapsed=0:00:27.01    frame=  841 fps= 31 q=-1.0 size=   27989KiB time=00:00:28.03 bitrate=8178.9kbits/s speed=1.02x elapsed=0:00:27.51    frame=  856 fps= 31 q=-1.0 size=   28560KiB time=00:00:28.53 bitrate=8199.6kbits/s speed=1.02x elapsed=0:00:28.01    frame=  871 fps= 31 q=-1.0 size=   28903KiB time=00:00:29.03 bitrate=8155.2kbits/s speed=1.02x elapsed=0:00:28.51    frame=  886 fps= 31 q=-1.0 size=   29392KiB time=00:00:29.53 bitrate=8152.9kbits/s speed=1.02x elapsed=0:00:29.01    frame=  901 fps= 31 q=-1.0 size=   30007KiB time=00:00:30.03 bitrate=8185.0kbits/s speed=1.02x elapsed=0:00:29.51    frame=  916 fps= 31 q=-1.0 size=   30602KiB time=00:00:30.53 bitrate=8210.4kbits/s speed=1.02x elapsed=0:00:30.01    frame=  931 fps= 31 q=-1.0 size=   31119KiB time=00:00:31.03 bitrate=8214.7kbits/s speed=1.02x elapsed=0:00:30.51    frame=  946 fps= 31 q=-1.0 size=   31450KiB time=00:00:31.53 bitrate=8170.5kbits/s speed=1.02x elapsed=0:00:31.01    frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.51    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.01    frame=  991 fps= 30 q=-1.0 size=   32797KiB time=00:00:33.03 bitrate=8133.3kbits/s speed=1.02x elapsed=0:00:32.51    frame= 1006 fps= 30 q=-1.0 size=   33295KiB time=00:00:33.53 bitrate=8133.9kbits/s speed=1.02x elapsed=0:00:33.01    frame= 1021 fps= 30 q=-1.0 size=   33723KiB time=00:00:34.03 bitrate=8117.3kbits/s speed=1.02x elapsed=0:00:33.51    frame= 1036 fps= 30 q=-1.0 size=   34879KiB time=00:00:34.53 bitrate=8273.9kbits/s speed=1.02x elapsed=0:00:34.01    frame= 1051 fps= 30 q=-1.0 size=   35211KiB time=00:00:35.03 bitrate=8233.7kbits/s speed=1.01x elapsed=0:00:34.51    frame= 1066 fps= 30 q=-1.0 size=   35610KiB time=00:00:35.53 bitrate=8209.7kbits/s speed=1.01x elapsed=0:00:35.01    frame= 1081 fps= 30 q=-1.0 size=   35998KiB time=00:00:36.03 bitrate=8184.0kbits/s speed=1.01x elapsed=0:00:35.51    frame= 1096 fps= 30 q=-1.0 size=   36471KiB time=00:00:36.53 bitrate=8178.0kbits/s speed=1.01x elapsed=0:00:36.01    frame= 1111 fps= 30 q=-1.0 size=   36872KiB time=00:00:37.03 bitrate=8156.3kbits/s speed=1.01x elapsed=0:00:36.51    frame= 1126 fps= 30 q=-1.0 size=   37333KiB time=00:00:37.53 bitrate=8148.2kbits/s speed=1.01x elapsed=0:00:37.01    frame= 1141 fps= 30 q=-1.0 size=   37915KiB time=00:00:38.03 bitrate=8166.4kbits/s speed=1.01x elapsed=0:00:37.51    frame= 1156 fps= 30 q=-1.0 size=   38464KiB time=00:00:38.53 bitrate=8177.2kbits/s speed=1.01x elapsed=0:00:38.01    frame= 1171 fps= 30 q=-1.0 size=   38978KiB time=00:00:39.03 bitrate=8180.4kbits/s speed=1.01x elapsed=0:00:38.52    frame= 1186 fps= 30 q=-1.0 size=   39477KiB time=00:00:39.53 bitrate=8180.2kbits/s speed=1.01x elapsed=0:00:39.02    frame= 1201 fps= 30 q=-1.0 size=   39769KiB time=00:00:40.03 bitrate=8138.0kbits/s speed=1.01x elapsed=0:00:39.52    frame= 1216 fps= 30 q=-1.0 size=   40289KiB time=00:00:40.53 bitrate=8142.6kbits/s speed=1.01x elapsed=0:00:40.02    frame= 1231 fps= 30 q=-1.0 size=   40704KiB time=00:00:41.03 bitrate=8126.3kbits/s speed=1.01x elapsed=0:00:40.52    frame= 1246 fps= 30 q=-1.0 size=   41305KiB time=00:00:41.53 bitrate=8147.0kbits/s speed=1.01x elapsed=0:00:41.02    frame= 1261 fps= 30 q=-1.0 size=   41634KiB time=00:00:42.03 bitrate=8114.1kbits/s speed=1.01x elapsed=0:00:41.52    frame= 1276 fps= 30 q=-1.0 size=   42713KiB time=00:00:42.53 bitrate=8226.6kbits/s speed=1.01x elapsed=0:00:42.02    frame= 1291 fps= 30 q=-1.0 size=   43264KiB time=00:00:43.03 bitrate=8235.9kbits/s speed=1.01x elapsed=0:00:42.52    frame= 1306 fps= 30 q=-1.0 size=   43568KiB time=00:00:43.53 bitrate=8198.6kbits/s speed=1.01x elapsed=0:00:43.02    frame= 1321 fps= 30 q=-1.0 size=   43806KiB time=00:00:44.03 bitrate=8149.6kbits/s speed=1.01x elapsed=0:00:43.52    frame= 1336 fps= 30 q=-1.0 size=   44370KiB time=00:00:44.53 bitrate=8162.0kbits/s speed=1.01x elapsed=0:00:44.02    frame= 1351 fps= 30 q=-1.0 size=   44813KiB time=00:00:45.03 bitrate=8151.9kbits/s speed=1.01x elapsed=0:00:44.52    [out#0/rtp @ 0x25b74740] video:44747KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.213062%
frame= 1366 fps= 30 q=-1.0 Lsize=   45290KiB time=00:00:45.53 bitrate=8148.2kbits/s speed=1.01x elapsed=0:00:45.03    
Exiting normally, received signal 2.

```

### source1.out（322字节）

```
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.12.102
m=video 61040 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


```

### source2.log（12772字节）

```
mpp[2289189]: mpp_platform: client 12 driver is not ready!
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from '/home/tang/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
  Stream #0:1 -> #1:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.12.102
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
Output #1, rtp, to 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.12.102
  Stream #1:0(eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.02 bitrate=9411.6kbits/s speed=2.04x elapsed=0:00:00.50    frame=   46 fps= 46 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.53x elapsed=0:00:01.00    frame=   61 fps= 41 q=-1.0 size=    2521KiB time=00:00:02.03 bitrate=10157.2kbits/s speed=1.35x elapsed=0:00:01.50    frame=   76 fps= 38 q=-1.0 size=    2882KiB time=00:00:02.53 bitrate=9318.7kbits/s speed=1.27x elapsed=0:00:02.00    frame=   91 fps= 36 q=-1.0 size=    3314KiB time=00:00:03.03 bitrate=8950.2kbits/s speed=1.21x elapsed=0:00:02.50    frame=  106 fps= 35 q=-1.0 size=    3790KiB time=00:00:03.53 bitrate=8786.8kbits/s speed=1.18x elapsed=0:00:03.00    frame=  121 fps= 35 q=-1.0 size=    4197KiB time=00:00:04.03 bitrate=8525.4kbits/s speed=1.15x elapsed=0:00:03.50    frame=  136 fps= 34 q=-1.0 size=    4674KiB time=00:00:04.53 bitrate=8445.4kbits/s speed=1.13x elapsed=0:00:04.00    frame=  151 fps= 34 q=-1.0 size=    5276KiB time=00:00:05.03 bitrate=8587.1kbits/s speed=1.12x elapsed=0:00:04.50    frame=  166 fps= 33 q=-1.0 size=    5819KiB time=00:00:05.53 bitrate=8615.1kbits/s speed=1.11x elapsed=0:00:05.00    frame=  181 fps= 33 q=-1.0 size=    6327KiB time=00:00:06.03 bitrate=8590.6kbits/s speed= 1.1x elapsed=0:00:05.50    frame=  196 fps= 33 q=-1.0 size=    6784KiB time=00:00:06.53 bitrate=8505.9kbits/s speed=1.09x elapsed=0:00:06.00    frame=  211 fps= 32 q=-1.0 size=    7103KiB time=00:00:07.03 bitrate=8273.6kbits/s speed=1.08x elapsed=0:00:06.50    frame=  226 fps= 32 q=-1.0 size=    7652KiB time=00:00:07.53 bitrate=8321.5kbits/s speed=1.08x elapsed=0:00:07.00    frame=  241 fps= 32 q=-1.0 size=    8060KiB time=00:00:08.03 bitrate=8219.7kbits/s speed=1.07x elapsed=0:00:07.50    frame=  256 fps= 32 q=-1.0 size=    8692KiB time=00:00:08.53 bitrate=8344.4kbits/s speed=1.07x elapsed=0:00:08.00    frame=  271 fps= 32 q=-1.0 size=    8998KiB time=00:00:09.03 bitrate=8159.8kbits/s speed=1.06x elapsed=0:00:08.50    frame=  286 fps= 32 q=-1.0 size=   10113KiB time=00:00:09.53 bitrate=8689.9kbits/s speed=1.06x elapsed=0:00:09.00    frame=  301 fps= 32 q=-1.0 size=   10609KiB time=00:00:10.03 bitrate=8661.8kbits/s speed=1.06x elapsed=0:00:09.50    frame=  316 fps= 32 q=-1.0 size=   10928KiB time=00:00:10.53 bitrate=8499.3kbits/s speed=1.05x elapsed=0:00:10.00    frame=  331 fps= 32 q=-1.0 size=   11174KiB time=00:00:11.03 bitrate=8296.7kbits/s speed=1.05x elapsed=0:00:10.50    frame=  346 fps= 31 q=-1.0 size=   11763KiB time=00:00:11.53 bitrate=8355.5kbits/s speed=1.05x elapsed=0:00:11.00    frame=  361 fps= 31 q=-1.0 size=   12212KiB time=00:00:12.03 bitrate=8313.3kbits/s speed=1.05x elapsed=0:00:11.50    frame=  376 fps= 31 q=-1.0 size=   12648KiB time=00:00:12.53 bitrate=8266.7kbits/s speed=1.04x elapsed=0:00:12.00    frame=  391 fps= 31 q=-1.0 size=   13021KiB time=00:00:13.03 bitrate=8184.5kbits/s speed=1.04x elapsed=0:00:12.50    frame=  406 fps= 31 q=-1.0 size=   13766KiB time=00:00:13.53 bitrate=8332.6kbits/s speed=1.04x elapsed=0:00:13.00    frame=  421 fps= 31 q=-1.0 size=   14251KiB time=00:00:14.03 bitrate=8319.2kbits/s speed=1.04x elapsed=0:00:13.50    frame=  436 fps= 31 q=-1.0 size=   14840KiB time=00:00:14.53 bitrate=8364.6kbits/s speed=1.04x elapsed=0:00:14.00    frame=  451 fps= 31 q=-1.0 size=   15108KiB time=00:00:15.03 bitrate=8232.6kbits/s speed=1.04x elapsed=0:00:14.50    frame=  466 fps= 31 q=-1.0 size=   15625KiB time=00:00:15.53 bitrate=8240.2kbits/s speed=1.03x elapsed=0:00:15.00    frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.50    frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.00    frame=  511 fps= 31 q=-1.0 size=   16979KiB time=00:00:17.03 bitrate=8165.7kbits/s speed=1.03x elapsed=0:00:16.50    frame=  526 fps= 31 q=-1.0 size=   17773KiB time=00:00:17.53 bitrate=8303.8kbits/s speed=1.03x elapsed=0:00:17.00    frame=  541 fps= 31 q=-1.0 size=   18599KiB time=00:00:18.03 bitrate=8449.0kbits/s speed=1.03x elapsed=0:00:17.50    frame=  556 fps= 31 q=-1.0 size=   18883KiB time=00:00:18.53 bitrate=8346.6kbits/s speed=1.03x elapsed=0:00:18.00    frame=  571 fps= 31 q=-1.0 size=   19171KiB time=00:00:19.03 bitrate=8251.3kbits/s speed=1.03x elapsed=0:00:18.50    frame=  586 fps= 31 q=-1.0 size=   19670KiB time=00:00:19.53 bitrate=8249.1kbits/s speed=1.03x elapsed=0:00:19.01    frame=  601 fps= 31 q=-1.0 size=   20069KiB time=00:00:20.03 bitrate=8206.5kbits/s speed=1.03x elapsed=0:00:19.51    frame=  616 fps= 31 q=-1.0 size=   20616KiB time=00:00:20.53 bitrate=8225.0kbits/s speed=1.03x elapsed=0:00:20.01    frame=  631 fps= 31 q=-1.0 size=   20977KiB time=00:00:21.03 bitrate=8170.0kbits/s speed=1.03x elapsed=0:00:20.51    frame=  646 fps= 31 q=-1.0 size=   21740KiB time=00:00:21.53 bitrate=8270.7kbits/s speed=1.02x elapsed=0:00:21.01    frame=  661 fps= 31 q=-1.0 size=   22203KiB time=00:00:22.03 bitrate=8255.0kbits/s speed=1.02x elapsed=0:00:21.51    frame=  676 fps= 31 q=-1.0 size=   22732KiB time=00:00:22.53 bitrate=8264.2kbits/s speed=1.02x elapsed=0:00:22.01    frame=  692 fps= 31 q=-1.0 size=   23239KiB time=00:00:23.03 bitrate=8264.7kbits/s speed=1.02x elapsed=0:00:22.51    frame=  706 fps= 31 q=-1.0 size=   23510KiB time=00:00:23.53 bitrate=8183.8kbits/s speed=1.02x elapsed=0:00:23.01    frame=  721 fps= 31 q=-1.0 size=   23930KiB time=00:00:24.03 bitrate=8156.9kbits/s speed=1.02x elapsed=0:00:23.51    frame=  736 fps= 31 q=-1.0 size=   24514KiB time=00:00:24.53 bitrate=8185.4kbits/s speed=1.02x elapsed=0:00:24.01    frame=  751 fps= 31 q=-1.0 size=   24967KiB time=00:00:25.03 bitrate=8170.2kbits/s speed=1.02x elapsed=0:00:24.51    frame=  766 fps= 31 q=-1.0 size=   25310KiB time=00:00:25.53 bitrate=8120.4kbits/s speed=1.02x elapsed=0:00:25.01    frame=  781 fps= 31 q=-1.0 size=   26113KiB time=00:00:26.03 bitrate=8217.1kbits/s speed=1.02x elapsed=0:00:25.51    frame=  796 fps= 31 q=-1.0 size=   26825KiB time=00:00:26.53 bitrate=8282.0kbits/s speed=1.02x elapsed=0:00:26.01    frame=  811 fps= 31 q=-1.0 size=   27110KiB time=00:00:27.03 bitrate=8215.2kbits/s speed=1.02x elapsed=0:00:26.51    frame=  826 fps= 31 q=-1.0 size=   27455KiB time=00:00:27.53 bitrate=8168.6kbits/s speed=1.02x elapsed=0:00:27.01    frame=  841 fps= 31 q=-1.0 size=   27989KiB time=00:00:28.03 bitrate=8178.9kbits/s speed=1.02x elapsed=0:00:27.51    frame=  856 fps= 31 q=-1.0 size=   28560KiB time=00:00:28.53 bitrate=8199.6kbits/s speed=1.02x elapsed=0:00:28.01    frame=  871 fps= 31 q=-1.0 size=   28903KiB time=00:00:29.03 bitrate=8155.2kbits/s speed=1.02x elapsed=0:00:28.51    frame=  887 fps= 31 q=-1.0 size=   29413KiB time=00:00:29.53 bitrate=8158.1kbits/s speed=1.02x elapsed=0:00:29.01    frame=  901 fps= 31 q=-1.0 size=   30007KiB time=00:00:30.03 bitrate=8185.0kbits/s speed=1.02x elapsed=0:00:29.51    frame=  917 fps= 31 q=-1.0 size=   30627KiB time=00:00:30.53 bitrate=8217.0kbits/s speed=1.02x elapsed=0:00:30.01    frame=  931 fps= 31 q=-1.0 size=   31119KiB time=00:00:31.03 bitrate=8214.7kbits/s speed=1.02x elapsed=0:00:30.51    frame=  946 fps= 31 q=-1.0 size=   31450KiB time=00:00:31.53 bitrate=8170.5kbits/s speed=1.02x elapsed=0:00:31.01    frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.51    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.01    frame=  991 fps= 30 q=-1.0 size=   32797KiB time=00:00:33.03 bitrate=8133.3kbits/s speed=1.02x elapsed=0:00:32.51    frame= 1006 fps= 30 q=-1.0 size=   33295KiB time=00:00:33.53 bitrate=8133.9kbits/s speed=1.02x elapsed=0:00:33.01    frame= 1021 fps= 30 q=-1.0 size=   33723KiB time=00:00:34.03 bitrate=8117.3kbits/s speed=1.02x elapsed=0:00:33.51    frame= 1036 fps= 30 q=-1.0 size=   34879KiB time=00:00:34.53 bitrate=8273.9kbits/s speed=1.02x elapsed=0:00:34.01    frame= 1052 fps= 30 q=-1.0 size=   35300KiB time=00:00:35.03 bitrate=8253.1kbits/s speed=1.02x elapsed=0:00:34.51    frame= 1066 fps= 30 q=-1.0 size=   35610KiB time=00:00:35.53 bitrate=8209.7kbits/s speed=1.01x elapsed=0:00:35.01    frame= 1082 fps= 30 q=-1.0 size=   36127KiB time=00:00:36.03 bitrate=8212.4kbits/s speed=1.01x elapsed=0:00:35.51    frame= 1096 fps= 30 q=-1.0 size=   36471KiB time=00:00:36.53 bitrate=8178.0kbits/s speed=1.01x elapsed=0:00:36.01    frame= 1112 fps= 30 q=-1.0 size=   37017KiB time=00:00:37.03 bitrate=8187.8kbits/s speed=1.01x elapsed=0:00:36.51    frame= 1126 fps= 30 q=-1.0 size=   37333KiB time=00:00:37.53 bitrate=8148.2kbits/s speed=1.01x elapsed=0:00:37.01    frame= 1142 fps= 30 q=-1.0 size=   38005KiB time=00:00:38.03 bitrate=8185.7kbits/s speed=1.01x elapsed=0:00:37.51    frame= 1156 fps= 30 q=-1.0 size=   38464KiB time=00:00:38.53 bitrate=8177.2kbits/s speed=1.01x elapsed=0:00:38.01    frame= 1171 fps= 30 q=-1.0 size=   38978KiB time=00:00:39.03 bitrate=8180.4kbits/s speed=1.01x elapsed=0:00:38.51    frame= 1186 fps= 30 q=-1.0 size=   39477KiB time=00:00:39.53 bitrate=8180.2kbits/s speed=1.01x elapsed=0:00:39.01    frame= 1201 fps= 30 q=-1.0 size=   39769KiB time=00:00:40.03 bitrate=8138.0kbits/s speed=1.01x elapsed=0:00:39.52    frame= 1217 fps= 30 q=-1.0 size=   40314KiB time=00:00:40.54 bitrate=8145.9kbits/s speed=1.01x elapsed=0:00:40.02    frame= 1231 fps= 30 q=-1.0 size=   40704KiB time=00:00:41.03 bitrate=8126.3kbits/s speed=1.01x elapsed=0:00:40.52    frame= 1247 fps= 30 q=-1.0 size=   41325KiB time=00:00:41.54 bitrate=8149.5kbits/s speed=1.01x elapsed=0:00:41.02    frame= 1261 fps= 30 q=-1.0 size=   41634KiB time=00:00:42.03 bitrate=8114.1kbits/s speed=1.01x elapsed=0:00:41.52    frame= 1277 fps= 30 q=-1.0 size=   42741KiB time=00:00:42.53 bitrate=8230.8kbits/s speed=1.01x elapsed=0:00:42.02    frame= 1291 fps= 30 q=-1.0 size=   43264KiB time=00:00:43.03 bitrate=8235.9kbits/s speed=1.01x elapsed=0:00:42.52    frame= 1307 fps= 30 q=-1.0 size=   43585KiB time=00:00:43.53 bitrate=8201.0kbits/s speed=1.01x elapsed=0:00:43.02    frame= 1321 fps= 30 q=-1.0 size=   43806KiB time=00:00:44.03 bitrate=8149.6kbits/s speed=1.01x elapsed=0:00:43.52    frame= 1337 fps= 30 q=-1.0 size=   44388KiB time=00:00:44.53 bitrate=8164.9kbits/s speed=1.01x elapsed=0:00:44.02    frame= 1351 fps= 30 q=-1.0 size=   44813KiB time=00:00:45.03 bitrate=8151.9kbits/s speed=1.01x elapsed=0:00:44.52    [out#0/rtp @ 0x2bdd2740] video:44766KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.213082%
[out#1/rtp @ 0x2be50d90] video:0KiB audio:1530KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.984959%
frame= 1367 fps= 30 q=-1.0 Lsize=   45309KiB time=00:00:45.53 bitrate=8151.4kbits/s speed=1.01x elapsed=0:00:45.03    
Exiting normally, received signal 2.

```

### source2.out（522字节）

```
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
t=0 0
a=tool:libavformat 62.12.102
m=video 61020 RTP/AVP 96
c=IN IP4 127.0.0.1
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F
m=audio 61022 RTP/AVP 97
c=IN IP4 127.0.0.1
b=AS:274
a=rtpmap:97 MPEG4-GENERIC/44100/2
a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=121056E500


```

### composition-rk386-vlc.log（13417字节）

```
-- logger module started --
main debug: VLC media player - 3.0.23 Vetinari
main debug: Copyright © 1996-2025 the VideoLAN team
main debug: revision 3.0.23-2-0-g79128878dd
main debug: configured with /builds/videolan/vlc/extras/package/win32/../../../configure  '--enable-update-check' '--enable-lua' '--enable-faad' '--enable-flac' '--enable-theora' '--enable-avcodec' '--enable-merge-ffmpeg' '--enable-mpc' '--enable-libass' '--enable-schroedinger' '--enable-realrtsp' '--enable-live555' '--enable-shout' '--enable-goom' '--enable-sse' '--enable-mmx' '--enable-libcddb' '--enable-zvbi' '--disable-telx' '--enable-nls' '--host=i686-w64-mingw32' '--with-contrib=../contrib/i686-w64-mingw32' '--with-breakpad=https://win.crashes.videolan.org' '--enable-qt' '--enable-skins2' '--enable-dvdread' '--enable-caca' 'host_alias=i686-w64-mingw32' 'CFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'CPPFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502' 'CXXFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'PKG_CONFIG=pkg-config' 'PKG_CONFIG_LIBDIR=/usr/i686-w64-mingw32/lib/pkgconfig:/usr/lib/i686-w64-mingw32/pkgconfig'
main debug: using multimedia timers as clock source
main debug:  min period: 1 ms, max period: 1000000 ms
main debug: searching plug-in modules
main debug: loading plugins cache file D:\VideoLAN\VLC\plugins\plugins.dat
main debug: recursively browsing `D:\VideoLAN\VLC\plugins'
main debug: plug-ins loaded: 495 modules
main debug: opening config file (C:\Users\ROG\AppData\Roaming\vlc\vlcrc)
main debug: looking for logger module matching "any": 2 candidates
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk386-vlc.log'
main debug: using logger module "file"
main debug: translation test: code is "C"
main debug: looking for keystore module matching "memory": 3 candidates
main debug: using keystore module "memory"
main debug: CPU has capabilities MMX MMXEXT SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 FPU 
main debug: Creating an input for 'Media Library'
main debug: Input is a meta file: disabling unneeded options
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' gives access `file' demux `directory' path `/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf'
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for access_demux module matching "file": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug:  (path: C:\Users\ROG\AppData\Roaming\vlc\ml.xspf)
main debug: looking for access module matching "file": 27 candidates
main debug: using access module "filesystem"
main debug: looking for stream_filter module matching "prefetch,cache_read": 24 candidates
cache_read debug: Using stream method for AStream*
cache_read debug: starting pre-buffering
cache_read debug: received first data after 0 ms
cache_read debug: pre-buffering done 304 bytes in 0s - 296875 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 0105e730
main debug: looking for stream_filter module matching "any": 24 candidates
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for demux module matching "directory": 56 candidates
main debug: using demux module "directory"
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
main debug: no meta reader modules matched
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' successfully opened
main debug: looking for xml reader module matching "any": 1 candidates
main debug: using xml reader module "xml"
main debug: EOF reached
main debug: removing module "directory"
main debug: removing module "record"
main debug: removing module "playlist"
main debug: removing module "cache_read"
main debug: removing module "filesystem"
main debug: creating audio output
main debug: looking for audio output module matching "any": 6 candidates
mmdevice debug: using default device
mmdevice debug: display name changed: VLC media player (LibVLC 3.0.23)
mmdevice debug: version 2 session control unavailable
mmdevice debug: volume from -65.250000 dB to +0.000000 dB with 0.031250 dB increments
main debug: using audio output module "mmdevice"
main debug: keeping audio output
main debug: looking for interface module matching "rc,none": 16 candidates
oldrc debug: base: 127.0.0.1, port: 63020
main debug: net: listening to 127.0.0.1 port 63020
main debug: using interface module "oldrc"
main debug: looking for interface module matching "hotkeys,none": 16 candidates
main debug: using interface module "hotkeys"
main debug: looking for interface module matching "globalhotkeys,none": 16 candidates
main debug: using interface module "win32"
main: Running vlc with the default interface. Use 'cvlc' to use vlc without interface.
main debug: looking for interface module matching "any": 16 candidates
main debug: looking for extension module matching "any": 1 candidates
lua debug: Opening Lua Extension module
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\extensions
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\extensions
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Scanning Lua script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Script D:\VideoLAN\VLC\lua\extensions\VLSub.luac has the following capability flags: 0x5
main debug: using extension module "lua"
main debug: using interface module "qt"
main debug: processing request item: null, node: Playlist, skip: 0
main debug: rebuilding array of current - root Playlist
main debug: rebuild done - 1 items, index -1
main debug: starting playback of new item
main debug: resyncing on udp://:62024
main debug: udp://:62024 is at 0
main debug: creating new input thread
main debug: Creating an input for 'udp://:62024'
main debug: requesting art for new input thread
qt debug: IM: Setting an input
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `udp://@:62024' gives access `udp' demux `any' path `@:62024'
main debug: creating demux: access='udp' demux='any' location='@:62024' file='\\@:62024'
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: udp://@:62024
main debug:  (path: \\@:62024)
main debug: looking for access module matching "udp": 27 candidates
main debug: looking for meta fetcher module matching "any": 1 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
udp debug: opening server=:0 local=:62024
main debug: net: opening any datagram port 62024
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
main debug: accepted socket 760 (from socket 1176)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
main error: read error: No error
prefetch debug: end of stream
playlist debug: not enough data
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\playlist
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\playlist
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\anevia_streams.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\anevia_xml.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\appletrailers.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\bbc_co_uk.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\cue.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\dailymotion.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\jamendo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\koreus.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\liveleak.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\newgrounds.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\rockbox_fm_presets.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\soundcloud.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\twitch.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\vimeo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\vocaroo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\youtube.luac
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for udp://@:62024
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='udp' demux='any' location='@:62024' file='\\@:62024'
main debug: looking for demux module matching "any": 56 candidates
es debug: cannot peek
es debug: cannot peek
es debug: cannot peek
es debug: cannot peek
es debug: cannot peek
mod debug: MOD validation failed (ext=)
ps debug: cannot peek
mjpeg warning: no more data
mjpeg error: cannot peek
mpgv debug: cannot peek
avcodec warning: cannot peek
ps debug: cannot peek
vobsub debug: could not read vobsub IDX file
main debug: no demux modules matched
main debug: removing module "record"
main debug: removing module "prefetch"
main debug: removing module "udp"
main debug: dead input
main debug: nothing to play
main debug: removing module "mmdevice"
qt debug: requesting exit...
qt debug: waiting for UI thread...
qt debug: IM: Deleting the input
qt debug: video widget is orphaned
main debug: exiting
main debug: no exit handler
qt debug: QApp exec() finished
qt debug: video widget is orphaned
qt debug: Killing extension dialog provider
qt debug: ExtensionsDialogProvider is quitting...
main debug: removing module "lua"
main debug: removing module "win32"
main debug: removing module "hotkeys"
main debug: removing module "oldrc"
main debug: destroying
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp36260
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

```

## VLC实际命令与清理确认

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk386-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk386-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru
$p.Id
```

本轮12项远端文件及其本机副本、1项本机VLC日志已在归档后清理；远端五个精确PID均不存在，本机VLC进程不存在，未生成快照。复用源、源码及构建产物保留。
