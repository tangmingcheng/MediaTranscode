# rk428 Windows 失败收尾

源35516、VLC17500、父31467。源完整3600帧119.90秒自然结束；VLC以RC quit退出，父exit0。本轮停止门禁由远端SIGTERM触发，错误见rk428-validation；未取快照，不宣称画面新门禁通过。本机FFmpeg未修改。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk428-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk428-native- --snapshot-format=png udp://@:62024
```
源日志为composition-rk428-source3.out/.log。远端停止错误及最终报告见rk428-validation。按精确清单清理out/acceptance下composition-rk428-source3.out、source3.log、vlc.log（同前缀），指定源保留。
