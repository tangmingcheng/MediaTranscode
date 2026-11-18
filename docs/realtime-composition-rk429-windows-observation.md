# rk429 Windows 源流与播放器记录

源37656、VLC42116、父会话21949。源完整3600帧119.90秒结束；VLC日志确认 Received first picture。本轮未取快照，不宣称新增画面门禁通过。本机FFmpeg未修改。RC quit首次受沙箱网络限制，提升权限后端口拒绝连接；核实VLC路径后按PID清理，不计自然退出。远端停止结果见rk429-validation。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk429-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk429-native- --snapshot-format=png udp://@:62024
```
源日志为composition-rk429-source3.out/.log。远端停止结果及最终报告见rk429-validation。按精确清单清理out/acceptance下composition-rk429-source3.out、source3.log、vlc.log（同前缀），指定源保留。
