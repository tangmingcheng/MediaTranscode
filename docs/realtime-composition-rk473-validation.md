# rk473：四屏真实回归失败

状态：FAIL。源码73cde7f2，CLI SHA256=95a9f0d6e4e9e9e93a5f63efffcef66232a105892bd315421303ae27eeec227a。四源120秒1280×720/30fps，H264/H264/H264 CBR8200kbps+AAC/HEVC CBR8000kbps，输出H264 VBR8–12Mbps/AAC192kbps MPEG-TS/RTP。没有降低参数。

10:20:30.837首错：node43 HardwareTransferNode，InvalidArgument: Hardware transfer frame differs from its immutable preparation feed。此前source0/1 SequenceGap触发generation1→2，source1 sequencer和extractor已合法发布generation2/releaseIdentity2719；初始preparation未更新，Transfer误将初始身份用于重入。双审确认源码根因，非外部依赖。

输出在约35秒停止，CLI未自然退出。监控175个样本CPU最大79.5%，RSS最大127884KiB；失败后127372KiB，不证明长期稳定。失败前最后av_drift_trace raw_phase9070ns、filtered9071ns、frequency0ppm，不能代替全程A/V验收。sender committed11591、deadline_misses0、delivery_evidence=not_proven。VLC启动晚于本轮输出失败，snapshot命令未生成图片，未验证画面。

进程：远端launcher2888610，source0=2888617，source1=2888618，CLI2888619，monitor2888621；本机source2=35352、source3=7508均自然退出0；VLC34200。首轮GDB2895060已detach，但未加载executable无法解栈，不能推断死锁根因；第二轮GDB2908811显式file/sysroot重取栈。

## 实际命令

~~~bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk473
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk473 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk473/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s SOURCE1=%s CLI=%s\n' "$s0" "$s1" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt

~~~

启动：bash -n acceptance/rk473/run.sh && bash acceptance/rk473/run.sh >acceptance/rk473/launcher.log 2>&1 &

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk473-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk473-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk473-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk473-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk473-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk473-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

第二次GDB已detach并取得符号栈：main在MediaGraphThreadedExecutor::abort→MediaGraphWorker::join等待thread0x7f22fdcb40（LWP2890645，mt-n66）；该线程VideoDecodeNode::submitPendingPacket→avcodec_send_packet→FFmpeg内部→mpi_decode_get_frame→mpp_get_frame→mpp_list_wait_timed→pthread_cond_timedwait。证明退出等待落在RKMPP取帧调用，尚不证明其内部根因。GDB附加后数据不得用于原始实时性能结论。测试已失败且四源已结束，现按精确PID清理CLI，退出不得记为自然结束。

最终结果：SOURCE0/1/2/3均exit0；CLI精确PID清理exit137，RUN_FINISHED=1。远端七个本轮PID查询为空，本机源/VLC查询为空。无画面证据，不判四屏通过。

GDB实际命令：
~~~bash
/opt/mt-tools/arm-gnu-toolchain-15.2.rel1-aarch64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gdb -q -batch -ex 'set pagination off' -ex 'thread apply all bt 30' -ex 'info sharedlibrary' -ex detach -p 2888619 > acceptance/rk473/gdb.txt 2>&1 &
/opt/mt-tools/arm-gnu-toolchain-15.2.rel1-aarch64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gdb -q -batch -ex 'set sysroot /' -ex 'file /home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli' -ex 'attach 2888619' -ex 'set pagination off' -ex 'thread apply all bt 30' -ex 'info sharedlibrary' -ex detach > acceptance/rk473/gdb-symbols.txt 2>&1 &
kill -KILL 2888619
~~~
VLC经TCP 127.0.0.1:42120发送snapshot命令，无png产物；测试结束核对exe路径后Stop-Process -Id 34200。

清理清单：本机composition-rk473-source2.log/out、source3.log/out、vlc.log、rk473-evidence.tar、rk473-run.sh，以及rk473-evidence内cli-error.log、cli-maps.txt、cli.log、gdb.txt、launcher.log、monitor.log、output.sdp、pids.txt、run.sh、socket-monitor.log、source0.log/out、source1.log/out、udp-before.txt。远端acceptance/rk473内同名运行文件加result.txt、udp-after.txt、gdb-symbols.txt，共18项；另out/acceptance/rk473-run.sh、rk473-evidence.tar。均在归档本记录后逐项删除，保留用户源及正式构建。

修复：HardwareTransfer只在Initial未激活feed核preparation身份；Next已激活帧使用当前epoch与lineage精确仲裁，未发表重入仍等待，旧代仍取消。两个独立审查者复审SOURCE PASS，尚待复验。此前rk470源码审查遗漏initial-only边界，本次实流FAIL保留。

清理完成：本机22项、远端20项临时文件逐项删除，rk473残留查询为空；VLC清理后WaitForExit返回0，但不记自然播放结束。修复后rk474构建PID2924273，8核增量4/4退出0；CLI13106552字节，SHA256=c2d9527865c3e1dfd47ae6256c60da70553001c7662b5975d6bf3744ca77af2e。原CLI与源码保留目标out/rollback/rk474。rk474实流尚未启动。

rk474构建临时tar/log已删除，进程与文件残留查询为空；回退保留。
