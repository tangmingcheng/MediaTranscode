# Realtime MPEG-TS UDP 工作集增长排查

## 根因

2026-07-29 的真实 CLI 对照验证确认：持续增长并非 DAG 队列、`AVPacket`、
`AVFrame` 或 CUDA/NVENC 对象泄漏，而是输入 URL 配置了过大的 FFmpeg UDP
接收环形缓冲区：

```text
udp://127.0.0.1:<port>?fifo_size=1000000&overrun_nonfatal=1
```

本机 FFmpeg `-h protocol=udp` 表明 `fifo_size` 的单位是 188 字节包。
`1000000` 对应约 179.3 MiB。Windows 在环形区页面首次被媒体数据写入时逐步增加
进程工作集，因此增长速度接近输入码率，看起来像稳定的线性泄漏。

验收配置改为：

```text
udp://127.0.0.1:<port>?fifo_size=65536&overrun_nonfatal=1
```

`65536` 对应约 11.75 MiB。`overrun_nonfatal=1` 只决定缓冲区溢出时是否继续运行，
不会限制缓冲区容量。

## 复现特征

- realtime CLI 工作集约每秒增长 1.2–1.3 MiB；
- DAG queued、`AVPacket`/`AVFrame` 活跃数量及有效载荷保持稳定；
- RTP 输入到 RTP 输出在相同编解码链路下内存稳定；
- 降低 UDP `fifo_size` 后，MPEG-TS 输入到 MPEG-TS 输出也稳定。

## 真实验收命令

以下命令均使用 PowerShell 和绝对路径。实时验收必须同时看到
`media_transcode_realtime_video_cli.exe`、`ffmpeg.exe`、`vlc.exe`，并在 60 秒内结束。

Local CLI：

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_local_video_cli.exe' `
  --input 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' `
  --output 'D:\Code\MyCode\MediaTranscode\out\validation\local-final.mp4' `
  --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256
```

MPEG-TS 发送：

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re `
  -i 'D:\Code\MyCode\MediaTranscode\out\validation\avsync-continuous-180s.mp4' `
  -map 0:v:0 -map 0:a:0 -c copy -f mpegts `
  'udp://127.0.0.1:64200?pkt_size=1316'
```

MPEG-TS realtime CLI：

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' `
  --media-id final-mpegts --input-type mpegts-udp --input-layout mpegts `
  --output-layout mpegts `
  --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' `
  --output 'udp://127.0.0.1:64210' `
  --open-timeout-ms 5000 --read-timeout-ms 1000 `
  --analyze-duration-us 5000000 --probe-size 5000000 `
  --mpegts-max-pcr-gap-ms 120 `
  --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 `
  --startup-max-video-unit-bytes 4194304 `
  --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 `
  --video-codec h264 --width 1280 --height 720 --fps 30 `
  --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 `
  --sample-rate 48000 --channels 2 --max-duration 60 `
  --progress-timeout-ms 5000 --first-output-timeout-ms 10000 `
  --poll-interval-ms 100
```

MPEG-TS VLC：

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:64210' --no-video-title-show
```

RTP realtime CLI 使用输入端口 64300/64302、输出端口 64320，并使用发送 SDP
发布的 H.264 与 MPEG4-GENERIC AAC `fmtp`。先启动 CLI，再启动 FFmpeg：

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' `
  --media-id final-rtp --input-type rtp --input-layout separate `
  --output-layout separate `
  --video-rtp-url 'rtp://127.0.0.1:64300' --video-rtp-codec h264 `
  --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 `
  --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' `
  --audio-rtp-url 'rtp://127.0.0.1:64302' --audio-rtp-codec aac `
  --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 `
  --audio-rtp-channels 2 `
  --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' `
  --rtp-host 127.0.0.1 --rtp-port 64320 `
  --sdp 'D:\Code\MyCode\MediaTranscode\out\validation\rtp-output-final.sdp' `
  --packet-size 1200 --open-timeout-ms 5000 --read-timeout-ms 1000 `
  --analyze-duration-us 5000000 --probe-size 5000000 `
  --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 `
  --startup-max-video-unit-bytes 4194304 `
  --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 `
  --video-codec h264 --width 1280 --height 720 --fps 30 `
  --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 `
  --sample-rate 44100 --channels 2 --max-duration 60 `
  --progress-timeout-ms 5000 --first-output-timeout-ms 10000 `
  --poll-interval-ms 100
```

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re `
  -i 'D:\Code\MyCode\MediaTranscode\out\validation\avsync-continuous-180s.mp4' `
  -map 0:v:0 -c:v copy -an -payload_type 96 -ssrc 11001 -f rtp `
  'rtp://127.0.0.1:64300?rtcpport=64301' `
  -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 11002 -f rtp `
  'rtp://127.0.0.1:64302?rtcpport=64303'
```

输出 SDP 由 CLI 写入绝对路径后交给 VLC：

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' `
  'D:\Code\MyCode\MediaTranscode\out\validation\rtp-output-final.sdp' `
  --no-video-title-show
```

