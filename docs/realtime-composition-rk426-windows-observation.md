# rk426 VLC 接收音频内容观察

Windows源36092完整3600帧119.90秒；VLC15836，父2348。完整恢复源结束后RC quit封口，父exit0。源→CLI→VLC实时链路未加入FFmpeg监控，本机依赖不变。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk426-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk426-received.ts}}" --sout-keep udp://@:62024
D:/mabs/local64/bin-video/ffprobe.exe -v error -show_entries format=duration,start_time,size:stream=codec_name,width,height,r_frame_rate,sample_rate,channels -of json D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk426-received.ts
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk426-received.ts -map 0:a:0 -vn -af silencedetect=noise=-80dB:d=0.5 -f null -
D:/mabs/local64/bin-video/ffprobe.exe -v error -select_streams a:0 -show_packets -show_entries packet=dts_time -of csv=p=0 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk426-received.ts
```

probe及完整离线分析exit0。TS 247196876B，start0.400000s、duration308.799400s，HEVC1280x72030、AAC44100双声道。近静音114.580907至162.020748秒共47.439841秒；随后118.457347秒未检测到该阈值持续静音；280.478095至308.799410再次近静音。对应全失源→恢复→再次失源。重入内部canonical约162.017631354秒与音频恢复接近，但不是内容级A/V对齐证明。

保留异常：离线null输出2条nonmonotonic DTS告警；完整13302音频packet仅第45处dts从1.421667回退1.378578秒，位于启动段。VLC日志有read error: No error和SetThumbNailClip失败。VLC重封装文件不足以归因原始线端或引擎时间戳，也不能证明逐样本零静音、音质或长期稳定。

归档后精确清理out/acceptance下composition-rk426-source3.out、source3.log、vlc.log、received.ts、audio-analysis.log（均同composition-rk426-前缀）；保留指定120秒源。
