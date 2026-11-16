# rk433 Windows VLC观察

VLC13808，父会话17370。本轮Windows仅接收，FFmpeg未改动。

实际命令：
```text
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk433-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk433-native- --snapshot-format=png rtp://@:62024
```

向127.0.0.1:42120发送snapshot换行命令。已人工查看composition-rk433-native-2026-11-16-13h05m03s193.png：全幅1280×720实际画面。该观察证明本轮旧头消费者的输出能够被VLC解码显示，不代表所有旧API调用、长期可靠性或合屏验收完成。远端源完整120秒结束，消费者约130秒因运行时进度超时自行FAILED/exit1，未发送停止信号；本轮不能记整体通过。随后向42120发送quit关闭VLC。记录后精确删除composition-rk433-vlc.log与上述原生快照，保留指定源。