## 2026-07-29 最终数据

- Local CLI：`completed=true`，errors/workerErrors/droppedBuffers 均为 0；
  H.264 2560×1440、AAC 44100 Hz，完整解码退出码 0。
- MPEG-TS 60 秒：CLI、FFmpeg、VLC 同时存在；后半段工作集约
  224.98–225.72 MiB，Private Bytes 约 563.74–564.72 MiB；
  errors/workerErrors/droppedBuffers 均为 0。首个音频、视频
  canonical target 和 master target 分别完全相同，启动差值为 0 ms。
- RTP 60 秒首次复验：后半段工作集 210.72–210.73 MiB，内存稳定；
  errors/workerErrors 均为 0，但在编码输出开始前发生一次启动期队列丢弃，
  最终 `droppedBuffers=1`，因此该次不计为完全通过。
- RTP 30 秒严格启动顺序复验：先启动 CLI，等待 1.5 秒完成 socket/worker
  就绪后再启动 FFmpeg；CLI、FFmpeg、VLC 同时存在，工作集约
  209.86–209.93 MiB，errors/workerErrors/droppedBuffers 均为 0。
  首个音频与视频 canonical/master target 差值均为 0.002 ms。
- RTP 60 秒严格启动顺序最终验收：CLI、FFmpeg、VLC 同时存在，工作集
  211.21–211.58 MiB，Private Bytes 549.25–549.61 MiB；
  errors/workerErrors/droppedBuffers 均为 0。
- 两条实时链路均无 `reacquire`、`hard_phase_error` 或 drift-controller 错误。
- 运行期均未触发相位误差恢复。RTP 验收应在 CLI 就绪后再送流，避免把启动端
  过早送入的单包淘汰混入稳定运行验收。

### MPEG-TS 持续 A/V 漂移数据

`MediaAudioDriftControllerNode` 在 planner 已决定的 compensation window 发布时输出
`av_drift_trace`，不另设硬编码采样周期。音频 phase 以与视频调度相同的 master clock
为参考，因此可以直接量化运行期 A/V 相位趋势。

60 秒链路共采集 10 个补偿窗口样本，覆盖 56.842 秒：

- raw phase：首值 -0.010061 ms，末值 +0.001418 ms，均值 +0.000355 ms，
  最大绝对值 0.010061 ms；
- filtered phase：首值 -0.010061 ms，末值 -0.000051 ms，均值 -0.001880 ms，
  最大绝对值 0.010061 ms；
- phase 首尾趋势：0.202 ppm；
- filtered frequency 为 -24–0 ppm，实际 stretch 为 -24–0 ppm；
- recovering 样本为 0，reacquire、hard-phase 和 drift error 均为 0。

这组数据证明在当前 60 秒窗口内相位误差没有持续扩大；它不能替代更长时间的 soak。

### RTP 持续 A/V 漂移数据

严格启动顺序的 60 秒链路共采集 10 个补偿窗口样本，覆盖 57.260 秒：

- raw phase 首值、末值、均值和最大绝对值均为 +0.000156 ms；
- filtered phase 末值和最大绝对值均为 +0.000156 ms；
- phase 首尾趋势为 0 ppm，filtered frequency 和 stretch 均为 0 ppm；
- recovering 样本为 0，reacquire、hard-phase 和 drift error 均为 0。

## 旧链路残留验收

同步 A/V 最终计划只允许保存 `avSyncRuntime`，视频单流计划只允许保存
`singleStreamOutput`，validator 要求两者严格二选一。生产代码不再保留旧的外层
video/audio output、mux、SDP、normalization 或 A/V barrier 权威字段。

排查命令：

```powershell
rg -n 'videoPacketCopyNormalizationRequired|audioPacketNormalizationRequired|avStartBarrier|legacyOutput|outer\.(videoOutput|audioOutput|muxedOutput|sdp|videoMux|audioMux)' `
  'D:\Code\MyCode\MediaTranscode\src\internal\graph'
```

预期无匹配。视频单流能力仍受支持，但通过独立的 `singleStreamOutput` 产品构建，
不会进入同步 A/V 链路。

## 以后复现时的排查顺序

1. 执行 `D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -h protocol=udp`，
   确认当前 FFmpeg 的 `fifo_size` 语义。
2. 检查 realtime CLI 的 MPEG-TS 输入 URL，避免把大 FIFO 的逐页工作集提交误判为泄漏。
3. 使用 `fifo_size=65536` 做不超过 60 秒的真实三进程复测。
4. 汇总 `av_drift_trace` 的样本数、raw/filtered phase 最大绝对值、均值、首尾值和
   趋势，同时检查 `reacquire`、`hard_phase_error`、drift-controller 错误和人眼音画。
5. 若降低 FIFO 后仍持续增长，再检查 DAG 队列、包/帧存量及 FFmpeg 内部引用。
