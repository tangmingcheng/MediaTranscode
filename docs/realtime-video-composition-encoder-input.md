# 合屏编码器帧输入与能力读回

## 实施范围

基线e6f411d9。唯一输出encoder不能要求虚构源流：共用创建器原先强制AVCodecParameters/sourceFormat/sourceTime，但只消费尺寸、SAR、色彩和cadence。本轮改收显式原始帧属性；尺寸、帧率必须来自已有planner open合同，缺失即失败。metadata resolver与动态输出preparer两个真实调用者同步接入，既有未知SAR、unspecified色彩按实际源事实保留。

## 行业依据及实现边界

- [FFmpeg编码接口](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)以AVFrame为encoder输入，不要求上游decoder或AVStream；[AVCodecContext](https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html)明确SAR、色彩、尺寸和时基字段。这里只改变事实传递，继续共用原format、rate-control、硬件帧池、open及readback adapter，不新增平台链路或公共参数。
- `MediaOpenedVideoEncoderProbe`接收选定encoder stage与实际打开的context，统一原通用硬件、RKMPP和软件路径的packet-layout/random-access/emission读回；三条现有能力路径都调用它。原RKMPP已广告软件surface的等价探测保留，继续检查codec格式支持和open readback等价；没有新增runtime fallback。
- 探测可能发送帧并进入drain，context仅供能力调查，不能交给生产继续编码。生产仍由`MediaVideoEncoderPreparer`保留未消费的实际context，并核对已准入emission/readback；硬件池按[AVHWFramesContext](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html)引用生命周期持有。
- 没有新增线程、队列或驻留预算，RAII及原分配错误传播保持。上游尺寸/帧率缺失现在在共同创建器明确拒绝，不能从源流再次猜测；完整合屏仍必须在DAG构建前形成这些事实。

## 剩余工作及验证

本轮尚未接通完整合屏协调器，也未实现画布encoder候选选择/设备准备。源/输出runtime权威拆分、真实filter/canvas准备事务、跨源总准入、纯视频源及inputs/grid/audioSource入口、Windows→RKMPP完整矩阵继续未完成。共用路径回归不能证明这些门禁通过。

两名未参与实现的独立审查者对7个源码均给出阶段Standards/Spec PASS，无新增阻断发现；核对了全部调用者、planner必需参数、SAR/色彩保持及探测context生命周期。完整合屏与交付仍FAIL，评分维持42/100。

本轮第一次Release全量构建configure成功，build触及120秒上限，脚本终止CMake/Ninja/CL进程树；随后实查无残留编译进程。第二次相同流程全量Release configure/build exit0，两个CLI时间11:14:24，7源码hash与冻结审查版一致。构建通过不代表媒体验收通过。

## r30 实时回归：FAIL

同规格RTP H.264/AAC→HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps/44100Hz双声道→MPEG-TS/RTP。使用指定120秒连续源，无循环、降规格或关闭硬件。本轮CLI PID8044（session65783），FFmpeg PID32828（40973），VLC PID18348；后台监控session26965。

实际命令：


```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-encoder-input-r30 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60950 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60952 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61950 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-input-r30.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-input-r30-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60950?rtcpport=60951&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60952?rtcpport=60953&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-input-r30-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-input-r30-vlc.log --extraintf=rc --rc-host=127.0.0.1:62950 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-encoder-input-r30- --snapshot-format=png rtp://@127.0.0.1:61950
```


FFmpeg完整3600帧/120.00秒自然exit0。11:16:02 VLC截图已实际查看，1280×720游戏画面；VLC使用D3D11VA，日志有音频too late104647和Failed to create video converter，不能判播放门禁通过。11:17:09.899七项purge ack完成、old1/next2进入acquiring；11:17:15.416 CLI无进展超时自然exit1，边46/47各10项满队列。没有重入或新合屏入口证据。

最终queued/workers/payload bytes为0，逻辑对象4，reservation71879/release71875；资源高水12065101字节/83对象，workerErrors/errors/pressure failures为0，stall1。428 CPU样本、22逻辑核，整机平均1.213179%/峰4.156479%，单核等效平均26.689931%/峰91.442543%；工作集108515328→191537152，峰196792320字节。后台11:15:37 CPU6.65625秒、WS194863104/Private402857984，11:15:42 CPU8.09375秒、WS194863104/Private402862080；监控自然exit0。118条generation1漂移记录，raw/filtered最大绝对值均156ns；短时趋势不能证明长期内存或A/V稳定。

sender提交86344 datagrams/104842272 payload字节，deadline/pressure/partial/ambiguous失败及pacing取消均0；delivery_evidence=not_proven。与r29一样仍为完整验收FAIL，不宣称退出修复；动态输出、RKMPP及多源合屏未运行。

源与CLI自然退出。归档后按精确身份清理VLC PID18348（不记自然退出），并逐项删除本轮三日志、SDP及11:16:02截图；后续实查媒体进程残留0、实时前缀文件0，固定源保留。无本轮抓包或远程测试。

## local-r30 本地回归：FAIL

同一固定源、HEVC CBR8Mbps、1280×720/30fps与AAC CBR192kbps/44100Hz双声道，实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_local_video_cli.exe --input D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 --output D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-input-local-r30.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-input-local-r30-cli.log 2>&1
exit $LASTEXITCODE
```

CLI PID32828（操作系统复用了已退出FFmpeg的PID，按新进程身份核验），约1秒自然exit1。11:18:41.422 CUDA候选仍因opened encoder did not expose a valid retained-frame bound拒绝；11:18:42.012 QSV纹理80070057/帧池失败，DAG构建前HardwareUnavailable，无输出媒体。宣布的VLC播放命令未执行。

后台监控exit0，四样本：11:18:41.353 CPU0.1875秒/WS136441856/Private241647616；.567 CPU0.359375/WS105263104/Private244625408；.774 CPU0.484375/WS131641344/Private262799360；.975 CPU0.640625/WS174772224/Private263340032。无生产A/V漂移，短时运行不能判断内存趋势。未降低规格；这是能力规划失败证据，不能证明后续encoder节点运行通过。

仅产生local-r30-cli.log，结果归档后按清单删除；实查本轮全部前缀文件0、媒体进程0，固定源保留。动态输出与RKMPP尚未复验，不宣称通过；当前完整合屏仍未完成。
