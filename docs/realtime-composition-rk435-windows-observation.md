# rk435 Windows源与VLC观察

Windows HEVC源17876，VLC36736，父92648。源码/本机FFmpeg未改动，复用现有工具。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk435-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk435-native- --snapshot-format=png udp://@:62024
```

向RC42120发送snapshot换行命令，人工查看composition-rk435-native-2026-11-16-13h15m50s484.png：1280×720四格均真实画面。源17876完整3600帧119.90秒结束。接管后第二张composition-rk435-native-2026-11-16-14h00m13s917.png已人工检查为全黑；此时远端仍编码。随后CLI正常caller_stop退出0。VLC将以RC quit关闭。清理清单为source3.out/source3.log/vlc.log/handoff.txt和上述两张PNG，均带composition-rk435-前缀，记录后精确删除，用户源保留。
