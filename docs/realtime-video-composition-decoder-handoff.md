# 准备解码器交接

## 范围与依据

2026-09-28，基线7a7312ac。把首帧证明得到的同一个decoder交给组合图，补齐此前仅绑定device后重新open的断点。旧Shared/local单源继续走同一公共decoder builder；组合source_decode必须有准备对象，不能缺失时重新open。

[FFmpeg flush API](https://ffmpeg.org/doxygen/trunk/group__lavc__misc.html) 说明flush释放decoder内部引用而不释放调用方帧引用。因此能力协商/copy完成后先释放探测首帧，再经已有codec adapter flush同一个context，原datagrams仍经正式clock/gate/canonical回放。[GStreamer preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html) 的准备、提交和flush边界作为发布前保留资源的参考，不把探测首帧伪装成生产lineage。

## 实现与所有权

- 共用MediaVideoDecoderPlanOptionCodec映射decoder产品、硬件格式、retention与明确lineage传播/容量。旧单源保留branch已经写入的lineage；source-only可直接为公共builder提供选项，不需要伪造encoder计划。
- prepareReplay只接受FirstFrame证据，释放帧并flush后创建一次性MediaPreparedVideoDecoder。源参数副本在准备额度内计费；生产claim只读原snapshot，核对codec/参数集/side data/time base/frame rate及planner帧合同/COPY_OPAQUE，领取后不可再次使用。
- storage lease随CodecContextPtr的deleter转移，含wrap/takeContext路径；先释放codec内部，再销毁reservation owner。尚未领取时源参数先销毁再释放decoder和额度。逻辑额度不等于RSS或全部FFmpeg/driver分配。
- 组合graph options/source domain/runtime state/registrar贯通准备对象；源对象必须非空且各源不同。图构建前核对必要device身份，运行注册再次核对。实际DRM PRIME descriptor的校验仍由公共frame/RGA adapter负责，不要求没有AVHWDeviceContext的RKMPP与CUDA同构。
- 单次领取由mutex保护，decoder只由之后的生产节点执行；没有新增线程、媒体队列或默认容量。失败保持显式报错，不回退重建decoder。

## 尚未完成

完整preflight协调器尚未调用首帧探测/交接，source-only预算与实际retention须由该事务汇总。需要同一prepared输出设备/画布的真实能力、纯视频非音源、等比内容矩形、跨源总账、迟到工作上界和已批准公共入口，随后Windows→RKMPP完整2～4源矩阵。FFmpeg flush为void，尤其RKMPP不能仅以调用返回当作恢复成功，仍须生产首帧与原deadline证据。未修改外部FFmpeg，未修复或宣布修复旧Shared无进展/4对象及AAC重入失败。

## 验证

冻结22个源码文件后，两位未参与实现的独立审查者均给出Standards PASS、限定阶段Spec PASS；完整合屏未通过。

首轮全量构建因新option codec缺少枚举名称声明头失败，已补正。第二轮触及120秒清理路径，taskkill报告子进程终止失败而脚本exit1；随后核对CMake/Ninja/CL/link均无残留，未把它当作成功。按同一全量入口继续复验。

第三次自动全量构建仍触及120秒截止并exit1，随后未发现构建进程残留。用户随后确认已经构建；本轮只读核对Release两CLI于09:55:30完成链接，12个修改的cpp对象均晚于源码，未重复构建。没有取得用户构建的configure/build退出码，不能将此记录为本代理全量构建成功。使用现有Release产物执行r27真实回归。

## r27 Release 原规格共享链路回归

2026-09-28，RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps，1280×720、30fps，AAC CBR192kbps、44100Hz双声道。指定连续120秒源。CLI PID11288自然exit1；FFmpeg PID26428完整3600帧/120秒自然exit0；VLC PID23600显示游戏画面，截图10:01:33已实际查看1280×720。完整验收FAIL：10:03:02.878完成7项purge ack后acquiring，10:03:08.177终止，无进展超时，边46/47各10项满队列。旧Shared缺陷仍复现，本轮不覆盖尚无生产caller的probe/prepareReplay或多源Preserve。

### 实际命令

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-decoder-handoff-r27 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60920 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60922 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61920 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-decoder-handoff-r27.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-decoder-handoff-r27-cli.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60920?rtcpport=60921&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60922?rtcpport=60923&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-decoder-handoff-r27-source.log 2>&1
exit $LASTEXITCODE
```

```powershell
& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-decoder-handoff-r27-vlc.log --extraintf=rc --rc-host=127.0.0.1:62920 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-decoder-handoff-r27- --snapshot-format=png rtp://@127.0.0.1:61920
```

### 指标与退出

最终queued/workers/payload bytes均0，payload objects4，reservation71871/release71867，高水11964257字节/83对象，pressure/workerErrors/errors均0，stall1。425个CPU样本，22逻辑核，整机平均1.181925%、峰3.125%，单核等效平均26.002349%、峰68.75%。工作集108584960→190410752，峰195354624字节。后台采样10:01:18 CPU4.34375秒/WS192667648/Private398282752，10:02:58 CPU31.078125秒/WS195354624/Private400154624，10:03:03 CPU32.109375秒/WS190410752/Private395599872；不证明长期稳定。118条generation1漂移，raw/filtered绝对值最大156ns，无重入证据。

sender提交86298datagrams/104789832payload字节，deadline/pressure/partial/ambiguous失败0，pacing取消1，purge取消15包/19212wire字节，最终backlog0，delivery_evidence=not_proven。播放告警待VLC关闭后补记。将核对并清理VLC PID23600，清理退出不记自然结束；随后逐项删除本轮日志、SDP及截图，保留指定源。

VLC使用D3D11VA，有2条too late及Failed to create video converter、解码surface/slice告警；截图确有画面，但播放门禁不判通过。CLI/源已自然退出；VLC已按身份核对后强制清理。

清理完成：上述5个文件已删除，本轮前缀文件0、三PID残留0，指定120秒源保留；无远程测试或本轮抓包。
