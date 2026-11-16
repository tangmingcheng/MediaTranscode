# rk430 Windows 源流与VLC原生快照

源30672、初始VLC39132、父会话4968。初始VLC收到首帧，但RC63020绑定报Access denied；核路径后精确清理该播放器，按相同接收参数将RC改42120、日志改composition-rk430-vlc-receiver.log重启为37260（父45022），源与远端C会话未重启。

实际命令：
```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk430-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk430-native- --snapshot-format=png udp://@:62024
```

第二次VLC命令与上面相同，仅 --rc-host=127.0.0.1:42120 与 --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk430-vlc-receiver.log 两项改变。向42120发送 snapshot 换行命令，生成 composition-rk430-native-2026-11-16-12h54m13s924.png，已人工查看：1280×720左右双格均有真实画面，上下黑色留边。不能据此证明内容级音画同步。

源30672最终3600帧119.90秒完整结束。第二张原生快照composition-rk430-native-2026-11-16-12h54m59s181.png为左格真实画面、右格黑；第三张composition-rk430-native-2026-11-16-12h55m31s135.png全黑。两张均人工查看，远端同时确认全部失源后编码包继续增长。随后向42120发送quit正常关闭VLC。

清理清单：composition-rk430-source3.out、composition-rk430-source3.log、composition-rk430-vlc.log、composition-rk430-vlc-receiver.log及上述三张快照。记录完成后逐项清除，指定120秒源保留。本机FFmpeg未改动。
