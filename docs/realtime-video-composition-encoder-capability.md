# 合屏独立编码能力准备

## 范围与依据

基线10978e42。完整合屏不能为唯一encoder虚构源decoder；现有候选创建、encoder open仍绑在整链能力探测中。本轮将候选枚举及打开/读回职责从源链分离，旧生产链立即消费相同实现，不增加独立平台媒体链或公共配置。

[FFmpeg VAAPI编码示例](https://ffmpeg.org/doxygen/trunk/vaapi_encode_8c-example.html)按raw-frame格式、设备与帧池打开encoder，没有上游decoder前置条件；[AVHWFramesContext](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html)将池绑定到具体device，并使用引用计数。这里复用该所有权与调用顺序，不采用示例中的固定帧池容量。尺寸/cadence/RC来自已有encoderOpenContract；实际帧池由调用方持有，新入口只验证并增加引用，不创建或猜测容量。

## 实施

- `enumerateEncoderCandidates`复用原backend profile、名称、优先级和帧合同；保留全部profile（含不可用codec）的表序。旧整链枚举直接配对同序源profile，原评分与候选选择不变。候选不是能力可用证明。
- `MediaVideoEncoderCapabilityProbe`统一encoder分配、帧属性、open adapter、设备/帧池引用及packet-layout/random-access/emission读回。通用硬件、RKMPP及软件三个原调用点共同使用；通用路径传入既有实际池，RKMPP保留原内部管理方式及广告软件surface的等价探测。
- 删除旧`MediaOpenedVideoEncoderProbe`外部入口。探测对象内部RAII持有context，失败或完成均销毁，不向调用方交付已发送帧或进入drain的context。生产`MediaVideoEncoderPreparer`仍独立创建未消费context。
- 验证encoder frame尺寸与open合同相符；传入硬件池时核对设备实例、像素/软件表面格式及可容纳尺寸。未知SAR可显式为0/1，缺失range保持原FFmpeg未指定值；旧整链synthetic probe仍显式传入原1/1，不冒充真实源SAR或tile证据。
- 不新增线程、队列、重试或运行时fallback；原背压及源链失败语义不变。原能力池容量未改变，也不得将其作为合屏生产总准入证据。

## 完成边界

本轮仅闭合候选/探测的共同入口，尚未形成保留唯一输出设备/context的合屏事务。完整source/output runtime、真实filter/tile/canvas、跨源总账、纯视频clock/startup、公共入口及Windows→RKMPP 2–4源矩阵仍未完成。不得将本轮单源回归当作合屏通过。

7条源码路径冻结（含旧接口删除、新接口增加），两名未参与实现者均给阶段Standards/Spec PASS，未发现新增阻断；完整功能/交付FAIL，评分保持42/100。首次Release全量configure/build exit0，两个CLI时间11:31:30，构建后5个现存源码hash与冻结版一致，旧两个源码确认删除。构建通过不代表媒体验收通过。

## r31 实时回归：FAIL

本轮CLI PID24692（session62060）、FFmpeg PID19332（49028）、VLC PID28048，后台监控92407。固定120秒源，RTP H.264/AAC→HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps/44100Hz双声道→MPEG-TS/RTP。实际执行：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-encoder-capability-r31 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60960 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60962 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61960 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-capability-r31.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-capability-r31-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60960?rtcpport=60961&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60962?rtcpport=60963&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-capability-r31-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-capability-r31-vlc.log --extraintf=rc --rc-host=127.0.0.1:62960 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-encoder-capability-r31- --snapshot-format=png rtp://@127.0.0.1:61960
```

FFmpeg完整3600帧/120.00秒自然exit0；11:33:17 VLC截图已实际查看，1280×720游戏画面。VLC使用D3D11VA，日志有音频too late88928及Failed to create video converter，不能判播放门禁通过。11:34:27.538七项purge ack完成、old1/next2进入acquiring；11:34:32.954 CLI无进展超时自然exit1，边46/47各10项满队列。没有重入或新合屏入口证据，完整验收FAIL。

最终queued/workers/payload bytes为0，逻辑对象4，reservation71905/release71901；资源高水12032305字节/79对象，workerErrors/errors/pressure failures均0，stall1。428 CPU样本、22逻辑核，整机平均1.144264%/峰3.149606%，单核等效平均25.173812%/峰69.291339%；工作集104910848→188837888，峰193830912字节。后台11:32:56 CPU7.671875秒、WS191959040/Private398155776；11:33:01 CPU8.875秒、WS192016384/Private398155776，监控自然exit0。118条generation1漂移记录，raw/filtered最大绝对值均156ns；短时趋势不能证明长期稳定。

sender提交86346 datagrams/104843104 payload字节，deadline/pressure/partial/ambiguous失败0，pacing取消1；delivery_evidence=not_proven。未宣称退出问题修复，也未把单源结果外推到动态输出或RKMPP。

源与CLI自然退出；结果归档后按精确身份清理VLC PID28048（不记自然退出），逐项删除三日志、SDP及11:33:17截图。实查实时前缀文件0、媒体进程0。无本轮抓包或远程测试，固定源保留。

## local-r31 本地回归：FAIL

指定同一120秒源，输出HEVC CBR8Mbps、1280×720/30fps和AAC CBR192kbps/44100Hz双声道。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_local_video_cli.exe --input D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 --output D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-capability-local-r31.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-encoder-capability-local-r31-cli.log 2>&1
exit $LASTEXITCODE
```

CLI PID7932约1秒自然exit1，无输出媒体，宣布的VLC播放命令未执行。11:35:46.749 CUDA仍因opened encoder did not expose a valid retained-frame bound拒绝；11:35:47.366 QSV纹理80070057/帧池初始化失败，最终DAG构建前HardwareUnavailable。未降低规格或改变外部FFmpeg；不能证明后续生产encoder运行通过。

后台监控exit0，四样本：11:35:46.624 CPU0.109375秒/WS122793984/Private140075008；.837 CPU0.296875/WS92536832/Private212180992；11:35:47.043 CPU0.34375/WS110161920/Private235741184；.245 CPU0.53125/WS178806784/Private266469376。无生产A/V漂移，短时数据不能判断内存趋势。

仅产生local-r31-cli.log，结果归档后删除；实查本轮全部前缀文件0、媒体进程0，固定源保留。动态输出和RKMPP未复验，完整合屏仍未完成。
