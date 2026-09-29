# 合屏输出编码规划

## 设计范围

基线6ef4a6c9。encoder候选与能力探测已独立，但open合同、执行策略和prepared VBV补全仍在完整源planner中。合屏不能构造虚假源流调用这些逻辑。本轮抽取`MediaVideoOutputPlanner`，消费已选择encoder、权威输出尺寸/cadence、既有编码意图和rate-control请求；旧单源规范化先按原源事实解析尺寸/帧率/码率，再调用同一输出规划。

`MediaVideoOutputPlan`只保留encoder和输出执行策略；移除未消费的源相关encodingRequest。完整链复用该输出产品，不再重复声明同义字段。单源`MediaPipelinePlan.encodingRequest`继续作为探测前不可变请求凭证，VBV补全不得改写它。

## 行业依据与边界

[FFmpeg AVCodecContext](https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html)以输出尺寸、framerate/time_base、RC和编码设置定义encoder；[send/receive接口](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)接收原始帧，未要求源压缩流身份。复用仓库已有rate-control planner、open adapter和prepared readback，不新增算法、平台路径、默认帧率或经验容量。缺少权威输出尺寸/cadence直接在planner失败。

不新增线程、队列、后台工作或资源分配；所有权、背压、编码失败及原RKMPP lineage/abort策略不变。VBV从已有prepared emission精确按1000 bit/kbit换算，继续拒绝0、非整除或超int范围；缺失readback不伪造VBV。实际设备/帧池准备、跨源总准入和完整coordinator仍未接通，不能把纯规划标作合屏可运行。

## 验证

源码冻结后执行双独立审查、Release全量构建及同规格真实CLI回归，结果按实际执行补充。完整2–4源、纯视频源、持续黑场/静音、恢复及Windows→RKMPP门禁仍未完成。

## 本轮执行结果（2026-09-28）

两名未参与实现者对冻结5源码给出Standards/阶段Spec PASS，完整功能FAIL。首次Release全量到120秒截止后taskkill报告子进程3528无法终止，入口exit1；复查无编译残留。第二次全量configure/build exit0，646项构建图完成，两个CLI时间11:51:02；5源码hash与双审冻结一致。构建成功不代表合屏通过。

### r32：RTP H.264/AAC → MPEG-TS/RTP HEVC CBR 8Mbps、1280×720/30fps、AAC CBR 192kbps

实际命令（分别运行）：

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-output-planner-r32 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60970 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:60972 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 61970 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-planner-r32.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-planner-r32-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:60970?rtcpport=60971&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:60972?rtcpport=60973&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-planner-r32-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-planner-r32-vlc.log --extraintf=rc --rc-host=127.0.0.1:62970 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-output-planner-r32- --snapshot-format=png rtp://@127.0.0.1:61970
```

源PID12168/session46614自然exit0，3600帧/120.00秒；CLI PID32356/session12464自然exit1，未强制结束。VLC PID26328启动命令exit0，截图composition-output-planner-r32-2026-09-28-11h52m24s318.png实际检查有1280×720游戏画面；VLC出现画面迟到65/25ms和Failed to create video converter，画面不能证明完整通过。

11:53:48.584 purge_ack=complete、ack_count=7、old=1/next=2、acquiring；11:53:53.804 abort.begin，最后报realtime runtime made no progress before timeout，edge46/47各10/10。最终queued/workers/payloadBytes=0，逻辑对象4，reservations71940/releases71936，高水11983918bytes/83objects，workerErrors/errors/pressureFailures=0。完整链路FAIL，不形成成功验收提交。

426个CPU采样、22逻辑核：机器口径均值1.163925%/峰值4.326923%，单核口径25.606339%/95.192308%；WS初始106807296、最终189280256、峰值193912832bytes。后台5秒监测11:52:06 CPU4.796875s/WS191016960/Private399306752，11:53:37 CPU28.5s/WS193912832/Private401551360，监测session88171自然exit0。118条generation1漂移raw绝对最大156ns；没有恢复后连续媒体证据。sender提交86346datagrams/104843104payloadbytes，deadline/pressure/partial/ambiguous=0，pacing_cancelled=1，delivery_evidence=not_proven。

清理清单：本轮CLI/source/VLC三日志、SDP、上述PNG，共5项；先记录本节，再核对VLC身份按PID清理及逐项删除。本轮无抓包、无远程测试；固定120秒源保留。

### local-r32：文件H.264/AAC → HEVC CBR 8Mbps、1280×720/30fps、AAC CBR 192kbps

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_local_video_cli.exe --input D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 --output D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-planner-local-r32.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-output-planner-local-r32-cli.log 2>&1
exit $LASTEXITCODE
```

PID19748自然exit1，约0.97秒，在DAG构建前失败，无输出媒体，故未执行VLC播放。11:55:14.357 CUDA探测报opened encoder did not expose a valid retained-frame bound；11:55:14.953 QSV texture 80070057/child frame pool初始化失败，最终HardwareUnavailable。不能把规划拒绝解释为CUDA缩放滤镜不可用。

并行监测exit0：11:55:14.273 CPU0.125s/WS142766080/Private248385536；.599 CPU0.34375s/WS109592576/Private237195264；.929 CPU0.578125s/WS183578624/Private266174464。未进入媒体运行，无持续A/V或长期内存结论。仅生成1项CLI日志，先归档本段再逐项删除。实时5项清理已核实零残留；本轮未运行动态输出、RKMPP或多源矩阵，均不得标作通过。
