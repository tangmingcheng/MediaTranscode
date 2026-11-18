# rk422 Windows 接收证据

本轮使用既有FFmpeg，未修改本机依赖。源PID9240、VLC27056、父会话29936。源完整3600帧、119.90秒自然结束；观测结束核exe清理VLC，父会话exit0，两个PID均不存在。VLC首次picture成功，启动有late警告；日志尾部read error不能据此宣称接收无错误。

实际源命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk422-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk422-native- --snapshot-format=png udp://@:62024
```

通过TCP127.0.0.1:63020发送snapshot换行取得VLC原生PNG，逐张查看1280x720：11h42m10s528四格有画；11h43m45s275右下黑、其他三格有画；11h44m18s455全黑。四路独立源启动时间不同，画面时间不一致不能直接归因为合屏漂移。此证据不证明恢复、静音或长时可靠性。

本轮精确清单（out/acceptance下）：composition-rk422-source3.out、composition-rk422-source3.log、composition-rk422-vlc.log，以及composition-rk422-native-2026-11-16-11h42m10s528.png、composition-rk422-native-2026-11-16-11h43m45s275.png、composition-rk422-native-2026-11-16-11h44m18s455.png。已记录结果，随后逐项删除；指定120秒源保留。
