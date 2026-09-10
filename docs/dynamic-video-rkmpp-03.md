# RKMPP03：H264 RTP 720p30 8Mbps → HEVC/H264 MPEGTS/RTP 1080p25 CBR6Mbps（PASS）

2026-09-10。按用户明确的三项验收边界：发送无超契约突发、动态增删正常、VLC正常解码，本轮均PASS。固定120秒连续源，不降规格、不循环；源H264 1280×720 30fps 8,013,434bps，输出1920×1080 25fps CBR6Mbps GOP50。Windows先行证据见[范围复核](dynamic-video-acceptance-scope.md)。

生产冻结98742fd6，RK CLI SHA256 9b7789cd6b9713fe36571f7b9e5ac057ce51f0afa85a601ce263c433f073eb5f；源SHA256 6e0de760672a6b8386b59b22f824de442e2a5ef7594d7b872c6851b64cabca9b。运行时/proc/1490370/maps确认六个FFmpeg库均来自/home/tang/ffmpeg-ab1e61a/lib，MPP/RGA来自既有平台路径。依赖详情见[部署记录](dynamic-video-rkmpp-dependency-deployment.md)。

## 结果

| 验收项 | 实测 |
|---|---|
| 发送突发 | 发送端抓包87477个输出RTP/RTCP，111252040 IP字节，与共享scope账目相等；50Mbps服务曲线最大超额1356B，等于最大包。四路sender deadline/pressure/partial/ambiguous/WouldBlock均0，最终backlog0。 |
| 动态增删 | 1路13:48:12.433运行；2路20.456运行并复用group1；3路27.733运行并新建group2；1/2/3于33.751、40.020、41.530正常退役；全删后4路44.788运行并新建group3。控制序列退出0。 |
| VLC解码 | 四路默认D3D11VA，四张1920×1080游戏画面均查看；demux corrupted/discontinuities均0。RTP到达序号连续，TS TEI/AFC/连续性与expert error无命中。 |

输出RTP包数依次13041/11832/8093/54471，RTCP7/6/3/24；发送及Windows接收端包数和IP字节相等，各路RTP相邻序号严格+1。目标tcpdump总捕获193588包/drop0（包含输入），Windows捕获87477包/drop0。接收端同曲线超额11583B，说明经过网络/接收观测后存在聚集；不能把接收间隔当发送时刻，也不声称物理链路绝无突发。发送端核验限于出口软件抓包及sender契约，TX timestamp未跟踪，仍保留delivery_evidence=not_proven。

源3600帧/120秒自然退出0；CLI在源结束后按既有NoProgress策略退出1，4路13:50:17.018退役且保留原因，非人工停止CLI。最终workers/errors/workerErrors/droppedBuffers/pressure均0、stalledIntervals1。payload0字节/0对象，68294次申请释放相等，峰11924771B/15对象。CPU497样本，单核等效平均16.563967%、峰48%；工作集61714432→79978496B，峰79978496B。纯视频A/V漂移不适用。

VLC结束后两次分离统计一致：video decoded1038/928/642/4302、displayed504/450/306/2143、lost1/0/0/0。这些是VLC统计口径，不当唯一AU帧数；初始路一次22ms显示晚帧不等于码流解码失败，按用户边界不单独否决。截图转换候选失败后最终PNG成功、RC关闭read error和窗口关闭SetThumbNailClip错误均保留，不掩盖或算入生产解码失败。MPP退出仍打印cleaning misc group；本轮引擎引用已归零，不宣称驱动内部所有资源已由本次证明完全释放。

## 输出profile调查

