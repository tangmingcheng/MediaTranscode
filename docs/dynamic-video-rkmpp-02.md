# RKMPP 02：RTP H264 720p30 8Mbps → MPEGTS over RTP HEVC 1080p25 CBR6Mbps（失败）

日期：2026-09-10。结论：FAIL，不作为成功验收提交。代码 a5b430a7（生产代码41bb17a1），目标全量构建退出0，CLI SHA256 f309078925a3fa72635ea499db11d22aae86da2867053dbf07771e9ca84deae5。源为固定120秒连续文件，SHA256 6e0de760672a6b8386b59b22f824de442e2a5ef7594d7b872c6851b64cabca9b。

09:33:33.335，首个 DRM 帧在共享源隔离 VideoFilter 节点10报错：`VideoFilterNode requires authoritative input frame time base`。共享 VideoDecode 在 VideoOnly 路径没有发布描述；Windows 后置 VideoTimestamp 曾补齐，因此必须修复共享边界并先回归 Windows。首包 pts/dts=3864998585，33389 bytes。尚未编码/发出输出媒体。

最终 workers=0，errors=2，encodedPackets=0；payload=8802 bytes/1 object，reservations=24/releases=23，峰值9067274 bytes/13 objects。CPU samples=0，初始/末工作集57851904 bytes；运行过短，不能评价CPU、内存趋势或播放/A/V漂移。随后出现 mpp_buffer_service_deinit cleaning misc group，未证明它与单包残留同源。

独立审查确认目标干净 d90e3a1 的 rkmpp_decode_close 没有释放私有 last_pkt，公共 ff_codec_close 也不先 flush；非空 last_pkt 的直接关闭存在真实依赖所有权缺陷。本轮8802对象具体归属仍未证明，不能称其已归零，也不能仅用 finalReport 早于 runtime reset 解释。依据：[固定版本关闭路径](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/rkmppdec.c)、[公共关闭路径](https://github.com/nyanmisaka/ffmpeg-rockchip/blob/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/avcodec.c)。

停止源流 PID3734807（SIGINT），未停止 CLI：source=1486 frames/49.53秒，退出255；CLI退出1，capture退出0，控制序列退出1。目标抓包43542 packets，内核丢包0；Windows接收抓包0 packets/drop0。另发现本轮 VLC 错绑127.0.0.1，不能接收发往192.168.96.122的数据；下轮绑定全部本地地址。该接收设置缺陷不解释本轮解码输出契约报错，但本轮不具备播放验收条件。

精确PID：远程脚本3734801、CLI3734806、源3734807、tcpdump3734805；Windows dumpcap7212、VLC11064/2424/21972/19628。已检查远程进程退出，Windows播放器关闭。原始抓包、日志、SDP、临时脚本/FIFO在记录证据后删除，保留固定源与构建产物。

## 实际执行

远程临时脚本调用：
```bash
/usr/bin/bash /home/tang/dynamic-rk-02.sh > /home/tang/dynamic-rk-02-control.log 2>&1 &
```

实际脚本内容（含 CLI、FFmpeg、控制及抓包命令）：
```bash
#!/usr/bin/env bash
base=/home/tang/dynamic-rk-02
mkfifo "$base.control"
exec 9<>"$base.control"
/usr/sbin/tcpdump -i any -n -s 0 -U -w "$base.pcap" 'udp portrange 60620-60621 or udp portrange 61620-61627' > "$base-capture.log" 2>&1 &
capture_pid=$!
/home/tang/dynamic-video-a5b430a7/out/build/rk-release/media_transcode_realtime_video_cli --media-id dynamic-rk-02 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61620 --sdp "$base.sdp" --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio < "$base.control" > "$base-cli.log" 2>&1 &
cli_pid=$!
/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > "$base-source.log" 2>&1 &
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

Windows 接收抓包：
```powershell
& 'D:/Wireshark/dumpcap.exe' -i 5 -q -f 'src host 192.168.130.229 and udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-02-received.pcapng'
```

实际 VLC 命令（本轮接收地址错误，按原样记录）：
```powershell
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-02-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-02-vlc.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-02-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-02-vlc-add.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-02-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-02-vlc-third.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-rk-02-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-02-vlc-readd.log' -Encoding UTF8
```
