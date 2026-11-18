# rk424 Windows 双路画面观察

源38512、VLC38016、父62571。源完整3600帧/119.90秒自然结束；观测完成核exe清理VLC，父62571 exit0，两PID均不存在。本机FFmpeg未改动。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk424-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk424-native- --snapshot-format=png udp://@:62024
```
源stdout/stderr分别重定向composition-rk424-source1.out/.log。

通过TCP127.0.0.1:63020发送snapshot换行取得VLC原生1280x720 PNG，逐张查看：12h00m33s183左右有画、上下留黑；12h02m21s267全黑；12h03m06s208仅左侧H264源恢复，右侧黑；12h05m05s330再次全黑。音频恢复仅有远端内部时间线证据，快照不证明可听音频或内容级同步。

精确临时清单：out/acceptance下composition-rk424-source1.out/.log、composition-rk424-vlc.log，以及composition-rk424-native-2026-11-16-12h00m33s183.png、12h02m21s267.png、12h03m06s208.png、12h05m05s330.png（后三项同前缀）。结果归档后逐项清理，保留指定120秒源。