直接从Windows接收到的MPEG-TS/RTP参数集读取：61624路H264 profile_idc=100，为High，非Baseline；61620/61622/61626路HEVC general_profile_idc=1，为Main。本轮未传--profile，不应从codec名称、CBR或无B帧猜测profile。profile映射依据[FFmpeg定义](https://ffmpeg.org/doxygen/8.0/defs_8h_source.html)。仅确认本次RKMPP输出，不外推所有平台默认值。

更正：通用编码planner已有profile/open contract能力，但本轮realtime CLI未暴露--profile；此前将通用CLI解析能力误写为实时CLI能力。RKMPP03实际未传profile，其参数集结论不变。后续用户授权的Beta profile扩展另行实现、构建和验收。首个tshark尝试用了不存在的h264.level_idc字段而失败，弃用；最终以已确认字段全量导出退出0，只统计端口开头的数据行，排除本机插件尾部提示。

## 实际命令

SSH连接：`D:/Code/Git/usr/bin/ssh.exe -tt root@192.168.130.229`。启用source /opt/mt-tools/mtenv.sh、mtenv on、ffenv on后，将PATH/LD_LIBRARY_PATH/PKG_CONFIG_PATH前置/home/tang/ffmpeg-ab1e61a对应目录。远程临时脚本通过SSH创建，以下为实际内容：

```bash
#!/usr/bin/env bash
base=/home/tang/dynamic-rk-03
mkfifo "$base.control"
exec 9<>"$base.control"
/usr/sbin/tcpdump -i any -n -s 0 -U -w "$base.pcap" 'udp portrange 60620-60621 or udp portrange 61620-61627' > "$base-capture.log" 2>&1 &
capture_pid=$!
/home/tang/dynamic-video-98742fd6/out/build/rk-release/media_transcode_realtime_video_cli --media-id dynamic-rk-03 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61620 --sdp "$base.sdp" --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio < "$base.control" > "$base-cli.log" 2>&1 &
cli_pid=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > "$base-source.log" 2>&1 &
source_pid=$!
printf 'SCRIPT_PID=%s CLI_PID=%s SOURCE_PID=%s CAPTURE_PID=%s\n' "$$" "$cli_pid" "$source_pid" "$capture_pid"
wait_state() {
  local id=$1 state=$2 line
  for ((attempt=0; attempt<400; ++attempt)); do
    line=$(grep "output_id=$id state=$state" "$base-cli.log" | tail -n 1)
    if [ -n "$line" ]; then
      printf '%s\n' "$line"
      [[ "$line" != *" error="* ]]
      return $?
    fi
    if ! kill -0 "$cli_pid" 2>/dev/null; then return 1; fi
    sleep 0.1
  done
  return 1
}
add_output() {
  printf 'add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port %s --sdp %s-%s.sdp --video-codec %s --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50\n' "$2" "$base" "$1" "$3" >&9
}
drive() {
  wait_state 1 running || return 1
  cat "/proc/$cli_pid/maps" > "$base-maps.log"
  sleep 6
  add_output add 61622 hevc
  wait_state 2 running || return 1
  sleep 6
  add_output third 61624 h264
  wait_state 3 running || return 1
  sleep 5
  printf 'remove 1\n' >&9
  wait_state 1 retired || return 1
  sleep 5
  printf 'remove 2\n' >&9
  wait_state 2 retired || return 1
  printf 'remove 3\n' >&9
  wait_state 3 retired || return 1
  sleep 2
  add_output readd 61626 hevc
  wait_state 4 running || return 1
}
drive
control_exit=$?
printf 'CONTROL_EXIT=%s\n' "$control_exit"
if [ "$control_exit" -ne 0 ]; then kill -INT "$source_pid"; fi
wait "$source_pid"
source_exit=$?
wait "$cli_pid"
cli_exit=$?
kill -INT "$capture_pid"
wait "$capture_pid"
capture_exit=$?
exec 9>&-
exec 9<&-
printf 'DONE SOURCE_EXIT=%s CLI_EXIT=%s CAPTURE_EXIT=%s CONTROL_EXIT=%s\n' "$source_exit" "$cli_exit" "$capture_exit" "$control_exit"
```

调用：`/usr/bin/bash /home/tang/dynamic-rk-03.sh > /home/tang/dynamic-rk-03-control.log 2>&1 &`。PID：脚本1490367、CLI1490370、源1490371、tcpdump1490369。源/CLI/capture/control退出分别0/1/0/0；本轮未触发失败分支停止源命令。

Windows命令（dumpcap8328，VLC28052/16632/30208/31600）：

```powershell
& 'D:/Wireshark/dumpcap.exe' -i 5 -q -f 'src host 192.168.130.229 and udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-03-received.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-03-initial- --snapshot-format=png --no-snapshot-preview rtp://@:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-03-vlc-initial.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-03-add- --snapshot-format=png --no-snapshot-preview rtp://@:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-03-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-03-third- --snapshot-format=png --no-snapshot-preview rtp://@:61624 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-03-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-03-readd- --snapshot-format=png --no-snapshot-preview rtp://@:61626 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-03-vlc-readd.log' -Encoding UTF8
```

源及CLI结束后才执行统计刷新和截图，媒体期间没有RC唤醒：

```powershell
$ErrorActionPreference='Stop'; foreach($port in @(62720,62722,62724,62726)){ $client=[Net.Sockets.TcpClient]::new('127.0.0.1',$port); try { $stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("atrack -1`r`n"); $stream.Write($bytes,0,$bytes.Length); Start-Sleep -Milliseconds 500; foreach($sample in 1,2){$bytes=[Text.Encoding]::ASCII.GetBytes("stats`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500;$buffer=[byte[]]::new(16384);$response='';while($stream.DataAvailable){$count=$stream.Read($buffer,0,$buffer.Length);$response+=[Text.Encoding]::UTF8.GetString($buffer,0,$count)};[pscustomobject]@{port=$port;sample=$sample;utc=[DateTime]::UtcNow.ToString('o');response=$response}|ConvertTo-Json -Compress|Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-03-final-stats.log' -Encoding UTF8}; $bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500 } finally {$client.Dispose()} }; Get-Content -Encoding UTF8 -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-03-final-stats.log'
```

抓包通过Ctrl+C正常结束；截图生成后对上述四个VLC PID调用CloseMainWindow。使用tshark离线导出frame.time_epoch、udp.dstport、ip.len、rtp.seq，用发送容量6250000B/s计算累计字节相对服务曲线的最大增量；参数集导出字段为h264.profile_idc、h265.general_profile_idc。未增加FFmpeg监控进程、WPR或调试器。

## 清理与交付边界

原始材料本机仅D盘、目标机仅/home/tang/dynamic-rk-03前缀；两位独立审查者windows_scope_review_a/b复算原始数据均PASS后，已删除本机32文件共599049372B、目标机12文件共462352252B及1个FIFO；两端本轮前缀和媒体/抓包进程残留0，固定源及构建产物保留。完整120秒覆盖既定三项，不外推多小时资源趋势、其他输入布局、硬件失响应或未来音视频多路同步。
