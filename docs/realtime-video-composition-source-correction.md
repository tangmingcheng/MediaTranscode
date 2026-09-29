# 源音频校正容量

## 设计与实际边界

基线4e7bd5e7。AudioEncodeBranchBuilder的真实链为decode→startupTrim→driftController→resample；driftController以解码后的canonical片段作观测，音频和校正通过同一原子事务发布。aggregate、encoder、scheduler与协议位于重采样之后，不是源校正执行路径的组成部分。

沿用[GStreamer有界队列延迟累计](https://gstreamer.freedesktop.org/documentation/additional/design/latency.html)与[FFmpeg显式软补偿](https://ffmpeg.org/doxygen/trunk/group__lswr.html)的职责边界：复用原容量乘法、样本域转换和校正quantizer，不新增控制算法。提取源组件界（decoder延迟、decode/resample队列、mailbox裕量、resampler块），旧整链规划调用同一源计算再叠加自己的encode/scheduler界。该和保留源组件的保守余量；观测点已在decode/startupTrim之后，校正可达性不依赖累计此前全部库存。这不是全源缓冲总账或最紧界，startupTrim等节点资源仍需资源compiler单独准入。

校正planner明确接收共享全链事实或源组件事实；源只累计自身前缀，原共享路径保留七项在途值和计算顺序。mailbox裕量按实际命令边容量推导，单执行块严格余量、measurement-gap headroom、补偿窗口和量化算法不变。移除结果中没有运行消费者的protocolBatchSamples记账字段，协议batch仍留在共享在途求和和输出事实中，不伪造源协议batch为0。

组合图构图前使用同一planner重算源合同，核对三项servo时间政策及实际事务边容量；不生成或修补调用方缺失政策。无新线程、批处理、队列容量、公共字段或平台链；源输入/命令与既有节点保持所有权和背压规则。缺少或冲突事实必须在构图前失败。该步骤仍不等于全源prepare/完整source runtime/公共入口接通。

首审追踪发现旧mailbox按queues.metadata记账，但真实音频和校正两边均用audioDriftTransaction（queues.frame）。现同步修正源与共享规划及validator，组合图核对真实事务边，不再把atomicMetadata当命令边。executor的lookahead是另一个有界命令窗口；满窗口阻塞拉取命令，不产生额外音频缓存，连续effective window和activeRemaining禁止超前执行，因此不叠加lookahead×distance为音频在途。

## 验证

8源码双独立最终Standards/阶段Spec均PASS。首审发现校正命令边与mailbox容量映射错误，已同步修正源及共享路径为真实frame容量。此修正可能改变lead/window，不宣称所有历史数值严格等价。两次Release全量session97593/34300均configure/build exit0、650项构建图；最终CLI更新时间2026-09-28 14:33:34、5122560bytes。没有运行local。

### r42：完整验收FAIL

FFmpeg PID30480/session6738自然exit0、3600帧/120.00秒；CLI PID34616/session97780自然exit1，无进展超时，未强制结束。VLC PID31716启动命令exit0，实际查看14h34m52s861截图为1280×720游戏画面；日志含playback迟到121883、picture迟到77/44ms及Failed to create video converter。

14:36:21.661 purge_ack=complete、7ack、old1/next2进入acquiring；26.781 abort.begin，26.820 abort.done。edge46/47均10/10；最终queued/workers/payloadBytes=0，4逻辑对象，reservations71838/releases71834，高水12103165bytes/95objects；workerErrors/errors/pressureFailures=0。最终stalledIntervals=0，不覆盖CLI明确的无进展失败。

372个CPU采样/22核：整机均值1.725782%、峰4.665314%；单核37.967208%/102.636917%。WS初始106418176、最终191766528、峰196976640bytes。后台5秒监控session91245 exit0：14:34:42 CPU7.734375s/WS194510848/Private399273984；14:36:22 CPU46.828125s/WS191766528/Private395587584，Private采样峰400834560。118条generation1漂移raw/filtered绝对最大156ns，不证明恢复；日志观察到compensation_distance=45346。sender提交86211datagrams/104682700payloadbytes，deadline/pressure/partial/ambiguous=0，pacing_cancelled=1，delivery_evidence=not_proven。

该共享单源路径执行共用源组件容量及校正规划，没有运行多源源专用分支、其他协议或RKMPP。原无进展和AAC重入未修复，完整FAIL/42。源专用校正产品不是完整source runtime或资源总账；唯一准备资源、多源协调器、纯视频源和原双平台门禁仍缺。

归档实际命令及必要证据后，按精确清单清理3日志、SDP及composition-source-correction-r42-2026-09-28-14h34m52s861.png共5项；VLC核对身份后按PID31716清理，不计自然退出。指定120秒源保留，无抓包、临时录制或远程产物。


### r42 实际执行命令

RTP H.264/AAC → MPEG-TS/RTP HEVC CBR8Mbps、1280×720/30fps，AAC CBR192kbps/44100Hz双声道。固定120秒源，分别直接执行：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-source-correction-r42 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-correction-r42.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-correction-r42-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-correction-r42-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-source-correction-r42-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-source-correction-r42- --snapshot-format=png rtp://@127.0.0.1:62020
```
