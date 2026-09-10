# RKMPP Profile01：H264 RTP 720p30 8Mbps → HEVC/H264 MPEGTS/RTP 1080p25 CBR6Mbps（PASS）

2026-09-10。使用最新Beta库和清晰版C示例走生产DAG，固定120秒连续源，不循环、不降规格。三项门禁发送无超契约突发、动态增删正常、VLC正常解码全部通过；新增profile的实际参数集核验通过。源码/包来源和哈希见[交付记录](dynamic-video-beta-package.md)。core543f8565，example f41a6554；后者未修改库源码及头文件。此轮独立成功提交，双审原始证据和清理随后补记。

源H264 1280×720 30fps 8,013,434bps，SHA256 6e0de760672a6b8386b59b22f824de442e2a5ef7594d7b872c6851b64cabca9b；所有输出1920×1080 25fps CBR6Mbps GOP50，出口容量50Mbps、最大wire residence100ms。按用户要求profile首先验收RKMPP，不宣称Windows新版profile已验收。

## 实测结果

| 路径 | 请求与实际参数集 | 动态结果 |
|---|---|---|
| 6200/6201 | HEVC main，PTL profile_idc=1，8bit | ID1运行14:47:20.360；40.546退役。 |
| 6202/6203 | HEVC main，PTL profile_idc=1，8bit | ID2运行28.475，复用group1；46.084退役。 |
| 6204/6205 | H264 baseline，SPS66、constraint_set0/1均1、PPS entropy_coding_mode=0，即Constrained Baseline/CAVLC | ID3运行34.902，新建group2；46.618退役。 |
| 6208/6209 | HEVC baseline不受支持 | ID4在40.121准备阶段Failed，Invalid argument；无该端口包，其他输出继续。 |
| 6206/6207 | H264 high，SPS100、constraint_set0/1均0、PPS entropy_coding_mode=1，即High/CABAC | 全删后ID5在49.155运行，新建group3。 |

