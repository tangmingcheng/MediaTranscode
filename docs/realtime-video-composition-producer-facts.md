# 合屏逐 producer 资源事实

基线5877d9db。解除 producer registry 对整图单一 input/video/audio envelope 的直接依赖，为多源分配归属接入统一入口；本阶段不是完整多源总账。

## 依据与边界

[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)以协商的格式、allocator、大小与数量决定分配；[FFmpeg AVBuffer](https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html)区分共享数据与独立引用。因此事实以真实生产者的 node/stream/payload 键组织，同键 fanout 共用一份；不同源即使尺寸相同也不能认定同一 allocation。没有新增线程、运行队列、公共参数或平台替代链。

FactsPlanner 从原单源规划产品生成逐 producer logical bound、frame contract 与 authority。原 maximumBytes、frameCreditContract、allocationAuthority 三函数体逐字保持；frame contract 解析集中为共享 planner。FinalCompiler 实际先生成事实再调用 Registry，后者不再接受整链 ledger。现有单源有效结果保持，缺事实时更早直接失败，不再返回 incomplete 产品。

Registry 对最终图核对全部分配 producer 输出键；拒绝缺失、重复、多余、无输出、错误 stream/payload 及不存在/重复的 selectedNodes。选择仍按 producer 节点，允许目标在其他执行段，保留动态输出跨段语义。受控节点输出表决定 runtime integration，不允许调用方布尔授权。帧scope由同一实际node.options解析器重新推导并与传入事实逐字段比较，软件帧不能被标记为设备observed-only而绕过扣账。

aggregate已调用reserve/shrink/attach，受控表记录其真实生产能力，但单源facts与实际frame合同解析仍拒绝aggregate；FinalCompiler白名单也保持拒绝。未齐备完整合同前不能放行。logicalBytes来自图像逻辑尺寸，不能用实际device surfaceBytes替代其量纲。Windows/RKMPP共用此规划与验证，adapter和外部FFmpeg未改。

## 仍需完成的总账

allocation owner与execution domain不同：aggregate候选仍持有源分配，encoder clone仍归canvas。必须补每源prepared input/startup/ingress envelope、逐decoder/filter/transfer及音频实际frame事实，再将独立allocation容量求和、有证据的alias去重、aggregate候选/贡献/metadata加入同一全局准入。不能相加N整链ledger、按相同尺寸去重、用全图hasFilter判断逐源pending，或自动把不足预算抬到required。

唯一输出池继续采用同图canvas retention；首次pool/readback准备事务、完整头对象预算、纯视频源、公共多源入口及Windows→RKMPP矩阵仍缺。完整合屏FAIL、42/100。

## 审查与验证

首轮A认可同图即时生成的信任边界，B指出独立facts接口缺少实际帧scope复核。本轮采纳B要求，生成与消费共用实际节点frame resolver，修后重新冻结审查。修后9源码双独立Standards/阶段Spec PASS。15文件UTF-8/CRLF及diff检查通过。Release全量session7632成功（662项构建图），configure/build均exit0，CLI2026-09-29 09:17:47、5140992bytes；不将旧共享路径运行当作多源执行证据。


## r47实际realtime链路

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps；AAC CBR192kbps、44100Hz双声道。固定120秒源，无local。CLI16940/session89889、FFmpeg10880/session33644、VLC30448（启动exit0），5秒监控session88902。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-producer-facts-r47 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-producer-facts-r47.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-producer-facts-r47-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-producer-facts-r47-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-producer-facts-r47-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-producer-facts-r47- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果

源完整3600帧/120.00秒自然exit0；CLI自然exit1/no-progress。09:20:43.187 purge7ack complete、old1/next2 acquiring；48.687 abort.begin、48.718 abort.done。edge46/47均10/10。final queued/workers/payloadBytes=0，仍4逻辑对象（reservations71848/releases71844），高水12081271bytes/104objects；workerErrors/errors/pressureFailures=0，stalledIntervals=1。

386CPU采样/22核：整机口径进程均值1.466832%、峰3.263403%；单核均值32.270312%、峰71.794872%。runtime WS初始125476864、最终189239296、峰194007040bytes。5秒监控22采样exit0，WS峰194007040、Private峰410730496；首09:19:02 CPU6.53125s/WS191930368/Private409628672，末09:20:47 CPU40.203125s/WS189239296/Private407384064。

118条generation1 raw/filtered漂移绝对最大20272ns，不证明恢复。sender86113datagrams/104568836payloadbytes，deadline/pressure/partial/ambiguous均0，pacing_cancelled=1，delivery_evidence=not_proven。VLC截图composition-producer-facts-r47-2026-09-29-09h19m30s628.png已观察到1280×720游戏画面；日志有buffer deadlock prevented、playback too late143915/1078977、丢迟到音频以及画面迟到946–1113ms。

原共享路径实际经过新FactsPlanner与Registry，能开始并持续转码；不能证明多源事实生成、allocation总账或aggregate执行。完整FAIL42，旧no-progress/4对象及AAC重入未解决。

### 清理

先归档命令和结果，再核验并清理VLC30448（不计自然退出）。CLI16940、源10880已自然结束。精确清单：composition-producer-facts-r47-cli.log、-source.log、-vlc.log、composition-producer-facts-r47.sdp及上述PNG。无远程、pcap、临时录制或测试脚本；指定复用源124427809bytes保留。清理复核完成：5文件及3个精确PID均无残留，指定复用源大小确认保留。
