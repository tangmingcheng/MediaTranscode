# rk522 四源复验

状态：执行中，未通过。源码39e292a2，CLI SHA256 ec59f553d028f4875af028646d1663adbd30be569273e2400354113f0974a9bc。

## 实际命令

目标机通过SSH执行 `nohup bash acceptance/rk522/run.sh >acceptance/rk522/runner.log 2>&1 &`，脚本如下。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp-cancel-eos-20261118/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk522
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk522 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk522/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
```

Windows直接命令：

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk522-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk522-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk522-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk522-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk522-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk522-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk522-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=300 --scene-prefix=composition-rk522-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
```

## 运行身份与中间观察

目标 capture=1357390，runner=1357391，源0/1=1357393/1357394，CLI=1357395，monitor=1357397；Windows 源2/3=28636/22424，VLC=15436。原生快照00301已人工确认四格。运行96秒时CLI仍存活，workerErrors=0、errors=0；此项不是完整验收结论。

## 已确认结果：重入阶段失败

首轮Windows源2/3退出码均0；目标源0/1退出码均0。源2于10:42:31.896关闭，视频/音频分别10:42:32.096、10:42:33.824封闭，frontier3625/5193。10:42:48.867音频清除时候选0、样本0、committed_real_samples=5271241；10:42:49.840释放cancelled=0。此为有序排空证据，不替代内容完整性。

重入目标源PID1375206/1375207，Windows源3912/18496。CLI自行退出1：Datagram GBRA reservation misses its immutable completion deadline，sequence=123011，now_ns=173088028454，release_ns=173021714286，deadline_ns=173088378014，not_before_ns=173088225580，not_after_ns=173088161054，duration_ns=216960，rate=6250000，queue_wire_bytes=24408，average_queue_residence_ns=22672790，previous_submitted_sequence=123010，last_observed_ns=173088008620，theoretical_arrival_ns=173088225580。允许启动窗口相差64526ns；该算术不证明根因。

最终workerErrors/errors=2/2，workers=0，payload=0，reservations/releases=174440/174440，pressureFailures=8，RSS112513024→134406144，平均整机口径CPU17.250562%。AAC关闭时报告2 frames left in queue，保留失败边界。抓包329263 captured、329450 received by filter、kernel drop0；差额原因未确认。

失败后目标重入源按精确PID TERM，退出255/255；抓包SIGINT退出0；Windows重入源已清理，VLC通过RC quit退出。重入未满120秒，不判定通过。内容诊断和最终产物清理待完成。

## 发送批次诊断

失败包为media，original_protocol_deadline与prefix_completion_limit均173088378014；materialized=173065355664，planning_committed_sequence=122943，observed=173065061367，projected_available=173065192576，activation=173066204720..173066214053。active batch122944..123028，成功至123010，first/last submit=173066290763/173088008620；66次等待，target lateness总计3492146ns、最大1442942ns，最大出现在123006。上一批122859..122943的延迟已进入planning snapshot，不重复计入。

按67个同尺寸包216960ns服务计算，纯序列化14536320ns，projected_available至TAT实际23033004ns，差8496684ns；该差额包含激活、等待、提交等执行开销，尚不能全归因于单一环节。发布剩余窗口23185438ns，当前证据不支持静态容量必然不足。独立审查认为拒发检查正确；下一步核对完整服务递推，禁止清债、加grace或提高速率绕过。Windows重入源清理退出码均-1。

## 离线音频内容证据

复用rk520-offline-evidence.md四段直接Python命令：run前缀改rk522，首代音频SSRC1758327284、视频3433254378、输出1056999722；长窗口105改118秒，局部检查改116/117/118秒（与长窗口重叠，不称独立留出）。协议解析329263包，首代3600视频AU/5168音频AU，各SSRC缺口0，输出TS连续计数错误0、TEI0。PCM reference/input/output样本5292000/5292032/6478848，PTS缺口0。输入去掉1024 priming样本后与原源5291008样本逐值相等；原源尾992样本未送入，不能归为引擎丢失。

输出1/10/30/60/90/100/105/110/115秒局部最大相关均偏移19767样本（448.231293ms），118秒长窗口相关0.9987981124，次峰0.06953933394；116/117/118秒局部相关0.9988732624/0.9996822676/0.9981133399，偏移一致。说明此前约110秒尾部提前静音本轮未出现，最终不足一秒及内容A/V仍未闭合。源SR音视频偏置287.77097485..287.79138339ms，不可把448ms直接当引擎漂移。

完整输出解码退出0但有H264 MB62,19 bytestream -46/corrupt decoded frame警告（161 bytes），未定位AU，不能宣称干净解码。视频/音频包4408/6327，各DTS非递增0。

## 音频末段与解码警告定位

按已测19767样本偏移，输出[5247900,5271241)对应原源至5291008样本（119.977505669秒），23341样本相关0.9994952188，RMS0.2139148151。结束位置5271241与运行时committed_real_samples精确一致。末个非零样本5271487（119.534852608秒），预期末端后1秒RMS0.0001390566；AAC尾部振铃不当作额外真实媒体。此证据支持实际送入的首代音频尾部已输出，最终源文件992样本未发送边界仍保留。

实际执行FFmpeg命令：D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -v info -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk522-62024-1056999722.ts -map 0:v:0 -vf showinfo -an -f null -，stderr记录frame-decode.log，退出0。唯一损坏警告出现在最后数帧n4405..4407（146.833333..146.9秒）附近；警告与失败截断相邻，但异步解码日志顺序不能单独锁定具体AU或证明根因。

## 目标发送能力实测

只读命令uname -r、ethtool -i eth0、ethtool -T eth0、zgrep内核配置、tc -s -d qdisc show dev eth0：内核5.10.110-g1cf46ce40714-dirty；CONFIG_NET_SCH_ETF未启用，FQ/FQ_CODEL/PTP_1588_CLOCK启用。驱动st_gmac Jan_2016，PTP Hardware Clock0，支持软/硬收发timestamp；当前mq下pfifo_fast。timestamp不证明Launch Time能力，不能直接启用既有SO_TXTIME adapter并宣称严格定时出线。未修改系统qdisc或内核。

独立审查确认active batch额外8.496684ms中wait target lateness仅解释3.492146ms，剩余约5.004538ms尚包含初次接棒、提交和owner路径；不得全部归因OS等待。继续检查现有路径开销，保持原速率和deadline合同。
