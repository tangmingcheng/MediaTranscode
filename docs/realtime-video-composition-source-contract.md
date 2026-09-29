# 源端合同与准备画布

## 实施边界

2026-09-28，基线9c0d53a9。继续合屏准备接线，解决组合图依赖完整单源编码计划、源segment索取输出encoder/FIFO事实，以及画布在runtime重新分配的问题。完整协调器和公共入口仍未完成，不能据此声称多源可运行。

## 依据与合同

- [GStreamer preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html) 区分准备、状态提交与flush；本项目保留图前资源直到唯一生产owner领取，不把探测帧发布为生产lineage。
- [FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 要求按codec状态推进输入/输出；重复等待不等于可重新提交已消费的RTP前缀。首帧准备仍须保留同decoder、原deadline和原始到达时间。
- source-only合同表达输入、decoder/filter和源运行产品；唯一输出负责encoder、FIFO及协议。真实共享输出codec的设备endpoint仍由源filter消费，不能用虚构逐源encoder替代。
- 准备画布拥有实际encoder帧池分配的黑模板和有限surface；使用真实tile首帧执行平台copy。图前和领取时验证plan与同一pool身份；运行领取绑定真实owner wakeup，不能缺准备对象时重新分配。
- 单个聚合worker独占画布与候选；准备对象领取受mutex保护，没有新增媒体工作线程或任务队列。分配/copy/合同错误明确失败，RAII释放资源。局部surface/header上界不等于跨源总资源准入或driver/RSS保证。

## 下一生产事务的依赖

现有RTP preflight在prepareRawRtpVideo返回后规划整链并sealPreflight。组合协调器必须在seal前完成真实首帧、实际filter输出和tile copy，再释放探测帧并交接同decoder；所有源成功、唯一输出及全局资源产品完整后才能构建DAG。

各源prepare当前拥有独立open/analysis截止时间和持续capture。不能逐源无限等待或为每次probe刷新截止时间；串行准备也必须计入其他源继续capture的保留上界。raw快照预算、源准备storage、实际画布、唯一输出与runtime owner总账须明确区分并汇总。尚不能通过相加多份单源输出账得到组合账。

非选定纯视频源仍需真实单轨clock/startup合同；既有A/V源的非选定音频必须消费，禁止伪造音频满足双轨启动。等比内容矩形、迟到工作上界、已批准inputs/grid/audioSource入口，以及Windows→RKMPP 2–4源失活/恢复/持续输出矩阵继续保留。

## 验证

双独立源码审查与Release全量构建通过；r28实时及local-r28本地回归均FAIL，详见下文。此前r27失败见[解码器交接记录](realtime-video-composition-decoder-handoff.md)。

冻结33个源码文件后，两位未参与实现者Standards/限定阶段Spec均PASS。Release全量重建configure/build均exit0，33源码构建前后SHA256一致。完整合屏及交付门禁仍未通过。

## r28 原规格实时回归

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps，1280×720/30fps，AAC CBR192kbps、44100Hz双声道；指定连续120秒源，无循环/降规格。CLI PID18704自然exit1；FFmpeg PID32184完整3600帧/120秒自然exit0；VLC PID31388，10:22:18截图已实际查看游戏画面1280×720。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-contract-r28 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60930 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60932 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61930 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-contract-r28.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-contract-r28-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60930?rtcpport=60931&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60932?rtcpport=60933&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-contract-r28-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-contract-r28-vlc.log --extraintf=rc --rc-host=127.0.0.1:62930 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-contract-r28- --snapshot-format=png rtp://@127.0.0.1:61930
```

完整验收FAIL。10:23:42.746七项purge ack完成，old1/next2进入acquiring；10:23:48.122 abort，无进展超时，边46/47各10项满队列。最终queued/workers/payload bytes为0，payload objects4；71849次reservation/71845次release，高水12219751字节/84对象，errors/workerErrors/pressure failures均0，stall1。

403个CPU样本，22逻辑核，整机平均1.361013%、峰4.212860%，单核等效均29.942283%、峰92.682927%。工作集114511872→186961920，峰191610880字节。后台10:22:03 CPU6.828125秒/WS189972480/Private400224256，10:23:39 CPU35.609375秒/WS191610880/Private401367040，10:23:44 CPU37秒/WS186961920/Private398516224；不证明长期稳定。118条generation1漂移raw/filtered最大绝对值156ns，无重入证据。

sender提交86262datagrams/104739504payload字节；deadline/pressure/partial/ambiguous失败0、pacing取消1，delivery_evidence=not_proven。运行的是Shared单源回归，未覆盖新composition source产品或prepared canvas实际生产领取，也未验证RKMPP。VLC告警待关闭后补记。将按身份核对PID31388清理VLC，清理不记自然退出；本轮日志/SDP/截图随后逐项删除。

VLC使用D3D11VA，有3条too late（音频99800、视频missing54/21ms）及Failed to create video converter；画面存在不等于播放门禁通过。VLC已强制清理，CLI/源自然退出。

清理核对：5项文件删除，本轮前缀残留0、三个媒体PID残留0，监控进程exit0，指定120秒源保留；无本轮远程测试或抓包。

## local-r28 本地共用路径回归

指定同一120秒文件，H.264/AAC文件 → MP4 HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps/44100Hz双声道。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_local_video_cli.exe --input D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 --output D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-contract-local-r28.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-contract-local-r28-cli.log 2>&1
exit $LASTEXITCODE
```

结果FAIL：CLI PID9188自然exit1，未生成输出文件，VLC播放命令未执行。10:25:41.941 CUDA候选报 opened encoder did not expose a valid retained-frame bound；随后QSV av_hwframe_ctx_init失败（纹理80070057），10:25:42.617图构建HardwareUnavailable。源码触发条件为MediaEncoderEmissionPreflightAdapter::probe的context.max_b_frames<0；调用发生在LocalFileTranscodeGraphBuilder::planVideoTranscodeFile能力规划，早于本轮改动的segment option映射。这些能力/本地规划实现本轮未修改；没有运行旧二进制对照，不能据此声称已复现基线或修复。

后台仅取得10:25:42.182 CPU0.34375秒、WS109281280、Private237510656一项，无法形成趋势；监控因目标消失终止exit1。没有进入生产DAG，无A/V漂移或播放证据。未降低复杂度、未关闭硬件或更改外部FFmpeg。仅产生composition-source-contract-local-r28-cli.log，归档后删除；无源流/VLC进程或输出媒体需要清理。

local-r28清理核对：本轮前缀文件0、PID9188残留0，指定源保留。
