# rk423 Windows 源重入观察

初始源28568、VLC30672、父81162；重入源23416、父96389。两次源均完整3600帧/119.90秒，重入父明确EXIT=0；观测完成核exe清理VLC，父81162 exit0，三个PID均不存在。本机FFmpeg未改动。

初始实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk423-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk423-native- --snapshot-format=png udp://@:62024
```
重入使用同一FFmpeg命令及原120秒源，未loop；输出日志改为composition-rk423-reentry-source3.out/.log。同一VLC与远端CLI保持运行。

TCP127.0.0.1:63020发送snapshot换行取得VLC原生1280x720快照，已逐张查看：11h50m28s880四格有画；11h52m32s439全黑；11h53m36s850上两格黑、下两格source2和source3恢复；11h55m27s309再次全黑。此证据证明本轮画面恢复，不独立证明接收音频静音/响度及长期可靠性。generation2音频和时序证据见rk423-validation。

精确临时清单：out/acceptance下composition-rk423-source3.out/.log、composition-rk423-reentry-source3.out/.log、composition-rk423-vlc.log及上述四张composition-rk423-native-2026-11-16-时间.png。归档后逐项清理，保留指定源。
