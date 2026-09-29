# 合屏共享元数据存储

基线e959afee；本轮将canonical retained identity、视频贡献、音频片段及编码后音频贡献迁移到共享只读存储。完整合屏仍FAIL、42/100；本记录不宣称完成元数据全局准入。

## 工业合同与实现

- [FFmpeg AVBuffer](https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html)分离存储与引用，最后引用回收且可明确只读。本实现复用这一所有权模式，用标准C++构造/析构非平凡元素，不把它们当作裸FFmpeg字节。
- [C++ allocator](https://eel.is/c++draft/allocator.members)的allocate(n)请求n个元素；[uninitialized_copy](https://eel.is/c++draft/uninitialized.copy)配合异常回滚。MediaImmutableArray只发布已完整构造的const数组，空数组不分配，检查乘法溢出；控制块创建失败也调用deleter。move后原owner为空且size归零。
- 元素payload为n*sizeof(T)，不保留传入vector的spare capacity。identity长度定界，嵌入NUL参与内容比较，不分配终止字节，不提供C字符串接口。派生lineage、source stamp与contribution共享identity，MediaAudioLineageKey也持owner，purge不使其悬空。

## 生产接线与边界

共同factory覆盖source canonical→decode/trim/resample→audio encode→encoded canonicalizer，以及aggregate音视频输出；VideoLineageDerivation共享不可变视频贡献。只读消费者使用span，AudioEncodeNode为既有mutable队列显式复制scratch。保留所有原stream、sequence、generation、timeline及连续样本校验，不另建媒体链、线程或平台实现。

Aggregate metadata guard改计真实元素payload及identity backing，仍保守按引用重复计共享owner；原lineage/buffer/source/pending等sizeof项目保留。字符串已无终止存储，因此不虚计+1。这不是allocation前全局credit，也没有放开FinalCompiler的aggregate门禁。

仍待完成：逐源prepared replay与startup同源事务；最大存活owner与分配前RAII配额；scratch vector、accumulator/deque及pending generation容量产品；allocator bookkeeping/shared_ptr控制块、驱动等范围说明；统一owner总账与已批准公共多源入口。当前identity每次创建lineage分配一次，尚非prepared源级interning。精确元素存储增加一次从scratch冻结的复制，其峰值与CPU需真实链路观测；不能靠size宣称整个进程内存硬界。

## 验证

18源码双独立审查均明确阶段PASS；完整准入/合屏仍FAIL42。24文件UTF-8无BOM/CRLF、diff检查通过。Release全量session54983成功（665项构建图），configure/build exit0；realtime CLI 2026-09-29 10:14:07、5155328bytes。未运行local CLI。

## r49实际realtime复验

输入RTP H.264/AAC → MPEG-TS/RTP HEVC CBR 8Mbps、1280×720/30fps；AAC CBR 192kbps、44100Hz双声道。沿用120秒原源，无降规格、循环或额外FFmpeg监控。CLI PID30224/session47281，源PID32232/session48406，VLC PID31884/启动exit0，外部5秒监控session23479。

```powershell
& D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-metadata-storage-r49 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-metadata-storage-r49.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-metadata-storage-r49-cli.log 2>&1
exit $LASTEXITCODE

& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200" > D:/Code/MyCode/MediaTranscode/out/acceptance/composition-metadata-storage-r49-source.log 2>&1
exit $LASTEXITCODE

& D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-metadata-storage-r49-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-metadata-storage-r49- --snapshot-format=png rtp://@127.0.0.1:62020
```

### 结果：FAIL

源3600帧/120.00秒自然exit0；CLI自然exit1/no-progress。10:17:18.932 purge7ack complete、old1/next2 acquiring；10:17:24.210 abort.begin、24.248 abort.done。edge46/47均10/10；abort前20queued/24objects/238665bytes，final queued/workers/payloadBytes=0、4逻辑对象（reservations71850/releases71846）；高水12102909bytes/88objects，workerErrors/errors/pressureFailures=0、stalledIntervals=1，AAC关闭时2帧残留。

413CPU采样/22核：进程整机口径均值1.277347%、峰4.978355%；单核均值28.101624%、峰109.523810%。runtime WS初始106532864、最终189693952、峰194220032bytes。外部5秒监控19样本exit0，10:15:51.776至10:17:22.077；CPU累计10.421875→34.96875s，WS193814528→189693952、峰194646016，Private404836352→401879040、峰405393408bytes。观察到的短时峰值不等于完整内存硬界或性能优化证明。

118条generation1 raw/filtered漂移绝对最大156ns，不能证明恢复。sender86254datagrams/104731644payloadbytes，deadline/pressure/partial/ambiguous均0，pacingCancelled1/backlogCancelled6、delivery_evidence=not_proven。VLC截图composition-metadata-storage-r49-2026-09-29-10h16m02s964.png已实际查看1280×720游戏画面；VLC日志存在buffer deadlock prevented、playback too late92683及Failed to create video converter，不能把截图当作持续无丢帧播放证明。

这次运行经过修改后的共同source/音频retained存储路径，未出现新metadata分配/校验报错；未直接运行多源aggregate，也未注入内存分配失败。原退出和AAC重入门禁仍失败，不计成功验收提交，Windows→RKMPP完整矩阵尚未通过。

### 清理清单

先记录命令、指标与失败，再按身份核验并清理VLC31884；该清理不记自然结束。CLI30224与源32232已经自然结束。删除本轮精确5文件：composition-metadata-storage-r49-cli.log、-source.log、-vlc.log、composition-metadata-storage-r49.sdp及上述PNG。未创建远程、抓包、临时录制或测试脚本；保留124427809bytes指定源及构建产物。清理复核完成：5文件与3个媒体PID均无残留，指定源大小确认保留。
