# rk427 Windows 失败收尾

源10628、VLC36336、父7663。远端发送期限错误发生于STOP之前；本机最后3251帧108.26秒，核exe中止源及VLC，父exit0，不记源完整运行或成功验收。本轮未取快照。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk427-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk427-native- --snapshot-format=png udp://@:62024
```
源日志为composition-rk427-source3.out/.log。远端首错及最终报告见rk427-validation。按精确清单清理out/acceptance下composition-rk427-source3.out、source3.log、vlc.log（同前缀），指定源保留。
