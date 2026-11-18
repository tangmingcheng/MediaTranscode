# r500：Windows 双源 HEVC CBR（FAIL：重入阶段发送超期）

沿用r499 Release产物SHA256 1c0c0066283204c2e8359d3cb4425ae7324c40f03d968a40283556be84e72b6f；只运行realtime，未改FFmpeg。两个H264源，其中第二个含所选AAC；输出HEVC CBR8000kbps 1280×720/30、AAC192kbps MPEGTS/RTP。

## 实际命令

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r500-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r500 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r500-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"
~~~

## 现场

VLC36728，源0=35832、所选源1（日志名source2）=34476，CLI34868。后台monitor62274保留进程句柄监控CPU/RSS与退出。


第一轮两源完整3600帧，正常SourceDrained old1→2；VLC已查看左右布局及全黑。保持CLI34868重入，同命令仅日志source0/source2增加-reentry，PID33824/39048。generation2 drift11338ns、frequency/stretch0；重入原生快照已查看左右格恢复。

## 结果与独立审查

CLI34868在输出约256.9秒自然退出1（后台保留句柄的monitor62274已返回CLI_EXIT=1）；不是清理终止。四次源均完整120秒/3600帧。未完成第二次全黑及正常调用方停止，因此不是双源完整通过。

原始错误：scheduled datagram submit exceeded its original deadline generation=1 sequence=135113 endpoint=1 release_ns=256849841740 reserved_at_ns=256915994600 not_before_ns=256916207560 deadline_ns=256916286408 observed_at_ns=256916594800。

独立审查retired_control_review_b判定FAIL：publication27238覆盖135106..135125，规划时距deadline4.462568ms，物化时剩4.406968ms（较release晚62.254660ms）。失败包预约成功，可发送窗口78.848微秒；请求等待211.160微秒，Windows等待API实耗590微秒，发送前比最晚起点晚308.392微秒。活跃批七次等待累计4.105700ms，累计目标迟到2.630380ms；would_block=0。证据支持较晚物化后的短余量被等待延迟耗尽，不支持归因socket阻塞，也不能把不相关canvas峰值认定为同一AU根因。与rk495预约拒绝不同，不改变期限或降低规格。

最终workerErrors/errors=2/2，payload254424预约=释放且回零，pressureFailures=0，高水位10848386字节/715对象。22核整机口径进程平均CPU1.527234%、峰值8.433735%；RSS起193953792、末及峰236675072字节。generation2内部音频漂移11338ns、frequency/stretch0；不等于接收端内容同步证明。stderr还保留动态frame color-range提示及失败关闭时AAC队列剩2帧，不能据清理回零判完整通过。

## 清理清单

本轮16项：cli.log、cli-error.log、monitor.log、output.sdp、source0/source2的.log/.out及各-reentry.log/.out、vlc.log，前缀均composition-r500-；三张native快照分别13h23m59s391、13h25m24s631、13h26m18s735。记录后逐项删除，保留指定120秒源。仅剩VLC36728待按身份关闭；CLI及四源已自然结束。
清理完成：16项均已删除，六个精确PID均不存在；指定源保留。