上述状态时间来自脚本发现事件的时间，核心事件/组记录保留于日志。profile含义参照[FFmpeg定义](https://ffmpeg.org/doxygen/8.0/defs_8h_source.html)及目标版本[RKMPP编码选项](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/rkmppenc.h)；通过复用既有AVOption/open contract能力实施，没有新编码算法、fallback、线程或队列策略。

发送软件出口及Windows接收各87657包、111371608 IP字节，与共享scope账目相等；RTP依次12463/11075/7199/56881，RTCP6/5/4/24，每路序号严格+1。发送50Mbps累计服务曲线最大超额1356B，等于最大包；接收15170.375B，保留网络/接收聚集事实，不当作发送时刻。四路sender WouldBlock/deadline/pressure/partial/ambiguous计数均0，最终backlog0。最后一条sender最大submit lateness100.437ms为原始事实，deadline_misses仍0；不将该相对release指标混为deadline失败或掩盖。

tcpdump捕获194352包（含输入）、kernel drop0；Windows dumpcap87657包/drop0，Ctrl+C退出1，计数完整。TS TEI/AFC/连续性及expert error过滤无数据行。tshark四次离线导出均退出0，排除本机插件尾行“No matching RTP/H.264 payload dumped.”，不把它算作包或错误。

四路VLC均默认D3D11VA，四张1920×1080游戏画面已逐张查看。结束后两次RC统计一致：decoded988/858/562/4504，displayed479/415/267/2238，lost1/0/0/0，demux corrupted/discontinuities均0。初始路21ms显示晚帧、其他debug late保留，按用户三项边界不独立否决；VLC计数不当作唯一AU数。截图阶段两路converter候选失败后PNG成功；RC连接关闭及窗口关闭错误属于观察工具退出记录。日志前置UTC为读取时间，不据此精确定位媒体事件时间。

源3600帧/120秒自然退出0；示例在源结束后NoProgress退出1，control/capture退出0，未人工停止示例。最终snapshot state5/completion5、queued0、drops0、errors0、encoded13642/13642。运行中134条snapshot：RSS68915200→84774912B、峰84774912B，后段保持；结束累计单核等效CPU15.426%。纯视频A/V漂移不适用。没有从未暴露的Beta快照推断payload计账归零。MPP仍有cleaning misc group提示，未据本轮证明驱动全部资源释放。

## 实际命令与进程

SSH使用D:/Code/Git/usr/bin/ssh.exe -tt root@192.168.130.229；先source /opt/mt-tools/mtenv.sh、mtenv on、ffenv on，将PATH/LD_LIBRARY_PATH/PKG_CONFIG_PATH前置/home/tang/ffmpeg-ab1e61a对应目录。实际进程maps再次确认该FFmpeg前缀。

远程调用：`/usr/bin/bash /home/tang/beta-profile-rk-01.sh /home/tang/packages/media-transcode-beta-rkmpp-f41a6554-20260910 > /home/tang/beta-profile-rk-01-control.log 2>&1 &`。PID：script1958034、example1958037、source1958038、tcpdump1958036。脚本内容：

```bash
#!/usr/bin/env bash
base=/home/tang/beta-profile-rk-01
mkfifo "$base.control" || exit 1
exec 9<>"$base.control"
/usr/sbin/tcpdump -i any -n -s 0 -U -w "$base.pcap" 'udp portrange 61884-61885 or udp portrange 6200-6209' > "$base-capture.log" 2>&1 &
capture_pid=$!
"$1/bin/beta_minimal" < "$base.control" > "$base-example.log" 2>&1 &
example_pid=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://192.168.130.229:61884?rtcpport=61885&pkt_size=1200' > "$base-source.log" 2>&1 &
source_pid=$!
printf 'SCRIPT_PID=%s EXAMPLE_PID=%s SOURCE_PID=%s CAPTURE_PID=%s\n' "$$" "$example_pid" "$source_pid" "$capture_pid"
wait_state() {
  local id=$1 state=$2 line
  for ((attempt=0; attempt<400; ++attempt)); do
    line=$(grep "output_id=$id output_state=$state" "$base-example.log" | tail -n 1)
    if [ -n "$line" ]; then printf '%s %s\n' "$(date -Ins)" "$line"; return 0; fi
    if ! kill -0 "$example_pid" 2>/dev/null; then return 1; fi
    sleep 0.1
  done
  return 1
}
drive() {
  wait_state 1 3 || return 1
  cat "/proc/$example_pid/maps" > "$base-maps.log"
  printf 'list\n' >&9
  sleep 6
  printf 'add hevc 6202 main\n' >&9
  wait_state 2 3 || return 1
  sleep 6
  printf 'add h264 6204 baseline\n' >&9
  wait_state 3 3 || return 1
  sleep 5
  printf 'add hevc 6208 baseline\n' >&9
  wait_state 4 6 || return 1
  printf 'list\nremove 1\n' >&9
  wait_state 1 5 || return 1
  sleep 5
  printf 'remove 2\n' >&9
  wait_state 2 5 || return 1
  printf 'remove 3\n' >&9
  wait_state 3 5 || return 1
  sleep 2
  printf 'add h264 6206 high\n' >&9
  wait_state 5 3 || return 1
  printf 'list\n' >&9
}
drive
control_exit=$?
printf 'CONTROL_EXIT=%s\n' "$control_exit"
if [ "$control_exit" -ne 0 ]; then kill -INT "$source_pid"; fi
wait "$source_pid"
source_exit=$?
wait "$example_pid"
example_exit=$?
kill -INT "$capture_pid"
wait "$capture_pid"
capture_exit=$?
exec 9>&-
exec 9<&-
printf 'DONE SOURCE_EXIT=%s EXAMPLE_EXIT=%s CAPTURE_EXIT=%s CONTROL_EXIT=%s\n' "$source_exit" "$example_exit" "$capture_exit" "$control_exit"
```

Windows实际命令（VLC PID4940/14908/6516/25088；dumpcap15560）：

```powershell
& 'D:/Wireshark/dumpcap.exe' -i 5 -q -f 'src host 192.168.130.229 and udp portrange 6200-6209' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-received.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=beta-profile-rk-01-initial- --snapshot-format=png --no-snapshot-preview rtp://@:6200 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-vlc-initial.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=beta-profile-rk-01-add- --snapshot-format=png --no-snapshot-preview rtp://@:6202 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=beta-profile-rk-01-third- --snapshot-format=png --no-snapshot-preview rtp://@:6204 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=beta-profile-rk-01-readd- --snapshot-format=png --no-snapshot-preview rtp://@:6206 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-vlc-readd.log' -Encoding UTF8
```

源和示例结束后才执行RC统计及截图，媒体期间无RC唤醒：

```powershell
$ErrorActionPreference='Stop'; foreach($port in @(62720,62722,62724,62726)){ $client=[Net.Sockets.TcpClient]::new('127.0.0.1',$port); try { $stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("atrack -1`r`n"); $stream.Write($bytes,0,$bytes.Length); Start-Sleep -Milliseconds 500; foreach($sample in 1,2){$bytes=[Text.Encoding]::ASCII.GetBytes("stats`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500;$buffer=[byte[]]::new(16384);$response='';while($stream.DataAvailable){$count=$stream.Read($buffer,0,$buffer.Length);$response+=[Text.Encoding]::UTF8.GetString($buffer,0,$count)};[pscustomobject]@{port=$port;sample=$sample;utc=[DateTime]::UtcNow.ToString('o');response=$response}|ConvertTo-Json -Compress|Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-final-stats.log' -Encoding UTF8}; $bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n");$stream.Write($bytes,0,$bytes.Length);Start-Sleep -Milliseconds 500 } finally {$client.Dispose()} }; Get-Content -Encoding UTF8 -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-final-stats.log'
```

截图生成后对四个精确VLC PID调用CloseMainWindow。离线分析命令：

```powershell
& 'D:/Wireshark/tshark.exe' -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01.pcap' -d udp.port==6200,rtp -d udp.port==6202,rtp -d udp.port==6204,rtp -d udp.port==6206,rtp -Y 'udp.dstport >= 6200 and udp.dstport <= 6209' -T fields -e frame.time_epoch -e udp.dstport -e ip.len -e rtp.seq | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-send-wire.tsv' -Encoding ascii
& 'D:/Wireshark/tshark.exe' -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-received.pcapng' -d udp.port==6200,rtp -d udp.port==6202,rtp -d udp.port==6204,rtp -d udp.port==6206,rtp -Y 'udp.dstport >= 6200 and udp.dstport <= 6209' -T fields -e frame.time_epoch -e udp.dstport -e ip.len -e rtp.seq | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-receive-wire.tsv' -Encoding ascii
& 'D:/Wireshark/tshark.exe' -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-received.pcapng' -d udp.port==6200,rtp -d udp.port==6202,rtp -d udp.port==6204,rtp -d udp.port==6206,rtp -Y 'h264.profile_idc or h265.general_profile_idc or h264.entropy_coding_mode_flag' -T fields -e udp.dstport -e h264.profile_idc -e h264.constraint_set0_flag -e h264.constraint_set1_flag -e h264.entropy_coding_mode_flag -e h265.general_profile_idc -e h265.bit_depth_luma_minus8 -e h265.bit_depth_chroma_minus8 | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-profiles.tsv' -Encoding ascii
& 'D:/Wireshark/tshark.exe' -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-received.pcapng' -d udp.port==6200,rtp -d udp.port==6202,rtp -d udp.port==6204,rtp -d udp.port==6206,rtp -Y 'mp2t.tei == 1 or mp2t.afc == 0 or mp2t.cc.drop or mp2t.afc.invalid or _ws.expert.severity == 8388608' -T fields -e frame.number -e _ws.expert.message | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/beta-profile-rk-01-ts-errors.tsv' -Encoding ascii
```

曲线计算采用6250000B/s：逐包取累计字节减去相对首包时间的容量乘积，最大值相对此前发送前最小值的增量即超额；使用decimal时间计算。未加入FFmpeg监控、WPR或调试器。

## 交付边界与清理

仅覆盖本次RKMPP的显式main/baseline/high及拒绝错误组合，不扩展为所有后端/输入布局/多小时保证。物理TX时间未跟踪，delivery_evidence=not_proven；长期资源、驱动释放以及未来音视频同步仍属后续。

待两位独立审查者完成原始材料复算后，删除D盘及/home/tang本轮前缀、传输归档、构建日志和临时对象，并记录进程/文件残留；固定源、源码构建目录及交付包保留。

