# rk425 Windows 三路画面观察

源32148、VLC39296、父28991。源完整3600帧/119.90秒自然结束；观测完成核exe清理VLC，父28991 exit0，两PID均不存在。本机FFmpeg未改动。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk425-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk425-native- --snapshot-format=png udp://@:62024
```
源stdout/stderr分别重定向composition-rk425-source2.out/.log。

通过TCP127.0.0.1:63020发送snapshot换行取得VLC原生1280x720 PNG，逐张查看：12h08m15s385三格有画右下黑；12h10m32s923全黑；12h11m21s534仅右上所选H264源恢复；12h13m24s593再次全黑。VLC日志有picture late 42ms告警，不能隐去；音频恢复有远端内部时间线证据，快照不证明可听音频或内容级同步。

精确临时清单：out/acceptance下composition-rk425-source2.out/.log、composition-rk425-vlc.log，以及composition-rk425-native-2026-11-16-12h08m15s385.png、12h10m32s923.png、12h11m21s534.png、12h13m24s593.png（后三项同前缀）。结果归档后逐项清理，保留指定120秒源。
