# 合屏唯一输出合同

## 本轮边界

基线6ffe4c3e。生产协调器仍被输出合同对源decoder的反向依赖阻断：`buildOutputEncoder`原先进入整链segment，发送规划又从完整单源计划重新提取编码读回。本轮移除这两项依赖，仍不代表公共合屏入口或多源验收完成。

## 依据与实施

- [FFmpeg编码接口](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)消费解码后的AVFrame；encoder配置和来源decoder是不同职责。输出产品只保留已选择的encoder stage、编码请求、lineage和abort合同，组合图消费该产品；旧完整链复用相同encoder映射与端口连线，不生成虚构decoder。
- [FFmpeg硬件帧池](https://ffmpeg.org/doxygen/trunk/structAVHWFramesContext.html)绑定具体device并使用引用计数；输出合同拆分不证明设备池已准备，实际encoder/画布仍须由生产事务准备和唯一领取。
- datagram planner直接接收上游已解析的`MediaPreparedRealtimeEmissionSet`。现有单源A/V及VideoOnly调用共同入口；编码读回的校验保留在上游resolver，wire计算、协议分片、队列硬界及失败传播沿用原实现。
- 不新增媒体线程、队列或平台执行链，不改变send/receive、背压和资源领取。准备期仍须遵循[GStreamer preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html)的准备与提交边界；不能用一次首帧输出声称GPU同步完成或长期驻留有界。

## 未完成与验证边界

唯一encoder候选准备、source/output runtime权威拆分、真实首帧filter/canvas事务、跨源总准入、纯视频源clock/startup、inputs/grid/audioSource入口及Windows→RKMPP完整矩阵仍未完成。旧单源链路只能验证本轮共用代码的回归，不能证明新合屏入口可运行。

11源码冻结后，首次Release构建因输出optional<bool>未显式取值而失败；修复为缺失事实拒绝、显式false保留。两位独立审查者复审Standards/限定阶段Spec均PASS；第二次全量Release configure/build exit0。构建后11源码hash与修正冻结版一致。源码与构建通过不等于媒体验收通过。

## r29 实时回归：FAIL

本轮CLI PID32728、FFmpeg PID2944、VLC PID17592；后台监控session93749。使用指定120秒连续源，无循环、降规格或关闭硬件。实际执行命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-output-contract-r29 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60940 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60942 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61940 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-contract-r29.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-contract-r29-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60940?rtcpport=60941&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60942?rtcpport=60943&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-contract-r29-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-contract-r29-vlc.log --extraintf=rc --rc-host=127.0.0.1:62940 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-output-contract-r29- --snapshot-format=png rtp://@127.0.0.1:61940
```

10:51:29的VLC截图已实际查看，游戏画面1280×720。FFmpeg完整3600帧/120.00秒自然exit0；CLI于10:52:50无进展超时自然exit1，边46/47各10项满队列。最终queued/workers/payload bytes为0，逻辑对象4，reservation71835/release71831，资源高水12060985字节/94对象；errors/workerErrors/pressure failures均0，stall1。完整验收FAIL，不证明合屏入口可运行。

404个CPU样本、22逻辑核，整机平均1.399737%/峰3.448276%，单核等效平均30.794204%/峰75.862069%。工作集115896320→191148032，峰195940352字节。后台10:51:06 CPU7.3125秒/WS193380352/Private402440192；10:52:27 CPU32.59375秒/WS195940352/Private404324352；10:52:47 CPU38.265625秒/WS191148032/Private400166912。不能从短时趋势声称长期稳定。

118条generation1漂移记录，raw/filtered最大绝对值均156ns；无重入证据。10:52:44.655七项purge ack完成、old1/next2进入acquiring，随后无进展失败。sender提交86197datagrams/104663412payload字节，deadline/pressure/partial/ambiguous失败0、pacing取消1，delivery_evidence=not_proven。VLC使用D3D11VA，有音频too late100696、视频missing52/19ms及Failed to create video converter，画面不等于播放验收通过。

源与CLI自然退出，监控exit0；已核对身份并强制清理VLC PID17592，不记自然退出。命令、指标及结论归档后清除本轮三日志、SDP和10:51:29截图；核对前缀文件0、三PID残留0，指定源保留。无远程测试或抓包。

## local-r29 本地回归：FAIL

指定同一120秒源，H.264/AAC文件→MP4 HEVC CBR8Mbps、1280×720/30fps、AAC CBR192kbps/44100Hz双声道。实际命令：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_local_video_cli.exe --input D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 --output D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-contract-local-r29.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-contract-local-r29-cli.log 2>&1
exit $LASTEXITCODE
```

CLI PID6296约1秒自然exit1，未生成输出，宣布的VLC播放命令未执行。CUDA候选仍因opened encoder did not expose a valid retained-frame bound失败；QSV帧池纹理80070057，10:54:37.699能力规划HardwareUnavailable。该失败发生在共用能力规划、早于segment配置，不能证明本地新映射已运行或通过；未降规格或改外部依赖。

后台10:54:37.057 CPU0.109375秒/WS178167808/Private307523584，.363 CPU0.3125秒/WS111976448/Private238419968，.622 CPU0.515625秒/WS178823168/Private263917568；进程很快退出，无法判断内存趋势，没有生产A/V漂移数据。监控exit0。仅生成本轮cli.log，归档后删除；未启动VLC或源流进程。

local-r29清理核对：前缀文件0、PID6296残留0，指定源保留。
