# r375：三源 RTP 合屏实际绑定回归

日期：2026-11-02。当前结果：四阶段画面与停止观测完成，完整验收仍未通过。

本轮使用 Input/Protocol 实际绑定及 sender 最迟提交序号诊断修改后的 Release 全量重建产物（配置/构建退出码均 0）。该范围已双独立局部审查 PASS，完整早期准入和 SourcePresentation 路径仍未完成。

## 实际启动命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r375 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r375-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
```

进程：source0=34916，source1=30936，source2=31896，CLI=17672，VLC=17628。启动前同执行环境 CIM 无残留，五个 UDP 输入/输出端口与 RC 端口空闲；启动后 UDP62024/TCP63020 均归本轮 VLC17628。

## 运行观察

- 已查看 composition-r375-native-2026-11-02-10h20m09s285.png：三格正常游戏画面，右下黑格。本轮 RC snapshot 命令成功发出，但其随后读取响应的 PowerShell 字节数组写法出错；文件实际生成并已查看，后续修正读取表达式，不影响媒体链路。

- 已查看 composition-r375-native-2026-11-02-10h21m49s929.png：三初始源结束后全黑，CLI仍持续输出。
- 重入源PID33244，实际命令：

```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```

- 已查看 composition-r375-native-2026-11-02-10h22m39s945.png：仅左下所选源恢复游戏画面，其余三格黑。尚不能凭画面证明音频内容对齐或静音。

VLC原生快照命令（修正后的实际执行）：
```powershell
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
```

- 已查看 composition-r375-native-2026-11-02-10h24m12s696.png：重入结束后再次全黑。
- 三初始源PID34916/30936/31896及重入PID33244均完整120秒、3600帧、退出0；保留启动句柄取得退出码。CLI PID17672退出0，end_reason=caller_stop，并非全失源自然退出。
- 同启动环境CIM核验全部六个本轮PID不存在，UDP62024与TCP63020均已释放。
- 最终errors、workerErrors、stalledIntervals及pressure为0；226023次预约/释放一致，current bytes/objects为0。平均整机进程CPU1.688225%，峰7.466063%；WS初始205099008、峰265453568、最终257286144 bytes。该短时趋势不能证明长期内存稳定。
- sender提交175637个datagram、203802716 payload bytes，deadline/partial/ambiguous失败0；最迟成功提交32.203532ms，对应global sequence145047，backlog最大停留69.4452ms；delivery_evidence=not_proven。原发送/调度期限失败未因本轮未复现判为修复。
- 已验证本版本三格→黑屏→所选源重入→黑屏和调用方停止；未证明内容级A/V、静音、发送交付、完整2～4源/C API/RKMPP矩阵及新SourcePresentation路径，故不是完整验收PASS。

## 实际调用方停止与VLC退出命令

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=17672"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r375*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R375Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R375Console]::FreeConsole() | Out-Null
if(-not [R375Console]::AttachConsole(17672)){throw 'AttachConsole failed'}
try {
[R375Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R375Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 17672 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R375Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R375Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 17628){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

## 非周期CLI诊断及完整stderr

~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 10:19:30.732] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-02 10:19:30.733] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-02 10:19:30.733] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-02 10:19:30.885] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:19:30.888] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:19:30.889] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r375.source.0 decoder=h264
[2026-11-02 10:19:34.138] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:19:34.154] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:19:34.154] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:19:34.245] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:19:34.245] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:19:34.245] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r375.source.1 decoder=h264
[2026-11-02 10:19:34.300] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:19:34.300] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:19:34.301] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r375.source.2 decoder=h264
[2026-11-02 10:19:34.397] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:19:34.449] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:19:34.449] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-02 10:19:34.457] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:19:34.458] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:19:34.458] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:19:34.464] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:19:34.464] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:19:34.464] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:19:34.473] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:19:34.473] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:19:34.473] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:19:34.519] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=829317192 high_water=855443527
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-02 10:19:34.525] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-02 10:19:34.526] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.527] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.528] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.528] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.528] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.528] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.528] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.528] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.528] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.529] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.530] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.531] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.532] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.533] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.533] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.533] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.533] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.533] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.534] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.535] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.536] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:19:34.537] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-02 10:19:34.538] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:19:34.538] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:19:34.538] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:19:34.538] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:19:34.538] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-02 10:19:34.538] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-02 10:19:34.539] [info] [graph][runtime.node] register node=1 name=composition-r375.output.video.codec_resolver kind=CodecResolver
[2026-11-02 10:19:34.539] [info] [graph][runtime.node] register node=2 name=composition-r375.output.video.encode kind=VideoEncode
[2026-11-02 10:19:34.539] [info] [graph][runtime.node] register node=3 name=composition-r375.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:19:34.539] [info] [graph][runtime.node] register node=4 name=composition-r375.output.audio.encode kind=AudioEncode
[2026-11-02 10:19:34.539] [info] [graph][runtime.node] register node=5 name=composition-r375.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-02 10:19:34.539] [info] [graph][runtime.node] register node=8 name=composition-r375.source_0.input kind=RawRtpInput
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=9 name=composition-r375.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=10 name=composition-r375.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=11 name=composition-r375.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=12 name=composition-r375.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=13 name=composition-r375.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=14 name=composition-r375.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=15 name=composition-r375.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=16 name=composition-r375.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=17 name=composition-r375.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:19:34.540] [info] [graph][runtime.node] register node=18 name=composition-r375.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=20 name=composition-r375.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=21 name=composition-r375.source_0.video.codec_resolver kind=CodecResolver
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=22 name=composition-r375.source_0.video.decode kind=VideoDecode
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=23 name=composition-r375.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=24 name=composition-r375.source_0.video.framerate kind=VideoFrameRate
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=25 name=composition-r375.source_0.video.preparation kind=VideoPreparation
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=26 name=composition-r375.source_0.video.filter kind=VideoFilter
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=27 name=composition-r375.source_1.input kind=RawRtpInput
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=28 name=composition-r375.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:19:34.541] [info] [graph][runtime.node] register node=29 name=composition-r375.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=30 name=composition-r375.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=31 name=composition-r375.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=32 name=composition-r375.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=33 name=composition-r375.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=34 name=composition-r375.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=35 name=composition-r375.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=36 name=composition-r375.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=37 name=composition-r375.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=39 name=composition-r375.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=40 name=composition-r375.source_1.video.codec_resolver kind=CodecResolver
[2026-11-02 10:19:34.542] [info] [graph][runtime.node] register node=41 name=composition-r375.source_1.video.decode kind=VideoDecode
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=42 name=composition-r375.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=43 name=composition-r375.source_1.video.framerate kind=VideoFrameRate
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=44 name=composition-r375.source_1.video.preparation kind=VideoPreparation
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=45 name=composition-r375.source_1.video.filter kind=VideoFilter
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=46 name=composition-r375.source_2.video.input kind=RawRtpInput
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=47 name=composition-r375.source_2.audio.input kind=RawRtpInput
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=48 name=composition-r375.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=49 name=composition-r375.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=50 name=composition-r375.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:19:34.543] [info] [graph][runtime.node] register node=51 name=composition-r375.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=52 name=composition-r375.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=53 name=composition-r375.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=54 name=composition-r375.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=55 name=composition-r375.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=56 name=composition-r375.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=57 name=composition-r375.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=58 name=composition-r375.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=59 name=composition-r375.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=60 name=composition-r375.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=62 name=composition-r375.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:19:34.544] [info] [graph][runtime.node] register node=63 name=composition-r375.source_2.video.codec_resolver kind=CodecResolver
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=64 name=composition-r375.source_2.video.decode kind=VideoDecode
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=65 name=composition-r375.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=66 name=composition-r375.source_2.video.framerate kind=VideoFrameRate
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=67 name=composition-r375.source_2.video.preparation kind=VideoPreparation
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=68 name=composition-r375.source_2.video.filter kind=VideoFilter
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=69 name=composition-r375.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=70 name=composition-r375.source_2.audio.encode.decode kind=AudioDecode
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=71 name=composition-r375.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-02 10:19:34.545] [info] [graph][runtime.node] register node=72 name=composition-r375.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-02 10:19:34.546] [info] [graph][runtime.node] register node=73 name=composition-r375.source_2.audio.encode.resample kind=AudioResample
[2026-11-02 10:19:34.546] [info] [graph][runtime.node] register node=74 name=composition-r375.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-02 10:19:34.546] [info] [graph][runtime.node] register node=75 name=composition-r375.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-02 10:19:34.546] [info] [graph][runtime.node] register node=76 name=composition-r375.output.mpegts.mux kind=FileMux
[2026-11-02 10:19:34.546] [info] [graph][runtime.node] register node=77 name=composition-r375.output.mpegts.plan.source kind=Unknown
[2026-11-02 10:19:34.546] [info] [graph][runtime.node] register node=78 name=composition-r375.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-02 10:19:34.546] [info] [graph][runtime.node] register node=79 name=composition-r375.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-02 10:19:34.547] [info] [graph][runtime.node] register node=80 name=composition-r375.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-02 10:19:34.547] [info] [graph][runtime.node] register node=81 name=composition-r375.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-02 10:19:34.547] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-02 10:19:34.547] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-02 10:19:34.549] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-02 10:19:34.550] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:19:34.550] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-02 10:19:34.550] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:19:34.550] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:34.550] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-02 10:19:34.551] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:19:34.550] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-02 10:19:34.551] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-02 10:19:34.551] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:19:34.551] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-02 10:19:34.551] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-11-02 10:19:34.550] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:34.552] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:34.552] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-02 10:19:34.553] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:19:34.555] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:19:34.555] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=30 generation=1
[2026-11-02 10:19:34.555] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450272532300 group=composition-r375.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-3024000000 epoch_master_release_ns=4966675700 video_count=15 min_presentation_ns=-3024000000 max_end_ns=-2524000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.556] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3889560155 dts=3889560155 size=108891 generation=1 sequence=30
[2026-11-02 10:19:34.556] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450272677200 group=composition-r375.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.556] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450272787700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.556] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-02 10:19:34.556] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-02 10:19:34.557] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:19:34.557] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:19:34.558] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=29 generation=1
[2026-11-02 10:19:34.558] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=30 generation=1
[2026-11-02 10:19:34.558] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450275238100 group=composition-r375.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-3009000000 epoch_master_release_ns=4969359100 video_count=15 min_presentation_ns=-3009000000 max_end_ns=-2509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.558] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1410309255 dts=1410309255 size=108891 generation=1 sequence=30
[2026-11-02 10:19:34.558] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450275431200 group=composition-r375.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.559] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450275602900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.559] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450275642200 group=composition-r375.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-3023000000 epoch_master_release_ns=4969666900 video_count=26 min_presentation_ns=-3023000000 max_end_ns=-2156333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.559] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=730342931 dts=730342931 size=108891 generation=1 sequence=29
[2026-11-02 10:19:34.559] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450275920900 group=composition-r375.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.559] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450276035300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.590] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:19:34.591] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450307725700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.591] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3889560155 format=cuda size=1280x720 buffer_identity=0xb08400000 drm_prime=0 software_frame=0
[2026-11-02 10:19:34.591] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:19:34.591] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450308369400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.592] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450308675600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.592] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450308844500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.592] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E18ABE9C70 stream=Video payload=Frame pts=3889560155 dts=3889560155 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.592] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1410309255 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E18ABE9C70 stream=Video payload=Frame pts=3889560155 dts=3889560155 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450309587700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E18ABE9C70 stream=Video payload=Frame pts=3889560155 dts=3889560155 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450310117300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E18ABE84D0 stream=Video payload=Frame pts=3889560155 dts=3889560155 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E18ABE84D0 stream=Video payload=Frame pts=3889560155 dts=3889560155 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.593] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:19:34.595] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450311775300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.595] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450311939800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.595] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E18ABE9880 stream=Video payload=Frame pts=1410309255 dts=1410309255 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.595] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E18ABE9880 stream=Video payload=Frame pts=1410309255 dts=1410309255 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.595] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450312273700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.595] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:19:34.595] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E18ABE9880 stream=Video payload=Frame pts=1410309255 dts=1410309255 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.596] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E18ABE80E0 stream=Video payload=Frame pts=1410309255 dts=1410309255 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.596] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450312731900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.596] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:19:34.596] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E18ABE80E0 stream=Video payload=Frame pts=1410309255 dts=1410309255 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.598] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:19:34.598] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450314990400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.598] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=730342931 format=cuda size=1280x720 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-11-02 10:19:34.599] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450315789600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.599] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E18E2A8BC0 stream=Video payload=Frame pts=730342931 dts=730342931 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.599] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450315943200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.599] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450316153300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.599] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E18E2A8BC0 stream=Video payload=Frame pts=730342931 dts=730342931 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.599] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E18E2A8BC0 stream=Video payload=Frame pts=730342931 dts=730342931 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.600] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:19:34.600] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E18E2A8FB0 stream=Video payload=Frame pts=730342931 dts=730342931 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.600] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450316875600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.600] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:19:34.600] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E18E2A8FB0 stream=Video payload=Frame pts=730342931 dts=730342931 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:19:34.666] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=72
[2026-11-02 10:19:34.666] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=65
[2026-11-02 10:19:34.667] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450384251000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.669] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450385953500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.696] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450412557300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.696] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=99
[2026-11-02 10:19:34.696] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=243448 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450413343900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.697] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=243448 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450414255300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.698] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0b120000 drm_prime=0 software_frame=0
[2026-11-02 10:19:34.697] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450413889600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.697] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450414459300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.697] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=243448 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450414270500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.699] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=243448 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450416151000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.699] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=26 audio_count=23 first_video_sequence=29
[2026-11-02 10:19:34.699] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5450416560700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.700] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296520 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450416595100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.700] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450416909500 group=composition-r375.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-3023000000 epoch_master_release_ns=5177703968 video_count=26 min_presentation_ns=-3023000000 max_end_ns=-2156333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.700] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=243448 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450416917700 group=composition-r375.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.700] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296520 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450417494500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.700] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296520 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450417517000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.701] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=470103 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450417709700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.701] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450417834100 group=composition-r375.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-3023000000 epoch_master_release_ns=5177703968 video_count=26 min_presentation_ns=-3023000000 max_end_ns=-2156333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.701] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r375.source.2.input.video identity_link=direct generation=1 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable pts=730342931 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450418194700 group=composition-r375.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-3023000000 epoch_master_release_ns=4969666900 video_count=26 min_presentation_ns=-3023000000 max_end_ns=-2156333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=26 release_count=28 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.701] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r375.source.2.input.video generation=1 steady_ns=5450418203700 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:19:34.701] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-02 10:19:34.701] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0b000000 drm_prime=0 software_frame=0
[2026-11-02 10:19:34.702] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296520 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450418616800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.702] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=470103 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450418871100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.702] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=470103 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450418891100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.702] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-11-02 10:19:34.703] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-02 10:19:34.703] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=30
[2026-11-02 10:19:34.703] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0b090000 drm_prime=0 software_frame=0
[2026-11-02 10:19:34.703] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=470103 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450420528500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.704] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5180613266 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:19:34.704] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296520 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450421179000 group=composition-r375.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.704] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450421184200 group=composition-r375.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-3024000000 epoch_master_release_ns=5182100768 video_count=15 min_presentation_ns=-3024000000 max_end_ns=-2524000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.704] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=30
[2026-11-02 10:19:34.705] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5227053175 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:19:34.705] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450421952100 group=composition-r375.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-3024000000 epoch_master_release_ns=4966675700 video_count=15 min_presentation_ns=-3024000000 max_end_ns=-2524000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.705] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r375.source.1.input.video generation=1 steady_ns=5450421969900 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:19:34.705] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r375.source.1.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable pts=3889560155 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450421952400 group=composition-r375.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-3024000000 epoch_master_release_ns=5182100768 video_count=15 min_presentation_ns=-3024000000 max_end_ns=-2524000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.705] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=470103 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5450422233900 group=composition-r375.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.705] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450422240100 group=composition-r375.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-3009000000 epoch_master_release_ns=5183132868 video_count=15 min_presentation_ns=-3009000000 max_end_ns=-2509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.705] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:19:34.706] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r375.source.0.input.video generation=1 steady_ns=5450423291500 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:19:34.706] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450423293300 group=composition-r375.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-3009000000 epoch_master_release_ns=5183132868 video_count=15 min_presentation_ns=-3009000000 max_end_ns=-2509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.706] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r375.source.0.input.video identity_link=direct generation=1 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable pts=1410309255 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5450423305800 group=composition-r375.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-3009000000 epoch_master_release_ns=4969359100 video_count=15 min_presentation_ns=-3009000000 max_end_ns=-2509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:19:34.707] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:19:34.707] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=261100 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:19:34.708] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=32100 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-02 10:19:34.707] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-02 10:19:34.708] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=1081800 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:19:34.709] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=85900 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-02 10:19:34.709] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=315300 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-02 10:19:34.709] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-02 10:19:34.710] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r375.source.0.input.video generation=1 steady_ns=5450426943100 source_sequence=30 presentation_ns=-3009000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:19:34.710] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-02 10:19:34.710] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r375.source.1.input.video generation=1 steady_ns=5450427154500 source_sequence=30 presentation_ns=-3024000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:19:34.710] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-02 10:19:34.710] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r375.source.2.input.video generation=1 steady_ns=5450427507100 source_sequence=29 presentation_ns=-3023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:19:34.711] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-02 10:19:34.719] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-02 10:19:34.719] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=90751 side_data_elems=1 received_footprint=90759 side_data[0]=Quality stats:8
[2026-11-02 10:19:34.719] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=90759 wrapped_size=90751 wrapped_side_data_elems=1
[2026-11-02 10:19:34.719] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5184719568 dispatch_master_ns=5184719568 now_ns=5030711400 node=74 identity_kind=output identity="composition-r375.output" canonical_sequence=1 canonical_created_steady_ns=5450436368100 head_observed_steady_ns=5450436415300 codec_receive_begin_steady_ns=5450436012000 codec_receive_end_steady_ns=5450436013300
[2026-11-02 10:19:34.720] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5031095000 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5184719568 present_dispatch_master_ns=5184719568
[2026-11-02 10:19:34.756] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-02 10:19:34.757] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5184719568 dispatch_master_ns=5184719568 now_ns=5067891000 node=74 identity_kind=output identity="composition-r375.output" canonical_sequence=1 canonical_created_steady_ns=5450473568400 head_observed_steady_ns=5450473592500 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 10:19:34.878] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-02 10:19:35.311] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6271951134 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:19:36.333] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7293629139 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:19:37.354] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8315307142 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:19:38.399] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=9360205101 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:19:39.148] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574779 ntp_fraction=635655159 arrival_ns=5454865331500
[2026-11-02 10:19:39.148] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:19:39.149] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:39.163] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574779 ntp_fraction=700079669 arrival_ns=5454879930700
[2026-11-02 10:19:39.163] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:19:39.163] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:39.194] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574779 ntp_fraction=828928688 arrival_ns=5454910279600
[2026-11-02 10:19:39.194] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:19:39.194] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:39.195] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574779 ntp_fraction=837518622 arrival_ns=5454911796900
[2026-11-02 10:19:39.195] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:19:39.195] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:39.421] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10381883106 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:19:40.442] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11403561110 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:19:41.464] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12425239115 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:19:42.509] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13470137074 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:19:43.531] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14491815079 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:19:44.151] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574784 ntp_fraction=644245094 arrival_ns=5459867601500
[2026-11-02 10:19:44.151] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:19:44.151] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:44.166] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574784 ntp_fraction=708669603 arrival_ns=5459882467600
[2026-11-02 10:19:44.166] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:19:44.166] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:44.211] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574784 ntp_fraction=906238099 arrival_ns=5459927964000
[2026-11-02 10:19:44.211] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:19:44.211] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574784 ntp_fraction=906238099 arrival_ns=5459928110300
[2026-11-02 10:19:44.212] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:44.212] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:19:44.213] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:44.552] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=15513493083 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:19:45.579] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=16535171088 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:19:46.619] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=17580069047 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:19:47.641] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=18601747051 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:19:48.663] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=19623425056 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:19:49.168] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574789 ntp_fraction=721554505 arrival_ns=5464884892600
[2026-11-02 10:19:49.168] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:19:49.168] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:49.186] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574789 ntp_fraction=794568949 arrival_ns=5464902053600
[2026-11-02 10:19:49.186] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:19:49.186] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:49.213] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574789 ntp_fraction=910533066 arrival_ns=5464929510900
[2026-11-02 10:19:49.213] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574789 ntp_fraction=910533066 arrival_ns=5464929551200
[2026-11-02 10:19:49.213] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:19:49.213] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:49.213] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:19:49.214] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:49.707] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=20668323015 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:19:50.729] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=21690001020 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:19:51.750] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=22711679024 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:19:52.773] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=23733357029 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:19:53.817] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=24778254988 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:19:54.197] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574794 ntp_fraction=846108557 arrival_ns=5469914388700
[2026-11-02 10:19:54.198] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:19:54.198] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:54.213] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574794 ntp_fraction=914828034 arrival_ns=5469929792800
[2026-11-02 10:19:54.213] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:19:54.213] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:54.242] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574794 ntp_fraction=1035087118 arrival_ns=5469958567200
[2026-11-02 10:19:54.242] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574794 ntp_fraction=1039382085 arrival_ns=5469959002100
[2026-11-02 10:19:54.242] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:19:54.242] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:54.242] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:19:54.243] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:54.839] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=25799932992 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:19:55.861] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=26821610997 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:19:56.906] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=978 source_master_ns=27866508956 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:19:57.927] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=28888186961 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:19:58.969] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=29909864965 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:19:59.206] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574799 ntp_fraction=884763262 arrival_ns=5474923195100
[2026-11-02 10:19:59.207] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:19:59.207] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:59.250] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574799 ntp_fraction=1069446856 arrival_ns=5474966733700
[2026-11-02 10:19:59.250] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:19:59.250] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:59.250] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574799 ntp_fraction=1073741824 arrival_ns=5474966814300
[2026-11-02 10:19:59.251] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:19:59.251] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:59.251] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574799 ntp_fraction=1073741824 arrival_ns=5474967584500
[2026-11-02 10:19:59.251] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:19:59.251] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:19:59.970] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=30931542970 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:20:01.016] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=31976440929 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:20:02.037] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=32998118933 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:20:03.059] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34019796938 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:20:04.104] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1288 source_master_ns=35064694897 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:20:04.218] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574804 ntp_fraction=936302870 arrival_ns=5479934782900
[2026-11-02 10:20:04.218] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:04.218] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:04.278] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574804 ntp_fraction=1194000908 arrival_ns=5479994870400
[2026-11-02 10:20:04.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:04.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:04.279] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574804 ntp_fraction=1194000908 arrival_ns=5479995355100
[2026-11-02 10:20:04.279] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:04.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:04.279] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574804 ntp_fraction=1198295875 arrival_ns=5479995877300
[2026-11-02 10:20:04.279] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:04.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:05.126] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36086372902 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:20:06.147] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37108050906 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:20:07.169] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38129728911 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:20:08.214] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39174626870 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:20:09.231] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574809 ntp_fraction=992137445 arrival_ns=5484948233500
[2026-11-02 10:20:09.232] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:09.232] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:09.236] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40196304875 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:20:09.286] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574809 ntp_fraction=1228360646 arrival_ns=5485003242700
[2026-11-02 10:20:09.286] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:09.287] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:09.316] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574809 ntp_fraction=1357209665 arrival_ns=5485033501100
[2026-11-02 10:20:09.317] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:09.317] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574809 ntp_fraction=1357209665 arrival_ns=5485033635900
[2026-11-02 10:20:09.317] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:09.318] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:09.317] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:10.257] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41217982879 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:20:11.302] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1598 source_master_ns=42262880838 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:20:12.324] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43284558843 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:20:13.345] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44306236847 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:20:14.258] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574814 ntp_fraction=1108101562 arrival_ns=5489975480000
[2026-11-02 10:20:14.259] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:14.259] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:14.288] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574814 ntp_fraction=1236950581 arrival_ns=5490005417000
[2026-11-02 10:20:14.289] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:14.289] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:14.318] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574814 ntp_fraction=1365799600 arrival_ns=5490035348900
[2026-11-02 10:20:14.319] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:14.319] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:14.360] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574814 ntp_fraction=1546188226 arrival_ns=5490077007400
[2026-11-02 10:20:14.361] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:14.361] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:14.367] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45327914852 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 10:20:15.412] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46372812811 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:20:16.434] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47394490816 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:20:17.455] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48416168820 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:20:18.503] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1908 source_master_ns=49461066779 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:20:19.266] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574819 ntp_fraction=1142461300 arrival_ns=5494983398000
[2026-11-02 10:20:19.267] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:19.267] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:19.308] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574819 ntp_fraction=1322849927 arrival_ns=5495025083300
[2026-11-02 10:20:19.309] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:19.309] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:19.341] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574819 ntp_fraction=1464583847 arrival_ns=5495057804000
[2026-11-02 10:20:19.341] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:19.341] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:19.371] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574819 ntp_fraction=1589137899 arrival_ns=5495087664200
[2026-11-02 10:20:19.371] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:19.371] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:19.522] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=50482744784 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:20:20.543] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=51504422788 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:20:21.566] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=52526100793 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:20:22.613] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=53570998752 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:20:22.659] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=52970716700 present_generation=1 present_canonical_target_ns=47902766439 present_canonical_dispatch_ns=47902766439 present_target_master_ns=53087486007 present_dispatch_master_ns=53087486007
[2026-11-02 10:20:22.662] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=52973054900 present_generation=1 present_canonical_target_ns=47933333333 present_canonical_dispatch_ns=47933333333 present_target_master_ns=53118052901 present_dispatch_master_ns=53118052901
[2026-11-02 10:20:23.633] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=54592676757 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:20:24.280] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574824 ntp_fraction=1198295875 arrival_ns=5499996552700
[2026-11-02 10:20:24.280] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:24.280] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:24.336] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574824 ntp_fraction=1434519076 arrival_ns=5500051546900
[2026-11-02 10:20:24.336] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:24.336] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:24.347] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574824 ntp_fraction=1486058684 arrival_ns=5500062913200
[2026-11-02 10:20:24.348] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:24.348] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:24.372] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574824 ntp_fraction=1597727834 arrival_ns=5500089268600
[2026-11-02 10:20:24.373] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:24.373] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:24.653] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=55614354761 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:20:25.698] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2218 source_master_ns=56659252720 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:20:26.720] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=57680930725 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:20:27.741] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=58702608729 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:20:28.763] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=59724286734 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:20:29.283] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574829 ntp_fraction=1215475744 arrival_ns=5505000467500
[2026-11-02 10:20:29.284] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:29.284] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:29.341] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574829 ntp_fraction=1464583847 arrival_ns=5505057871100
[2026-11-02 10:20:29.341] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:29.342] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:29.384] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574829 ntp_fraction=1649267441 arrival_ns=5505100947900
[2026-11-02 10:20:29.384] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:29.385] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:29.416] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574829 ntp_fraction=1782411427 arrival_ns=5505132316800
[2026-11-02 10:20:29.416] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:29.416] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:29.808] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=60769184693 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:20:30.830] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=61790862698 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:20:31.852] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=62812540702 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:20:32.896] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2528 source_master_ns=63857438661 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:20:33.918] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=64879116666 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:20:34.290] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574834 ntp_fraction=1245540515 arrival_ns=5510006932500
[2026-11-02 10:20:34.290] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:34.290] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:34.346] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574834 ntp_fraction=1486058684 arrival_ns=5510062828300
[2026-11-02 10:20:34.346] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:34.346] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:34.414] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574834 ntp_fraction=1773821493 arrival_ns=5510130529600
[2026-11-02 10:20:34.414] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:34.414] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:34.457] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574834 ntp_fraction=1962800054 arrival_ns=5510173873000
[2026-11-02 10:20:34.457] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:34.457] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:34.941] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=65900794670 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:20:35.962] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=66922472675 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:20:37.006] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=67967370634 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:20:38.028] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=68989048639 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:20:39.049] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70010726643 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:20:39.308] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574839 ntp_fraction=1318554959 arrival_ns=5515024564000
[2026-11-02 10:20:39.308] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:39.308] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:39.348] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574839 ntp_fraction=1494648619 arrival_ns=5515064887200
[2026-11-02 10:20:39.348] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:39.349] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:39.451] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574839 ntp_fraction=1932735283 arrival_ns=5515167503900
[2026-11-02 10:20:39.451] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:39.451] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:39.480] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574839 ntp_fraction=2061584302 arrival_ns=5515196991000
[2026-11-02 10:20:39.480] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:39.481] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:40.072] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=71032404648 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:20:41.116] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72077302607 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:20:41.801] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=72112732600 present_generation=1 present_canonical_target_ns=67036009070 present_canonical_dispatch_ns=67036009070 present_target_master_ns=72220728638 present_dispatch_master_ns=72220728638
[2026-11-02 10:20:41.818] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=72129397800 present_generation=1 present_canonical_target_ns=67066666666 present_canonical_dispatch_ns=67066666666 present_target_master_ns=72251386234 present_dispatch_master_ns=72251386234
[2026-11-02 10:20:42.139] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73098980611 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:20:43.159] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74120658616 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:20:44.205] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75165556575 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:20:44.324] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574844 ntp_fraction=1391569403 arrival_ns=5520041036500
[2026-11-02 10:20:44.324] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:44.324] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:44.353] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574844 ntp_fraction=1516123455 arrival_ns=5520070305900
[2026-11-02 10:20:44.354] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:44.354] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:44.457] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574844 ntp_fraction=1962800054 arrival_ns=5520174246800
[2026-11-02 10:20:44.458] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:44.458] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:44.512] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574844 ntp_fraction=2194728288 arrival_ns=5520228504200
[2026-11-02 10:20:44.512] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:44.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:45.227] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76187234580 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:20:46.248] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77208912584 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:20:47.269] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=78230590589 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:20:48.314] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79275488548 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:20:49.334] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574849 ntp_fraction=1434519076 arrival_ns=5525051047000
[2026-11-02 10:20:49.334] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:49.334] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:49.337] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80297166553 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:20:49.385] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574849 ntp_fraction=1653562408 arrival_ns=5525101792500
[2026-11-02 10:20:49.385] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:49.385] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:49.482] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574849 ntp_fraction=2070174236 arrival_ns=5525199322700
[2026-11-02 10:20:49.483] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:49.483] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:49.513] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574849 ntp_fraction=2203318222 arrival_ns=5525229808200
[2026-11-02 10:20:49.513] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:49.513] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:50.359] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81318844557 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:20:51.403] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82363742516 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:20:52.425] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83385420521 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:20:53.446] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84407098525 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:20:54.356] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574854 ntp_fraction=1529008357 arrival_ns=5530072995200
[2026-11-02 10:20:54.356] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:54.356] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:54.386] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574854 ntp_fraction=1657857376 arrival_ns=5530103137600
[2026-11-02 10:20:54.387] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:54.387] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:54.469] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85428776530 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:20:54.521] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574854 ntp_fraction=2237677961 arrival_ns=5530237750500
[2026-11-02 10:20:54.521] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:54.521] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:54.522] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574854 ntp_fraction=2241972928 arrival_ns=5530239085300
[2026-11-02 10:20:54.522] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:54.522] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:55.513] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=86473674489 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:20:56.535] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=87495352494 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:20:57.557] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=88517030498 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:20:58.601] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3635 source_master_ns=89561928457 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:20:59.368] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574859 ntp_fraction=1576252997 arrival_ns=5535084497500
[2026-11-02 10:20:59.368] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:20:59.368] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:59.395] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574859 ntp_fraction=1696512081 arrival_ns=5535111785300
[2026-11-02 10:20:59.395] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:20:59.395] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:59.538] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574859 ntp_fraction=2310692405 arrival_ns=5535255278600
[2026-11-02 10:20:59.538] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574859 ntp_fraction=2310692405 arrival_ns=5535255430900
[2026-11-02 10:20:59.539] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:20:59.539] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:59.539] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:20:59.540] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:20:59.623] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=90583606462 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:21:00.644] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=91605284466 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:21:01.666] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=92626962471 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:21:02.712] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=93671860430 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:21:03.733] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=94693538435 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:21:04.368] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574864 ntp_fraction=1580547964 arrival_ns=5540085027300
[2026-11-02 10:21:04.368] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:21:04.369] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:04.413] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574864 ntp_fraction=1773821493 arrival_ns=5540129961000
[2026-11-02 10:21:04.413] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:21:04.414] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:04.548] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574864 ntp_fraction=2353642078 arrival_ns=5540265175600
[2026-11-02 10:21:04.549] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:21:04.549] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:04.549] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574864 ntp_fraction=2357937045 arrival_ns=5540265670900
[2026-11-02 10:21:04.549] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:21:04.549] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:04.754] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=95715216439 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:21:05.799] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3945 source_master_ns=96760114398 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:21:06.821] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=97781792403 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:21:07.842] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=98803470407 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:21:08.864] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=99825148412 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:21:09.376] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574869 ntp_fraction=1610612736 arrival_ns=5545092573500
[2026-11-02 10:21:09.376] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:21:09.376] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:09.420] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574869 ntp_fraction=1803886264 arrival_ns=5545137167100
[2026-11-02 10:21:09.420] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:21:09.421] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:09.571] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574869 ntp_fraction=2448131358 arrival_ns=5545287646500
[2026-11-02 10:21:09.571] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:21:09.571] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:09.586] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574869 ntp_fraction=2512555868 arrival_ns=5545302344200
[2026-11-02 10:21:09.586] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:21:09.586] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:09.909] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=100870046371 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 10:21:10.931] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=101891724376 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 10:21:11.952] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=102913402380 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 10:21:12.997] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4255 source_master_ns=103958300339 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 10:21:14.023] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=104979978344 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 10:21:14.396] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574874 ntp_fraction=1700807049 arrival_ns=5550112919100
[2026-11-02 10:21:14.396] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:21:14.396] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:14.441] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574874 ntp_fraction=1894080577 arrival_ns=5550157953500
[2026-11-02 10:21:14.441] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:21:14.442] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:14.575] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574874 ntp_fraction=2465311227 arrival_ns=5550291082300
[2026-11-02 10:21:14.575] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:21:14.576] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:14.603] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574874 ntp_fraction=2589865279 arrival_ns=5550320340900
[2026-11-02 10:21:14.604] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:21:14.604] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:15.041] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106001656348 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 10:21:16.063] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107023334353 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 10:21:17.111] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108068232312 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 10:21:18.129] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109089910317 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 10:21:19.152] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110111588321 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 10:21:19.403] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574879 ntp_fraction=1730871820 arrival_ns=5555120424500
[2026-11-02 10:21:19.404] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:21:19.404] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:19.447] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574879 ntp_fraction=1919850381 arrival_ns=5555164457400
[2026-11-02 10:21:19.448] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:21:19.448] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:19.584] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574879 ntp_fraction=2503965933 arrival_ns=5555300458500
[2026-11-02 10:21:19.584] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:21:19.584] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:19.614] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574879 ntp_fraction=2637109919 arrival_ns=5555331425800
[2026-11-02 10:21:19.615] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:21:19.615] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:20.196] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4565 source_master_ns=111156486280 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 10:21:21.217] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112178164285 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 10:21:22.239] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113199842289 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 10:21:23.271] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114221520294 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 10:21:24.305] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115266418253 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 10:21:24.419] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2306948744 ntp_seconds=4002574884 ntp_fraction=1799591297 arrival_ns=5560135771500
[2026-11-02 10:21:24.419] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2306948744
[2026-11-02 10:21:24.419] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:24.459] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3189793667 ntp_seconds=4002574884 ntp_fraction=1967095021 arrival_ns=5560175616200
[2026-11-02 10:21:24.459] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3189793667
[2026-11-02 10:21:24.459] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:24.603] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3021349188 ntp_seconds=4002574884 ntp_fraction=2589865279 arrival_ns=5560320397800
[2026-11-02 10:21:24.604] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3021349188
[2026-11-02 10:21:24.604] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:24.616] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=70483136 ntp_seconds=4002574884 ntp_fraction=2645699854 arrival_ns=5560333376200
[2026-11-02 10:21:24.617] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=70483136
[2026-11-02 10:21:24.617] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:21:25.327] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116288096258 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 10:21:26.349] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117309774262 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 10:21:27.394] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4875 source_master_ns=118354672221 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 10:21:28.415] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119376350226 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 10:21:28.613] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=5564329860600
[2026-11-02 10:21:28.613] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:21:28.613] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r375.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=3021349188
[2026-11-02 10:21:28.613] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.0 phase=purging old=1 next=2 transition=1
[2026-11-02 10:21:28.615] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-02 10:21:28.616] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3593
[2026-11-02 10:21:28.617] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:21:28.627] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=5564344185100
[2026-11-02 10:21:28.627] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:21:28.627] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=5564344311200
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r375.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=3189793667 audio_raw_generation=0 audio_ssrc=2306948744
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r375.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=3189793667 audio_raw_generation=0 audio_ssrc=2306948744
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.2 phase=purging old=1 next=2 transition=1
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5146
[2026-11-02 10:21:28.628] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3592
[2026-11-02 10:21:28.629] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r375.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=70483136
[2026-11-02 10:21:28.629] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.1 phase=purging old=1 next=2 transition=1
[2026-11-02 10:21:28.630] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=5564345225500
[2026-11-02 10:21:28.631] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:21:28.630] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-02 10:21:28.632] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-02 10:21:28.633] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3593
[2026-11-02 10:21:28.634] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:22:02.668] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574922 ntp_fraction=2869038153 arrival_ns=5598385059300
[2026-11-02 10:22:02.668] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:02.669] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:22:02.668] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r375.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=1812454431 raw_generation=1 raw_sequence=5149 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-02 10:22:02.669] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574922 ntp_fraction=2869038153 arrival_ns=5598385073500
[2026-11-02 10:22:02.670] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:02.670] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:03.210] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-02 10:22:03.705] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=5599422522900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3514 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.706] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5599422997900 group=composition-r375.source.2 release_identity=15625 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=154116565100 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.706] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=5599423464100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=26 release_count=3541 release_committed_count=3514 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.706] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5599423438400 group=composition-r375.source.2 release_identity=15625 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=154116565100 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.707] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5599423626900 group=composition-r375.source.2 release_identity=15625 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=154116565100 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.707] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5599424058500 group=composition-r375.source.2 release_identity=15625 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=154116565100 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.707] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=5599424086200 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:22:03.707] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5599424470700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:22:03.708] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=154117011812 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:22:03.708] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=154163451721 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:22:03.713] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=5599430303800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:22:03.713] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5599430500200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5599430636600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=5599430703000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=5599431013700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=5599431053500 input_count=3439 accepted_count=3439 would_block_count=0 resolved_count=3439 output_attempt_count=0 published_count=3438 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5599431214700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5599431342000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:22:03.714] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5599431420000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.715] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=167848764 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=5599431778000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.715] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=55950 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5599431981900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.715] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=55950 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5599432094200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.715] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=5599432116100 input_count=3432 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3432 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.715] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=55950 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5599432315000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.715] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=55950 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5599432529600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.716] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r375.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=55950 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=5599432689900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:22:03.721] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r375.source.2.input.video generation=2 steady_ns=5599438481100 source_sequence=4 presentation_ns=1093000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:22:03.741] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r375.source.2.input.video generation=2 steady_ns=5599458024700 source_sequence=4 presentation_ns=1093000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:22:04.244] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=155208349680 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:22:05.266] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=156230027685 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:22:06.302] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=157251705689 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:22:07.332] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=158296603648 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:22:07.693] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574927 ntp_fraction=2972117368 arrival_ns=5603408847700
[2026-11-02 10:22:07.693] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:07.693] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:07.698] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574927 ntp_fraction=2997887172 arrival_ns=5603415008600
[2026-11-02 10:22:07.699] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:07.699] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:08.354] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=159318281653 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:22:09.376] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=160339959657 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:22:10.402] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=161361637662 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:22:11.447] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=162406535621 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:22:12.464] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=163428213626 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:22:12.712] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574932 ntp_fraction=3058016714 arrival_ns=5608429039900
[2026-11-02 10:22:12.713] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:12.713] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:12.713] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574932 ntp_fraction=3058016714 arrival_ns=5608429119200
[2026-11-02 10:22:12.713] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:12.713] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:13.485] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=164449891630 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:22:14.531] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=165494789589 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:22:15.552] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=166516467594 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:22:16.574] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=167538145598 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:22:17.595] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=168559823603 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:22:17.733] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574937 ntp_fraction=3143916060 arrival_ns=5613449541300
[2026-11-02 10:22:17.733] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:17.733] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:17.747] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574937 ntp_fraction=3208340570 arrival_ns=5613463922700
[2026-11-02 10:22:17.748] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:17.748] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:18.643] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=169604721562 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:22:19.677] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=170626399567 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:22:20.684] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=171648077571 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:22:21.729] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=172692975530 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:22:22.745] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574942 ntp_fraction=3199750635 arrival_ns=5618461963400
[2026-11-02 10:22:22.745] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:22.745] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:22.750] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=173714653535 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:22:22.776] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574942 ntp_fraction=3332894621 arrival_ns=5618492970400
[2026-11-02 10:22:22.776] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:22.776] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:23.772] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=174736331539 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:22:24.798] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=175758009544 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:22:25.848] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=176802907503 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:22:26.860] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=177824585508 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:22:27.768] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574947 ntp_fraction=3298534883 arrival_ns=5623485042700
[2026-11-02 10:22:27.768] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:27.769] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:27.814] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574947 ntp_fraction=3491808411 arrival_ns=5623530535000
[2026-11-02 10:22:27.814] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:27.814] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:27.882] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=178846263512 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:22:28.928] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=179891161471 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:22:29.957] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=180912839476 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:22:30.970] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=181934517480 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:22:31.992] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=182956195485 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:22:32.773] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574952 ntp_fraction=3320009719 arrival_ns=5628490071300
[2026-11-02 10:22:32.773] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:32.774] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:32.817] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574952 ntp_fraction=3508988280 arrival_ns=5628534254700
[2026-11-02 10:22:32.818] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:32.818] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:33.039] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=184001093444 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:22:34.058] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=185022771449 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:22:35.080] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=186044449453 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:22:36.059] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=186370074400 present_generation=1 present_canonical_target_ns=181301405895 present_canonical_dispatch_ns=181301405895 present_target_master_ns=186486125463 present_dispatch_master_ns=186486125463
[2026-11-02 10:22:36.064] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=186375053200 present_generation=1 present_canonical_target_ns=181333333333 present_canonical_dispatch_ns=181333333333 present_target_master_ns=186518052901 present_dispatch_master_ns=186518052901
[2026-11-02 10:22:36.126] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1421 source_master_ns=187089347412 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:22:37.146] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=188111025417 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:22:37.774] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574957 ntp_fraction=3324304687 arrival_ns=5633490961400
[2026-11-02 10:22:37.775] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:37.775] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:37.846] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574957 ntp_fraction=3633542332 arrival_ns=5633563410100
[2026-11-02 10:22:37.847] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:37.847] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:38.173] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=189132703421 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:22:39.190] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=190154381426 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:22:40.237] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=191199279385 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:22:41.258] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=192220957390 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:22:42.278] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=193242635394 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:22:42.794] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574962 ntp_fraction=3410204033 arrival_ns=5638511107700
[2026-11-02 10:22:42.794] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:42.794] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:42.879] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574962 ntp_fraction=3770981285 arrival_ns=5638595649000
[2026-11-02 10:22:42.879] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:42.879] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:43.323] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1731 source_master_ns=194287533353 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 10:22:44.350] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=195309211358 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:22:45.366] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=196330889363 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:22:46.392] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=197352567367 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:22:47.433] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=198397465326 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:22:47.795] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574967 ntp_fraction=3414499000 arrival_ns=5643512282600
[2026-11-02 10:22:47.796] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:47.796] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:47.915] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574967 ntp_fraction=3925600108 arrival_ns=5643631592800
[2026-11-02 10:22:47.915] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:47.915] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:48.454] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=199419143331 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:22:49.477] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=200440821335 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:22:50.521] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2041 source_master_ns=201485719295 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:22:51.549] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=202507397299 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:22:52.565] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=203529075304 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:22:52.823] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574972 ntp_fraction=3534758084 arrival_ns=5648540135300
[2026-11-02 10:22:52.823] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:52.824] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:52.917] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574972 ntp_fraction=3929895075 arrival_ns=5648632601600
[2026-11-02 10:22:52.917] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:52.917] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:53.587] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=204550753308 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:22:54.641] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=205595651267 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:22:55.653] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=206617329272 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:22:56.675] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=207639007276 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:22:57.696] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=208660685281 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:22:57.848] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574977 ntp_fraction=3642132267 arrival_ns=5653564757100
[2026-11-02 10:22:57.848] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:22:57.848] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:57.948] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574977 ntp_fraction=4071628996 arrival_ns=5653665161400
[2026-11-02 10:22:57.948] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:22:57.948] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:22:58.741] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=209705583240 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:22:59.769] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2439 source_master_ns=210727261245 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:23:00.801] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=211748939249 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:23:01.832] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=212793837208 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:23:02.851] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574982 ntp_fraction=3655017168 arrival_ns=5658568326500
[2026-11-02 10:23:02.852] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:02.852] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:02.853] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=213815515213 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:23:02.973] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574982 ntp_fraction=4174708211 arrival_ns=5658689534900
[2026-11-02 10:23:02.973] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:02.973] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:03.876] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=214837193217 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:23:04.900] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=215858871222 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:23:05.944] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=216903769181 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:23:06.964] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=217925447186 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:23:07.868] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574987 ntp_fraction=3728031612 arrival_ns=5663584739700
[2026-11-02 10:23:07.868] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:07.868] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:07.983] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=218947125190 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:23:08.004] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574988 ntp_fraction=12884901 arrival_ns=5663720450300
[2026-11-02 10:23:08.004] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:08.004] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:09.030] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=219992023149 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:23:10.054] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=221013701154 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:23:11.071] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=222035379158 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:23:12.093] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=223057057163 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:23:12.898] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574992 ntp_fraction=3852585664 arrival_ns=5668614667100
[2026-11-02 10:23:12.898] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:12.899] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:13.046] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574993 ntp_fraction=197568495 arrival_ns=5668762885200
[2026-11-02 10:23:13.046] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:13.047] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:13.138] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=224101955122 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:23:14.159] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=225123633127 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:23:15.187] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=226145311131 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:23:16.234] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=227190209090 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:23:17.247] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=228211887095 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:23:17.917] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002574997 ntp_fraction=3938485010 arrival_ns=5673633766100
[2026-11-02 10:23:17.917] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:17.917] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:18.069] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002574998 ntp_fraction=296352743 arrival_ns=5673785920100
[2026-11-02 10:23:18.070] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:18.070] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:18.269] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=229233565100 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:23:19.303] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=230255243104 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:23:20.351] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=231300141063 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:23:21.357] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=232321819068 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:23:22.379] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=233343497072 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:23:22.942] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575002 ntp_fraction=4041564225 arrival_ns=5678658048100
[2026-11-02 10:23:22.942] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:22.942] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:23.076] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575003 ntp_fraction=326417514 arrival_ns=5678792894200
[2026-11-02 10:23:23.076] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:23.077] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:23.424] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3458 source_master_ns=234388395031 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:23:24.449] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=235410073036 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:23:25.008] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=235319374000 present_generation=1 present_canonical_target_ns=230249070294 present_canonical_dispatch_ns=230249070294 present_target_master_ns=235433789862 present_dispatch_master_ns=235433789862
[2026-11-02 10:23:25.012] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=235323585200 present_generation=1 present_canonical_target_ns=230266666666 present_canonical_dispatch_ns=230266666666 present_target_master_ns=235451386234 present_dispatch_master_ns=235451386234
[2026-11-02 10:23:25.467] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=236431751041 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:23:26.489] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=237453429045 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:23:27.553] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=238498327004 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:23:27.953] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575007 ntp_fraction=4093103833 arrival_ns=5683670393500
[2026-11-02 10:23:27.954] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:27.954] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:28.113] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575008 ntp_fraction=485331304 arrival_ns=5683829805200
[2026-11-02 10:23:28.113] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:28.114] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:28.555] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=239520005009 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:23:29.579] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=240541683013 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:23:30.623] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3768 source_master_ns=241586580973 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:23:31.645] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=242608258977 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:23:32.666] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=243629936982 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:23:32.965] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575012 ntp_fraction=4140348473 arrival_ns=5688681543200
[2026-11-02 10:23:32.965] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:32.965] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:33.138] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575013 ntp_fraction=592705486 arrival_ns=5688854831400
[2026-11-02 10:23:33.138] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:33.138] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:33.689] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=244651614986 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:23:34.732] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=245696512945 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:23:35.753] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=246718190950 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:23:36.784] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=247739868954 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:23:37.822] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4078 source_master_ns=248784766914 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:23:37.978] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575017 ntp_fraction=4200478015 arrival_ns=5693695131000
[2026-11-02 10:23:37.978] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:37.979] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:38.182] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575018 ntp_fraction=773094113 arrival_ns=5693896862000
[2026-11-02 10:23:38.183] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:38.183] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:38.845] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4122 source_master_ns=249806444918 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 10:23:39.864] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4166 source_master_ns=250828122923 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 10:23:40.885] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4210 source_master_ns=251849800927 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 10:23:41.936] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4255 source_master_ns=252894698886 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 10:23:42.961] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4299 source_master_ns=253916376891 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 10:23:42.980] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575022 ntp_fraction=4209067950 arrival_ns=5698696921200
[2026-11-02 10:23:42.980] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:42.981] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:43.183] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575023 ntp_fraction=777389080 arrival_ns=5698897829000
[2026-11-02 10:23:43.183] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:43.183] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:43.975] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4343 source_master_ns=254938054895 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 10:23:45.020] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4388 source_master_ns=255982952855 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 10:23:46.040] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4432 source_master_ns=257004630859 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 10:23:47.062] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4476 source_master_ns=258026308864 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 10:23:47.981] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575027 ntp_fraction=4213362917 arrival_ns=5703698235300
[2026-11-02 10:23:47.982] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:47.982] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:48.083] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4520 source_master_ns=259047986868 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 10:23:48.201] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575028 ntp_fraction=858993459 arrival_ns=5703917516900
[2026-11-02 10:23:48.201] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:48.201] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:49.128] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4565 source_master_ns=260092884827 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 10:23:50.150] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4609 source_master_ns=261114562832 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 10:23:51.171] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4653 source_master_ns=262136240836 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 10:23:52.194] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4697 source_master_ns=263157918841 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 10:23:52.985] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575032 ntp_fraction=4230542786 arrival_ns=5708702433700
[2026-11-02 10:23:52.986] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:52.986] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:53.202] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575033 ntp_fraction=863288426 arrival_ns=5708918416800
[2026-11-02 10:23:53.202] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:53.202] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:53.239] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4742 source_master_ns=264202816800 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 10:23:54.260] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4786 source_master_ns=265224494805 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 10:23:55.282] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4830 source_master_ns=266246172809 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 10:23:56.327] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4875 source_master_ns=267291070768 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 10:23:57.348] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4919 source_master_ns=268312748773 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 10:23:58.000] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1812454431 ntp_seconds=4002575038 ntp_fraction=0 arrival_ns=5713717165900
[2026-11-02 10:23:58.000] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1812454431
[2026-11-02 10:23:58.001] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:58.224] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1077701371 ntp_seconds=4002575038 ntp_fraction=962072674 arrival_ns=5713940758000
[2026-11-02 10:23:58.224] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1077701371
[2026-11-02 10:23:58.224] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:23:58.378] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=269334426778 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-02 10:23:59.393] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=270356104782 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-02 10:23:59.627] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=269938110400 present_generation=1 present_canonical_target_ns=264870022675 present_canonical_dispatch_ns=264870022675 present_target_master_ns=270054742243 present_dispatch_master_ns=270054742243
[2026-11-02 10:23:59.632] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=269943233600 present_generation=1 present_canonical_target_ns=264900000000 present_canonical_dispatch_ns=264900000000 present_target_master_ns=270084719568 present_dispatch_master_ns=270084719568
[2026-11-02 10:24:00.442] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=271401002741 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-02 10:24:01.458] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=272422680746 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-11-02 10:24:02.155] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=5717872022500
[2026-11-02 10:24:02.155] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r375.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1077701371 audio_raw_generation=1 audio_ssrc=1812454431
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r375.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1077701371 audio_raw_generation=1 audio_ssrc=1812454431
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.2 phase=purging old=2 next=3 transition=2
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=2 cutoff=0
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=2 cutoff=0
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=2 cutoff=7187
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=2 cutoff=10339
[2026-11-02 10:24:02.156] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=5717873261700
[2026-11-02 10:24:02.157] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-02 10:24:02.157] [info] [graph][runtime.node] av_reacquisition group=composition-r375.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-02 10:24:34.679] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-11-02 10:24:34.680] [info] [graph][runtime.lifecycle] worker.finished node=2
[2026-11-02 10:24:34.680] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=3593750000 process_calls=27003
[2026-11-02 10:24:34.680] [info] [graph][runtime.lifecycle] worker.finished node=77
[2026-11-02 10:24:34.680] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=8
[2026-11-02 10:24:34.680] [info] [graph][runtime.lifecycle] worker.finished node=80
[2026-11-02 10:24:34.681] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=6
[2026-11-02 10:24:34.680] [info] [graph][runtime.lifecycle] worker.finished node=4
[2026-11-02 10:24:34.681] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=6453125000 process_calls=40378
[2026-11-02 10:24:34.680] [info] [graph][runtime.lifecycle] worker.finished node=5
[2026-11-02 10:24:34.681] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=390625000 process_calls=38761
[2026-11-02 10:24:34.712] [info] [graph][runtime.lifecycle] worker.finished node=74
[2026-11-02 10:24:34.713] [info] [graph][runtime.lifecycle] worker.finished node=75
[2026-11-02 10:24:34.713] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=406250000 process_calls=65605
[2026-11-02 10:24:34.713] [info] [graph][runtime.lifecycle] worker.finished node=78
[2026-11-02 10:24:34.713] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=437500000 process_calls=65050
[2026-11-02 10:24:34.713] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=796875000 process_calls=65902
[2026-11-02 10:24:34.740] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=175637 produced_ts_payload_bytes=203802716 pending_bytes=0 peak_pending_bytes=92872 access_units=21919 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=26802720 maximum_mux_input_after_emit_ns=55827921 maximum_mux_input_after_dispatch_ns=-44172079 worst_mux_input_ready_ns=269940910300 worst_mux_input_emit_ns=269885082379 worst_mux_input_dispatch_ns=269985082379 maximum_protocol_batch_produced_after_release_ns=59311952 worst_protocol_batch_produced_ns=162477368000 worst_protocol_batch_release_ns=162418056048 worst_protocol_batch_deadline_ns=162484722716 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-11-02 10:24:34.741] [info] [graph][runtime.lifecycle] worker.finished node=76
[2026-11-02 10:24:34.741] [info] [graph][runtime.lifecycle] worker.finished node=79
[2026-11-02 10:24:34.741] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=1546875000 process_calls=105610
[2026-11-02 10:24:34.741] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=3046875000 process_calls=133025
[2026-11-02 10:24:34.741] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=175637 last_sequence=175637 maximum_submit_interval_ns=101400 maximum_submit_interval_sequence=175637 maximum_start_lateness_ns=175566 maximum_start_lateness_sequence=175637 first_release_master_ns=305051386234 first_deadline_master_ns=305151357676 last_release_master_ns=305051386234 last_deadline_master_ns=305151357676 first_submitted_master_ns=305051663200 last_submitted_master_ns=305051663200 worst_wait_started_master_ns=305040799600 worst_wait_completed_master_ns=305051559200 worst_submit_observed_master_ns=305051561800 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=10757100 wait_call_max_ns=10757100 wait_before_call_count=1 wait_before_call_total_ns=100 wait_before_call_max_ns=100 wait_arm_count=1 wait_arm_total_ns=1100 wait_arm_max_ns=1100 wait_api_wall_count=1 wait_api_wall_total_ns=10754500 wait_api_wall_max_ns=10754500 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=2500 wait_return_to_next_clock_max_ns=2500 wait_target_lateness_count=1 wait_target_lateness_total_ns=172966 wait_target_lateness_max_ns=172966 worst_wait_metric=call_wall worst_wait_sequence=175637 worst_wait_target_master_ns=305051386234 worst_wait_sampled_master_ns=305040799600 worst_wait_sampled_steady_ns=5750446500900 worst_wait_timeout_ns=10586634 worst_wait_entered_steady_ns=5750446501000 worst_wait_returned_steady_ns=5750457258100 worst_wait_arm_before_steady_ns=5750446501400 worst_wait_arm_after_steady_ns=5750446502500 worst_wait_api_before_steady_ns=5750446502500 worst_wait_api_returned_steady_ns=5750457257000 worst_wait_windows_due_100ns=-105867 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=17672 worst_wait_windows_tid=21676 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=305051559200 worst_wait_next_steady_ns=5750457260600 worst_wait_outcome=deadline
[2026-11-02 10:24:34.742] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=32424 committed_datagrams=175637 committed_payload_bytes=203802716 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=175637 pacing_cancelled=0 pacing_submitted=175637 pacing_maximum_submit_lateness_ns=32203532 pacing_worst_late_global_sequence=145047 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=69445200 last_materialized_sequence=175637 last_scheduled_sequence=175637 last_submitted_sequence=175637 last_committed_sequence=175637 submitted_service_generation=1 submitted_service_sequence=175637 submitted_service_completed_ns=305051663200 submitted_service_duration_ns=34560 submitted_service_available_ns=305051697760 submitted_scope_available_ns=305051697760 shared_scope_charged_datagrams=175637 shared_scope_charged_wire_bytes=208720552 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=175637 endpoint_1_committed_payload_bytes=203802716
[2026-11-02 10:24:34.742] [info] [graph][runtime.lifecycle] worker.finished node=81
[2026-11-02 10:24:34.742] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=10609375000 process_calls=64000
[2026-11-02 10:24:34.940] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-11-02 10:24:34.940] [info] [graph][runtime.lifecycle] stop.begin
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=234375000 process_calls=93143
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=93750000 process_calls=10577
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=781250000 process_calls=183257
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=81
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=15625000 process_calls=309
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=265625000 process_calls=88580
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=104
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=80
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=107
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=457
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=454
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=46875000 process_calls=10544
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=140625000 process_calls=10694
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=80
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=15625000 process_calls=80
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=125000000 process_calls=10684
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=234375000 process_calls=10558
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=93750000 process_calls=10636
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=125000000 process_calls=10634
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=80
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=156250000 process_calls=21435
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=234375000 process_calls=30793
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=62500000 process_calls=10695
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=296875000 process_calls=21361
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=109375000 process_calls=10595
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=687500000 process_calls=51592
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=515625000 process_calls=30714
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1234375000 process_calls=14271
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1234375000 process_calls=14422
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=309
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2750000000 process_calls=34956
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=187500000 process_calls=21300
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=104
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=375000000 process_calls=31349
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=105
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=93750000 process_calls=11371
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=140625000 process_calls=11278
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=250000000 process_calls=22210
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=281250000 process_calls=21624
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1718750000 process_calls=50597
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=265625000 process_calls=30948
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=302
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=156250000 process_calls=10737
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=625000000 process_calls=52316
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=93750000 process_calls=10584
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=359375000 process_calls=51598
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=171875000 process_calls=10620
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=93750000 process_calls=10655
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=484375000 process_calls=51363
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=140625000 process_calls=10577
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=828125000 process_calls=192089
[2026-11-02 10:24:34.941] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=1125000000 process_calls=42248
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=93750000 process_calls=10557
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=125000000 process_calls=10527
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=453125000 process_calls=31200
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1015625000 process_calls=13712
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=921875000 process_calls=13706
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1781250000 process_calls=25093
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=812500000 process_calls=50188
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=218750000 process_calls=10499
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=93750000 process_calls=10502
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=3453125000 process_calls=49148
[2026-11-02 10:24:34.942] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=343750000 process_calls=21183
[2026-11-02 10:24:34.969] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=59337852 worst_materialized_at_ns=162477393900 worst_release_ns=162418056048 worst_deadline_ns=162484722716 worst_global_sequence=91268
[2026-11-02 10:24:34.969] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750397234500 maximum_inter_event_gap_ns=49611200 gap_begin_generation=1 gap_begin_begin=795648 gap_begin_end=796672 gap_begin_rate=44100 gap_begin_steady_ns=5468513802800 gap_end_generation=1 gap_end_begin=796672 gap_end_end=797696 gap_end_rate=44100 gap_end_steady_ns=5468563414000
[2026-11-02 10:24:34.970] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=543 missing=audio begin_steady_ns=5468513818800 end_steady_ns=5468563417000 observed_wait_ns=49598200 held_dispatch_master_ns=23251386234 begin_master_ns=23108117400 end_master_ns=23157714900 dispatch_remaining_at_end_ns=93671334 resumed_dispatch_master_ns=23249844284
[2026-11-02 10:24:34.970] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750397222300 maximum_inter_event_gap_ns=49583800 gap_begin_generation=1 gap_begin_begin=795648 gap_begin_end=796672 gap_begin_rate=44100 gap_begin_steady_ns=5468513780400 gap_end_generation=1 gap_end_begin=796672 gap_end_end=797696 gap_end_rate=44100 gap_end_steady_ns=5468563364200
[2026-11-02 10:24:34.970] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750397227800 maximum_inter_event_gap_ns=49605400 gap_begin_generation=1 gap_begin_begin=795648 gap_begin_end=796672 gap_begin_rate=44100 gap_begin_steady_ns=5468513786300 gap_end_generation=1 gap_end_begin=796672 gap_end_end=797696 gap_end_rate=44100 gap_end_steady_ns=5468563391700
[2026-11-02 10:24:34.971] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750382246500 maximum_inter_event_gap_ns=50139100 gap_begin_generation=1 gap_begin_begin=797696 gap_begin_end=798720 gap_begin_rate=44100 gap_begin_steady_ns=5468512370000 gap_end_generation=1 gap_end_begin=798720 gap_end_end=799744 gap_end_rate=44100 gap_end_steady_ns=5468562509100
[2026-11-02 10:24:34.971] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750382334000 maximum_inter_event_gap_ns=49459100 gap_begin_generation=1 gap_begin_begin=797696 gap_begin_end=798720 gap_begin_rate=44100 gap_begin_steady_ns=5468513760500 gap_end_generation=1 gap_end_begin=798720 gap_end_end=799744 gap_end_rate=44100 gap_end_steady_ns=5468563219600
[2026-11-02 10:24:34.971] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750397210500 maximum_inter_event_gap_ns=49476100 gap_begin_generation=1 gap_begin_begin=795648 gap_begin_end=796672 gap_begin_rate=44100 gap_begin_steady_ns=5468513766000 gap_end_generation=1 gap_end_begin=796672 gap_end_end=797696 gap_end_rate=44100 gap_end_steady_ns=5468563242100
[2026-11-02 10:24:34.971] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750397221600 maximum_inter_event_gap_ns=49488300 gap_begin_generation=1 gap_begin_begin=795648 gap_begin_end=796672 gap_begin_rate=44100 gap_begin_steady_ns=5468513771700 gap_end_generation=1 gap_end_begin=796672 gap_end_end=797696 gap_end_rate=44100 gap_end_steady_ns=5468563260000
[2026-11-02 10:24:34.972] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-02 10:24:34.972] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750382202300 maximum_inter_event_gap_ns=50112600 gap_begin_generation=1 gap_begin_begin=797696 gap_begin_end=798720 gap_begin_rate=44100 gap_begin_steady_ns=5468512331300 gap_end_generation=1 gap_end_begin=798720 gap_end_end=799744 gap_end_rate=44100 gap_end_steady_ns=5468562443900
[2026-11-02 10:24:34.972] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=12919 last_generation=1 last_begin=13228032 last_end=13229056 last_rate=44100 last_steady_ns=5750382243400 maximum_inter_event_gap_ns=48670400 gap_begin_generation=1 gap_begin_begin=797696 gap_begin_end=798720 gap_begin_rate=44100 gap_begin_steady_ns=5468513796000 gap_end_generation=1 gap_end_begin=798720 gap_end_end=799744 gap_end_rate=44100 gap_end_steady_ns=5468562466400
[2026-11-02 10:24:34.972] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r375.output audio_commits=12919 real_samples=10249216 silence_samples=2979840
[2026-11-02 10:24:34.973] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:24:34.973] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-02 10:24:34.973] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=9000 maximum_call_ns=20501600 maximum_begin_steady_ns=5572929050700 maximum_end_steady_ns=5572949552300 maximum_encoder_pts=3675 maximum_native_result=0 maximum_between_submissions_ns=63375400 before_gap_begin_steady_ns=5680658872200 before_gap_end_steady_ns=5680662350900 before_gap_encoder_pts=6907 before_gap_native_result=0 after_gap_begin_steady_ns=5680725726300 after_gap_end_steady_ns=5680729055100 after_gap_encoder_pts=6908 after_gap_native_result=0
[2026-11-02 10:24:34.973] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=49148 maximum_ns=22670700 begin_steady_ns=5622666125600 end_steady_ns=5622688796300 video_cursor=5167 audio_cursor=not_owned next_video_cursor=5167 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:24:34.973] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=49148 maximum_ns=1444200 begin_steady_ns=5564332126800 end_steady_ns=5564333571000 video_cursor=3418 audio_cursor=not_owned next_video_cursor=3418 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:24:34.973] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=49144 maximum_ns=383300 begin_steady_ns=5646524492500 end_steady_ns=5646524875800 video_cursor=5883 audio_cursor=not_owned next_video_cursor=5883 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:24:34.973] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=49144 maximum_ns=12547300 begin_steady_ns=5481767478800 end_steady_ns=5481780026100 video_cursor=941 audio_cursor=not_owned next_video_cursor=941 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:24:34.974] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=10 maximum_ns=20100 begin_steady_ns=5450423753600 end_steady_ns=5450423773700 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:24:34.974] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=9000 maximum_ns=12730700 begin_steady_ns=5506358063800 end_steady_ns=5506370794500 video_cursor=1678 audio_cursor=not_owned next_video_cursor=1678 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:24:34.974] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=9000 maximum_ns=12714600 begin_steady_ns=5506358069200 end_steady_ns=5506370783800 video_cursor=1678 audio_cursor=not_owned next_video_cursor=1678 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:24:34.975] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=9001 maximum_ns=11644000 begin_steady_ns=5622677151400 end_steady_ns=5622688795400 video_cursor=5167 audio_cursor=not_owned next_video_cursor=5167 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:24:34.975] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=49147 maximum_ns=144060800 begin_steady_ns=5450272867600 end_steady_ns=5450416928400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:24:34.975] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r375.output output_generation=1 video_commits=9000 source[0].group=composition-r375.source.0 source[0].identity=composition-r375.source.0.input.video source[0].real_frames=3418 source[0].black_frames=5582 source[0].contribution_generation=0 source[1].group=composition-r375.source.1 source[1].identity=composition-r375.source.1.input.video source[1].real_frames=3418 source[1].black_frames=5582 source[1].contribution_generation=0 source[2].group=composition-r375.source.2 source[2].identity=composition-r375.source.2.input.video source[2].real_frames=6971 source[2].black_frames=2029 source[2].contribution_generation=0  source[2].first_interval_miss_first_candidate_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_candidate_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_candidate_final_source_sequence=4  source[2].first_interval_miss_first_candidate_final_presentation_ns=1093000000  source[2].first_interval_miss_first_candidate_final_duration_ns=33333333  source[2].first_interval_miss_first_candidate_final_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_final_source_sequence=4  source[2].first_interval_miss_first_real_final_presentation_ns=1093000000  source[2].first_interval_miss_first_real_final_duration_ns=33333333  source[2].first_interval_miss_first_real_final_decoded_sequence=unavailable source[2].first_interval_miss_cursor=4470 source[2].first_interval_miss_target_ns=154184719568 source[2].first_interval_miss_steady_ns=5599424180600 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable source[2].first_interval_miss_epoch_source_start_ns=993000000 source[2].first_interval_miss_epoch_master_release_ns=154116565100 source[2].first_interval_miss_prepare_expired=0 source[2].first_interval_miss_prepare_old_generation=0 source[2].first_interval_miss_prepare_incomplete=0 source[2].first_interval_miss_first_real_cursor_at_miss=unavailable source[2].first_interval_miss_first_real_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_begin_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_end_ns_at_miss=unavailable source[2].first_interval_miss_prepare_last_begin_ns=unavailable source[2].first_interval_miss_prepare_last_end_ns=unavailable source[2].first_interval_miss_first_candidate_steady_ns_final=5599438481100 source[2].first_interval_miss_first_candidate_begin_ns_final=154216565100 source[2].first_interval_miss_first_candidate_end_ns_final=154249898433 source[2].first_interval_miss_first_real_cursor=4471 source[2].first_interval_miss_first_real_steady_ns=5599458024700 source[2].first_interval_miss_cursor_committed_generation=0 source[2].first_interval_miss_cursor_committed_steady_ns=5599424244300
[2026-11-02 10:24:34.976] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6975 operation=copy maximum_ns=431400 begin_steady_ns=5674524421800 end_steady_ns=5674524853200 native_result=0 thread_cpu_ns=0
[2026-11-02 10:24:34.976] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=20784 operation=copy maximum_ns=1414900 begin_steady_ns=5502358417400 end_steady_ns=5502359832300 native_result=0 thread_cpu_ns=0
[2026-11-02 10:24:34.976] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41570 operation=copy maximum_ns=4087900 begin_steady_ns=5607827125300 end_steady_ns=5607831213200 native_result=0 thread_cpu_ns=0
[2026-11-02 10:24:34.977] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=97000 begin_steady_ns=5450166560700 end_steady_ns=5450166657700 native_result=0 thread_cpu_ns=0
[2026-11-02 10:24:34.977] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6975 operation=copy maximum_ns=407300 begin_steady_ns=5711792333400 end_steady_ns=5711792740700 native_result=0 thread_cpu_ns=0
[2026-11-02 10:24:34.977] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6973 operation=copy maximum_ns=12558300 begin_steady_ns=5506358216000 end_steady_ns=5506370774300 native_result=0 thread_cpu_ns=0
[2026-11-02 10:24:34.977] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=5506358069800 end_steady_ns=5506370783400 wall_ns=12713600 thread_cpu_ns=0 surface_count=1 surface_total_ns=900 surface_maximum_ns=900 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1300 validate_maximum_ns=700 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=80300 black_maximum_ns=80300 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=57200 tile_maximum_ns=25300 tile_maximum_tile=1 tile_maximum_result=0 publish_count=1 publish_total_ns=3300 publish_maximum_ns=3300 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=2800 context_push_maximum_ns=2800 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=2700 source_wait_maximum_ns=1800 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=108200 plane_copy_maximum_ns=68000 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=3900 context_pop_maximum_ns=3900 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=12557400 batch_completion_maximum_ns=12557400 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=5800 source_query_ready_begin_steady_ns=5506358158600 source_query_ready_end_steady_ns=5506358164400 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-02 10:24:34.978] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=5750694653400 input_count=3569 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3569 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:24:34.978] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=5750694907300 input_count=3436 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3436 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=11 incomplete=0
[2026-11-02 10:24:34.978] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=5750695143200 input_count=3434 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3434 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=11 incomplete=0
[2026-11-02 10:24:34.978] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-02 10:24:34.979] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:24:34.979] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=5750696387400 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:24:34.989] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:24:34.990] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=5750707147500 input_count=3447 accepted_count=3447 would_block_count=0 resolved_count=3447 output_attempt_count=0 published_count=3447 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:34.995] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:24:34.995] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=5750711956700 input_count=3445 accepted_count=3445 would_block_count=0 resolved_count=3445 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.003] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=5750720242500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.003] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=5750720463400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3447 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.003] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=5750720581500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.004] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3610 download=0 upload=0
[2026-11-02 10:24:35.004] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=5750720779400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.004] [info] [graph][runtime.node] hardware_transfer.summary forwarded=38 download=0 upload=0
[2026-11-02 10:24:35.004] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=5750720989600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.004] [info] [graph][runtime.node] hardware_transfer.summary forwarded=40 download=0 upload=0
[2026-11-02 10:24:35.004] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=5750721180300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.004] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-02 10:24:35.005] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:24:35.005] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=5750721871800 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:24:35.005] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-02 10:24:35.021] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:24:35.021] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=5750738374600 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:24:35.021] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-02 10:24:35.036] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:24:35.036] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=5750753417300 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=79 incomplete=0
[2026-11-02 10:24:35.037] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-02 10:24:35.058] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=5750774847700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.058] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=5750775304400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3541 release_committed_count=3525 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.059] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=5750775633300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3541 release_committed_count=3525 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.059] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=5750775948500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.059] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=5750776280900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3525 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.060] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=5750776613200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3525 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:24:35.060] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:24:35.060] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:24:35.061] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:24:35.061] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10289 completion_waits=10289 completed_datagrams=10415 drained_datagrams=156 would_block=32 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:24:35.061] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=143305 completion_waits=143305 completed_datagrams=211022 drained_datagrams=67748 would_block=32 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:24:35.062] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=78134 completion_waits=78134 completed_datagrams=104341 drained_datagrams=26298 would_block=92 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:24:35.063] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=76687 completion_waits=76687 completed_datagrams=104326 drained_datagrams=27729 would_block=92 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:24:35.064] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-02 10:24:35.065] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-02 10:24:35.121] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=782, threads=81, processThreads=130, workers=0, workerProgress=575033, workerProcessCalls=2178006, workerWaits=1602956, workerWakeups=1580135, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=941, logicalProcessors=22, averageSystemMachineCpuPercent=8.312212, averageProcessMachineCpuPercent=1.688225, peakProcessMachineCpuPercent=7.466063, averageProcessSingleCoreCpuPercent=37.140950, peakProcessSingleCoreCpuPercent=164.253394, initialWorkingSetBytes=205099008, workingSetBytes=257286144, peakWorkingSetBytes=265453568, totalPushed=493829, totalPopped=493793, droppedBuffers=0, encodedPacketsPushed=100601, encodedPacketsPopped=100601, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=14005644, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=615, graphPayloadReservations=226023, graphPayloadReleases=226023, graphPayloadPressureFailures=0, lastInputReceivedAtNs=5717871447400
[CLI] realtime video validation stopped successfully
~~~

~~~text
[in @ 000001e18a8b7380] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e18a8b7380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15670.102833
[in @ 000001e18a95bb00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e18a95bb00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43217.335056
[in @ 000001e18e4b2900] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e18e4b2900] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8114.921456
[in @ 000001e18b3c9e40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e18b3c9e40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8114.921456
[in @ 000001e18aa64600] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e18aa64600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43217.335056
[in @ 000001e18b3cab00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e18b3cab00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15670.102833
[aac @ 000001e18a8c8540] Qavg: 15265.516

~~~

## source0 完成日志

~~~text
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.18 bitrate=8114.5kbits/s speed=   1x elapsed=0:01:58.68    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.20    
[out#0/rtp @ 000001d8503f5700] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001d852033800] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## source1 完成日志

~~~text
frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.66 bitrate=8116.4kbits/s speed=   1x elapsed=0:01:58.15    
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.67    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.18    
[out#0/rtp @ 0000027a80596880] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

## source2 完成日志

~~~text
frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.66 bitrate=8116.4kbits/s speed=   1x elapsed=0:01:58.17    
frame= 3576 fps= 30 q=-1.0 size=  118130KiB time=00:01:59.20 bitrate=8118.5kbits/s speed=   1x elapsed=0:01:58.69    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.21    
[out#0/rtp @ 000002bca8f05e00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

## reentry 完成日志

~~~text
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.66    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.18    
[out#0/rtp @ 000001bb64329300] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001bb660ddec0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## CPU与内存逐秒采样

列：elapsed_seconds,cpu_machine_percent,working_set_bytes,private_bytes。

~~~csv
1.0012391,0.567478655379937,18202624,345190400
2.0317437,0.827044078476435,140447744,567263232
3.0575944,0.969262078602843,144756736,571019264
4.0877807,0.0689416344138214,148148224,574017536
5.1159318,0.552624821567392,196788224,711806976
6.1374008,4.72803918136082,249593856,1246425088
7.1723521,2.60772041772752,249901056,1246425088
8.1998369,2.14280984541528,250195968,1244712960
9.2256978,2.63082805511316,252878848,1247084544
10.2574277,1.99631617820623,252153856,1246289920
11.2832214,1.93863187465117,254734336,1248821248
12.3139073,1.72270541570248,254103552,1247756288
13.340378,2.07573564270448,254341120,1247760384
14.3596609,2.16005246968682,254377984,1247772672
15.3883634,2.76164303178916,254410752,1247772672
16.4081397,2.01971657010375,257486848,1250988032
17.4326836,2.91149510084882,257630208,1250988032
18.4732356,2.32066511550863,257658880,1250988032
19.5025256,1.72504171012852,257830912,1250988032
20.5295057,1.93639230559225,256872448,1249779712
21.5594004,1.44818423934726,257081344,1249779712
22.5847609,1.59312039743362,257093632,1249779712
23.6177862,2.26882148965761,257150976,1249779712
24.6616117,2.24534656415272,257208320,1249779712
25.6957209,2.67852405107349,257282048,1249787904
26.7226739,3.18129987890922,257282048,1249787904
27.7569652,1.92270433255734,257273856,1249787904
28.7872839,1.93011770400398,257331200,1249787904
29.8095144,1.52852023100465,257445888,1249787904
30.8271297,2.02400562462956,257515520,1249792000
31.8586432,2.20329377436871,258342912,1250582528
32.8832258,1.45569256468661,259551232,1251704832
33.908942,2.07726251977088,261656576,1253203968
34.9424049,2.06169163709875,261672960,1253203968
35.9777857,1.78348961955921,260341760,1251815424
37.0026886,1.52453466567418,259293184,1250762752
38.0321645,2.41462228940517,259293184,1250762752
39.0548422,1.59729964511079,259354624,1250762752
40.0885024,1.99258817443981,259678208,1251131392
41.1241808,2.53731361887138,260673536,1252134912
42.1532162,2.20859969708259,261865472,1253285888
43.1793097,1.59198233618353,262660096,1254080512
44.2067428,2.90330781191937,260718592,1251966976
45.2293539,2.5002840100388,259031040,1250275328
46.2669205,2.94340360679235,259502080,1250795520
47.2983158,1.51493806496888,259837952,1250988032
48.322184,2.21974593285275,259854336,1250988032
49.36186,1.98105860951786,260304896,1251389440
50.3932289,2.27246526436856,261808128,1252798464
51.4243298,1.85977302159626,261808128,1252798464
52.4438478,1.60225001154735,261869568,1252798464
53.4689958,1.73201155522732,260759552,1251598336
54.497092,2.00337195187483,260833280,1251598336
55.5280259,1.65339936396063,260825088,1251598336
56.5597135,2.40944589674768,260833280,1251606528
57.5838817,2.4964826888964,261566464,1252335616
58.6194883,1.50877756090005,261545984,1252220928
59.6448245,3.46338729056515,261570560,1252253696
60.6739647,2.13936307750348,261636096,1252253696
61.6972117,2.70695771757588,261632000,1252253696
62.733314,1.71369968179607,261627904,1252253696
63.7565973,1.87398117057479,261685248,1252253696
64.791064,1.92237832656804,261685248,1252253696
65.8160567,1.87085589620652,261943296,1252450304
66.8476457,2.40967619327606,263245824,1253834752
67.8807295,1.58121028252764,263245824,1253834752
68.9090733,1.93382443073645,263245824,1253834752
69.9298213,3.27021770487739,263245824,1253834752
70.960075,2.34386222274448,263245824,1253834752
71.9930767,2.26887332324816,263245824,1253834752
73.0262152,2.33731753029506,264040448,1254621184
74.0584912,2.20166629150271,262209536,1252786176
75.08004,2.3638349213202,262565888,1253072896
76.1036439,1.73462428368843,262565888,1253072896
77.1286056,2.28667080926047,262565888,1253072896
78.1530982,1.59447001107936,262565888,1253072896
79.1909706,2.18979449952352,262627328,1253072896
80.2233214,1.78872424866712,262627328,1253072896
81.2465092,2.01298245630869,262627328,1253072896
82.2733497,1.59082421006256,262737920,1253072896
83.3070371,1.85511948424991,263548928,1254113280
84.3315764,1.59439733280385,263553024,1254113280
85.3622948,2.13608735951017,263569408,1254113280
86.3961121,2.19838386601509,263565312,1254113280
87.4325651,2.53541734076788,263565312,1254113280
88.4646572,1.72035827211368,263684096,1254113280
89.4966941,2.20217636862334,263684096,1254113280
90.531548,3.56879538085696,264175616,1254363136
91.5637699,1.99536465067162,264269824,1254363136
92.6024092,2.25655817183116,264269824,1254363136
93.6300567,1.79691081727042,264273920,1254367232
94.6567601,1.79856315766647,264273920,1254367232
95.6932195,1.91868235614088,264368128,1254367232
96.7253109,1.72035943892003,264486912,1254367232
97.758473,1.92480576246106,264486912,1254367232
98.799481,1.97852378743399,264544256,1254367232
99.8267115,2.35076034762668,265064448,1255198720
100.8531132,2.83702941858128,265080832,1255198720
101.8872499,2.06034832549875,265080832,1255198720
102.9195574,2.33919905384076,265080832,1255198720
103.9425809,1.52733539356624,265170944,1255198720
104.9723637,1.58627890004836,265170944,1255198720
106.0018697,1.65569258901401,265170944,1255198720
107.0331786,2.68579701351978,265175040,1255198720
108.0562084,2.36041289048737,265420800,1255198720
109.0848223,2.89997495217064,265420800,1255198720
110.1150833,1.93022580068194,265420800,1255198720
111.1426467,2.34999877114416,265420800,1255198720
112.1716295,2.2777348659278,265420800,1255198720
113.2085826,1.98626060417687,265420800,1255198720
114.2427606,2.12894158012891,265420800,1255198720
115.2740838,2.34143159707134,265420800,1255198720
116.3006219,1.72966612911707,265420800,1255198720
117.3283453,2.28052606372493,265461760,1255198720
118.3547694,1.72985823483508,265453568,1255198720
119.3828898,2.69412645020598,240705536,1234821120
120.4204924,1.16363081938727,240721920,1235247104
121.4530906,1.23805086132156,240721920,1235247104
122.489216,1.37092918043949,240721920,1235247104
123.5241894,1.37245512344042,240721920,1235247104
124.554078,1.10338500335243,240734208,1235255296
125.5860531,1.2387983885552,240730112,1235255296
126.6045512,1.25519045240152,240730112,1235255296
127.6261944,1.11229011886306,240730112,1235255296
128.6539557,0.967460228185449,240730112,1235255296
129.6883875,1.16719764767128,240734208,1235259392
130.7193865,1.10219664263848,240742400,1235259392
131.7538918,1.44172994834082,240734208,1235259392
132.7884444,0.549205345558855,240713728,1235226624
133.8312711,1.02158959786022,240713728,1235226624
134.8592413,1.24362466043188,240721920,1235226624
135.8949511,1.37147929415612,240713728,1235226624
136.929329,0.755284891527548,240713728,1235226624
137.9576527,0.690665082140246,240713728,1235226624
138.9941873,1.02779097686747,240713728,1235226624
140.0235377,1.17295953218298,240717824,1235230720
141.055991,0.825483077319541,240717824,1235230720
142.0868693,0.964534981304969,240717824,1235230720
143.1270389,0.819359388385045,241487872,1236000768
144.1594679,1.16946188419383,240717824,1235230720
145.1814271,1.11194618764001,240717824,1235230720
146.2369786,0.874704317643879,240721920,1235234816
147.2693249,1.30715034110339,240721920,1235234816
148.3040062,1.37284258008193,240721920,1235234816
149.3251537,1.18238194152792,240742400,1235267584
150.3540274,1.17350299034404,240742400,1235267584
151.3866557,0.962900379369985,240742400,1235267584
152.4153799,0.897515052669564,240742400,1235267584
153.444373,1.31140997756137,243810304,1237716992
154.4758507,1.51481704354833,247525376,1241006080
155.5029094,1.59048623732288,249970688,1244225536
156.5270661,1.31760288067421,251191296,1245241344
157.5635871,2.05560892464488,251826176,1245835264
158.5896933,1.86882569890296,252248064,1246121984
159.6210331,1.7904776961879,252563456,1246334976
160.6443871,1.17983255416638,252719104,1247346688
161.6742102,1.44828492653471,253440000,1247674368
162.69992,1.52333535274794,254066688,1248026624
163.7197604,2.08923064646368,254185472,1248186368
164.7586734,1.09380057460409,254992384,1248976896
165.7981645,1.09319227106765,254275584,1248215040
166.8244835,1.38402830450819,254332928,1248272384
167.8596908,1.57796677754565,254427136,1249091584
168.8900912,1.24069157087779,255369216,1250029568
169.9196123,1.65566830494823,254754816,1249251328
170.9436263,1.45650086105003,255045632,1249374208
171.9766674,0.618760033317697,255078400,1249529856
173.0161653,1.98139802967285,255090688,1249579008
174.05246,1.43924047158332,255225856,1249583104
175.0839396,2.06565579986438,255225856,1249583104
176.1154269,1.79022166253617,255291392,1249595392
177.1502085,1.30407403666804,255299584,1249595392
178.1732371,1.80502373940563,255623168,1249927168
179.2031462,1.86192513141562,255836160,1250136064
180.2312634,1.86517027082481,255856640,1250148352
181.2636448,1.5134910411985,256053248,1250349056
182.291851,1.79593442355328,256053248,1250349056
183.3168667,1.87081391666846,256102400,1250439168
184.3547281,2.32668131532085,256102400,1250439168
185.3852266,1.30949420904717,256139264,1250471936
186.4135983,1.65751882762379,256139264,1250471936
187.4504228,1.50700528392217,256290816,1250623488
188.4711648,1.87864674556711,256303104,1250635776
189.4944846,1.59629739136557,256364544,1250672640
190.5115789,1.67589716562708,256389120,1250672640
191.5390434,1.65898233422706,256397312,1250684928
192.5777979,1.98281604643741,256397312,1250684928
193.6068331,1.44939383290999,256401408,1250684928
194.6356285,1.38069682801317,256405504,1250684928
195.6682763,1.99454169263623,256409600,1250684928
196.6936603,2.07793550336441,256409600,1250684928
197.7275314,1.30522249648125,256446464,1250684928
198.7538089,1.59169691167617,256446464,1250684928
199.7781096,1.59476873077674,256512000,1250684928
200.8124516,1.16729898199662,256126976,1249505280
201.8411923,1.7259628026948,256126976,1249505280
202.8702698,1.58736608979667,256126976,1249505280
203.9072997,1.36973345267529,256151552,1249505280
204.9402554,1.44389277558299,256299008,1249505280
205.9775574,1.3693741508785,256303104,1249505280
206.9998539,1.87579008278285,256294912,1249505280
208.0313139,1.23941703111038,256294912,1249505280
209.0561709,1.73250334614306,256294912,1249505280
210.0780321,1.59857593895604,256294912,1249505280
211.0993945,1.59935663117494,256536576,1249763328
212.1249009,0.831074995994874,256536576,1249763328
213.1623965,1.91676606978995,256544768,1249763328
214.1971809,1.44134108779304,256536576,1249763328
215.2296968,1.37572171571843,256536576,1249763328
216.2636632,1.51117096261542,256544768,1249763328
217.2896308,1.31527722530596,256544768,1249763328
218.3181206,1.104885664752,256544768,1249763328
219.3535006,1.8520868051958,256544768,1249763328
220.3760348,1.87535403350185,256614400,1249763328
221.4048542,1.58776431244664,256630784,1249779712
222.4362278,1.23952085927841,256630784,1249779712
223.4681829,1.30764586383831,256630784,1249779712
224.5000759,1.65186260062372,256630784,1249779712
225.5288129,1.72596901036729,256667648,1249816576
226.5544148,1.66199521914444,256667648,1249816576
227.5756974,1.80810963497361,256667648,1249816576
228.603089,1.58997088089167,257470464,1250594816
229.6252514,1.94551899349496,256688128,1249816576
230.654016,1.51881198089434,256688128,1249816576
231.6899447,1.57686791308391,256688128,1249816576
232.7113808,2.01643459723923,256688128,1249816576
233.7422557,1.37791166072097,256688128,1249816576
234.7725875,1.37863797415022,256688128,1249816576
235.8121194,1.29811487091623,256688128,1249816576
236.849176,1.23272836883647,256737280,1249816576
237.8782277,1.86347647680252,256753664,1249816576
238.9088405,1.5850013965213,256753664,1249816576
239.944823,1.8510096805338,256757760,1249816576
240.9744421,2.13836800954311,256753664,1249816576
241.9983996,1.3872202171033,256753664,1249816576
243.0280021,1.6555374084129,256761856,1249816576
244.0569527,1.79463514486595,256753664,1249816576
245.085068,1.31252965322257,256753664,1249816576
246.1313682,1.9685163884219,256753664,1249816576
247.1447837,2.10247605072332,256749568,1249816576
248.1712127,1.52226797956799,256749568,1249816576
249.2053869,1.51086731809787,256749568,1249816576
250.2253637,1.6015292968161,256749568,1249816576
251.2533991,1.58897517271559,256749568,1249816576
252.2756295,1.52852038053259,256749568,1249816576
253.2975663,1.59845768082013,257118208,1250304000
254.3167261,1.32406303523923,257118208,1250304000
255.3459653,2.07015222329451,257118208,1250304000
256.3845568,1.50444135158049,257118208,1250304000
257.4124944,1.52003390089044,257118208,1250304000
258.4414984,1.31139608610058,257896448,1251082240
259.4691283,2.07339414528691,257118208,1250304000
260.503045,1.57993649514781,257118208,1250304000
261.533574,1.37837416070246,257118208,1250304000
262.5570148,2.01248483635701,257118208,1250304000
263.5892428,1.92654758797117,257118208,1250304000
264.6165106,1.52102499465085,257118208,1250304000
265.6507071,1.92288057795251,257126400,1250304000
266.6815072,1.92921630841549,257118208,1250304000
267.7195776,2.18937682138641,257118208,1250304000
268.7568925,1.09548569712401,257118208,1250304000
269.7798998,1.45793414448484,257118208,1250304000
270.8041415,1.04012647511901,257118208,1250304000
271.8288574,2.07929028736827,257118208,1250304000
272.8658319,1.36980663020601,257118208,1250304000
273.896916,1.30875048716379,257118208,1250304000
274.9174183,1.1135336357043,257118208,1250304000
275.9518688,1.16717654797052,257118208,1250304000
276.9873668,1.09740785241847,257118208,1250304000
278.0200656,1.03160854751736,257118208,1250304000
279.0504156,1.03396021651954,257118208,1250304000
280.0790965,0.897552831539372,257118208,1250304000
281.099659,1.32224319253533,257126400,1250304000
282.1250242,1.10825258782298,257118208,1250304000
283.152271,0.691389131343386,257118208,1250304000
284.179972,1.24395042031588,257126400,1250304000
285.209761,1.10349172147269,257118208,1250304000
286.2332389,0.76332864637332,257118208,1250304000
287.26437,0.895420043625365,257888256,1251074048
288.3038151,0.956585568413531,257118208,1250304000
289.3423024,0.683905592997895,257118208,1250304000
290.3717103,0.965912717221384,257118208,1250304000
291.4349697,1.26914637969044,257122304,1250304000
292.4855746,0.743619223553976,257118208,1250304000
293.5334656,1.01665240859108,257118208,1250304000
294.5871734,0.876234810585505,257888256,1251074048
295.6553306,1.06385430568048,257118208,1250304000
296.7194145,1.26816298807058,257118208,1250304000
297.7768262,1.34333159492614,257118208,1250304000
298.8368906,1.33997004847494,257286144,1250340864
299.8667758,1.03442685562516,257286144,1250340864
300.8948221,0.690851445822326,257286144,1250340864
301.9297585,1.30387898056521,257286144,1250340864
302.9663016,0.890744875486065,257286144,1250340864
304.0001238,1.16788589337353,257286144,1250340864
305.0243073,1.04018558109061,257286144,1250340864

~~~

## VLC原生日志

~~~text
-- logger module started --
main debug: VLC media player - 3.0.23 Vetinari
main debug: Copyright © 1996-2025 the VideoLAN team
main debug: revision 3.0.23-2-0-g79128878dd
main debug: configured with /builds/videolan/vlc/extras/package/win32/../../../configure  '--enable-update-check' '--enable-lua' '--enable-faad' '--enable-flac' '--enable-theora' '--enable-avcodec' '--enable-merge-ffmpeg' '--enable-mpc' '--enable-libass' '--enable-schroedinger' '--enable-realrtsp' '--enable-live555' '--enable-shout' '--enable-goom' '--enable-sse' '--enable-mmx' '--enable-libcddb' '--enable-zvbi' '--disable-telx' '--enable-nls' '--host=i686-w64-mingw32' '--with-contrib=../contrib/i686-w64-mingw32' '--with-breakpad=https://win.crashes.videolan.org' '--enable-qt' '--enable-skins2' '--enable-dvdread' '--enable-caca' 'host_alias=i686-w64-mingw32' 'CFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'CPPFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502' 'CXXFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'PKG_CONFIG=pkg-config' 'PKG_CONFIG_LIBDIR=/usr/i686-w64-mingw32/lib/pkgconfig:/usr/lib/i686-w64-mingw32/pkgconfig'
main debug: using multimedia timers as clock source
main debug:  min period: 1 ms, max period: 1000000 ms
main debug: searching plug-in modules
main debug: loading plugins cache file D:\VideoLAN\VLC\plugins\plugins.dat
main debug: recursively browsing `D:\VideoLAN\VLC\plugins'
main debug: plug-ins loaded: 495 modules
main debug: opening config file (C:\Users\ROG\AppData\Roaming\vlc\vlcrc)
main debug: looking for logger module matching "any": 2 candidates
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r375-vlc.log'
main debug: using logger module "file"
main debug: translation test: code is "C"
main debug: looking for keystore module matching "memory": 3 candidates
main debug: using keystore module "memory"
main debug: CPU has capabilities MMX MMXEXT SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 FPU 
main debug: Creating an input for 'Media Library'
main debug: Input is a meta file: disabling unneeded options
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' gives access `file' demux `directory' path `/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf'
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for access_demux module matching "file": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug:  (path: C:\Users\ROG\AppData\Roaming\vlc\ml.xspf)
main debug: looking for access module matching "file": 27 candidates
main debug: using access module "filesystem"
main debug: looking for stream_filter module matching "prefetch,cache_read": 24 candidates
cache_read debug: Using stream method for AStream*
cache_read debug: starting pre-buffering
cache_read debug: received first data after 0 ms
cache_read debug: pre-buffering done 304 bytes in 0s - 296875 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 03b6bc38
main debug: looking for stream_filter module matching "any": 24 candidates
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for demux module matching "directory": 56 candidates
main debug: using demux module "directory"
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
main debug: no meta reader modules matched
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' successfully opened
main debug: looking for xml reader module matching "any": 1 candidates
main debug: using xml reader module "xml"
main debug: EOF reached
main debug: removing module "directory"
main debug: removing module "record"
main debug: removing module "playlist"
main debug: removing module "cache_read"
main debug: removing module "filesystem"
main debug: creating audio output
main debug: looking for audio output module matching "any": 6 candidates
mmdevice debug: using default device
mmdevice debug: version 2 session control unavailable
mmdevice debug: volume from -65.250000 dB to +0.000000 dB with 0.031250 dB increments
main debug: using audio output module "mmdevice"
main debug: keeping audio output
main debug: looking for interface module matching "rc,none": 16 candidates
oldrc debug: base: 127.0.0.1, port: 63020
main debug: net: listening to 127.0.0.1 port 63020
main debug: using interface module "oldrc"
main debug: looking for interface module matching "hotkeys,none": 16 candidates
main debug: using interface module "hotkeys"
main debug: looking for interface module matching "globalhotkeys,none": 16 candidates
main debug: using interface module "win32"
main: Running vlc with the default interface. Use 'cvlc' to use vlc without interface.
main debug: looking for interface module matching "any": 16 candidates
main debug: looking for extension module matching "any": 1 candidates
lua debug: Opening Lua Extension module
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\extensions
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\extensions
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Scanning Lua script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Script D:\VideoLAN\VLC\lua\extensions\VLSub.luac has the following capability flags: 0x5
main debug: using extension module "lua"
main debug: using interface module "qt"
main debug: processing request item: null, node: Playlist, skip: 0
main debug: rebuilding array of current - root Playlist
main debug: rebuild done - 1 items, index -1
main debug: starting playback of new item
main debug: resyncing on udp://127.0.0.1:62024
main debug: udp://127.0.0.1:62024 is at 0
main debug: creating new input thread
main debug: Creating an input for 'udp://127.0.0.1:62024'
main debug: requesting art for new input thread
main debug: using timeshift granularity of 50 MiB
qt debug: IM: Setting an input
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: looking for meta fetcher module matching "any": 1 candidates
main debug: no access_demux modules matched
main debug: creating access: udp://@127.0.0.1:62024
main debug:  (path: \\@127.0.0.1:62024)
main debug: looking for access module matching "udp": 27 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
udp debug: opening server=:0 local=127.0.0.1:62024
main debug: net: opening 127.0.0.1 datagram port 62024
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\playlist
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\playlist
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\anevia_streams.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\anevia_xml.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\appletrailers.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\bbc_co_uk.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\cue.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\dailymotion.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\jamendo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\koreus.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\liveleak.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\newgrounds.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\rockbox_fm_presets.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\soundcloud.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\twitch.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\vimeo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\vocaroo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\youtube.luac
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for udp://@127.0.0.1:62024
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for demux module matching "any": 56 candidates
main debug: looking for xml reader module matching "any": 1 candidates
main debug: using xml reader module "xml"
webvtt debug: subtitle demux discarded
ts debug: Standard set to Auto
main debug: using demux module "ts"
ts debug: DEMUX_SET_GROUP 0 00000000
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
main debug: no meta reader modules matched
main debug: `udp://@127.0.0.1:62024' successfully opened
ts debug: PATCallBack called
ts debug: new PAT ts_id=1 version=0 current_next=1
ts debug:   * number=1 pid=256
ts debug: temporary receiving program 1
ts debug: PMTCallBack called for program 1
ts debug: new PMT program number=1 version=0 pid_pcr=257
ts debug:   * pid=257 type=0x24 ISO/IEC Reserved
ts debug:    => pid 257 has now es fcc=hevc
main debug: selecting program id=1
main debug: looking for packetizer module matching "any": 25 candidates
main debug: using packetizer module "hevc"
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
ts debug: Default program is 1
ts debug:   * pid=258 type=0xf ISO/IEC 13818-7 Audio with ADTS transport
ts debug:      - SL/FMC descriptor not found/matched
ts debug:    => pid 258 has now es fcc=mp4a
main debug: looking for packetizer module matching "any": 25 candidates
mpeg4audio debug: running MPEG4 audio packetizer
mpeg4audio debug: ADTS Mode
main debug: using packetizer module "mpeg4audio"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
ts debug:   * pid=17 listening for SDT
ts debug: enabling pid 257 from program 1
ts debug: enabling pid 258 from program 1
ts debug: enabling pcr pid 257 from program 1
main debug: Buffering 0%
main debug: Buffering 6%
mpeg4audio: AAC channels: 2 samplerate: 44100
main debug: restarting module due to input format change
main debug: removing module "faad"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
faad warning: decoded zero sample
main debug: reusing audio output
main debug: looking for aout stream module matching "any": 2 candidates
main debug: restarting module due to input format change
main debug: Buffering 13%
main debug: removing module "avcodec"
main debug: Buffering 19%
main debug: Buffering 26%
main debug: Buffering 33%
main debug: Buffering 39%
main debug: Buffering 46%
main debug: Buffering 53%
main debug: Buffering 59%
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 18 ms)
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
avcodec debug: available hardware decoder output format 53 (dxva2_vld)
avcodec debug: available hardware decoder output format 118 (d3d11va_vld)
avcodec debug: available hardware decoder output format 174 (d3d11)
avcodec debug: available software decoder output format 0 (yuv420p)
avcodec debug: trying format d3d11va_vld
main debug: looking for text renderer module matching "any": 3 candidates
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
wasapi debug: modified format
main debug: using text renderer module "freetype"
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 32x32 (32x32) chroma: YUVA -> 16x16 (16x16) chroma: RGBA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: looking for video converter module matching "any": 24 candidates
wasapi debug: buffer size    : 96000 frames
wasapi debug: maximum latency: 000 ns
wasapi debug: default period : 10000000 ns
wasapi debug: minimum period : 3000000 ns
main debug: using aout stream module "wasapi"
main debug: output 'f32l' 48000 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio volume module matching "any": 2 candidates
main debug: using audio volume module "float_mixer"
main debug: input 'f32l' 44100 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio filter module matching "scaletempo": 16 candidates
scaletempo debug: format: 44100 rate, 2 nch, 4 bps, fl32
scaletempo debug: params: 30 stride, 0.200 overlap, 14 search
scaletempo debug: 1.000 scale, 1323.000 stride_in, 1323 stride_out, 1059 standing, 264 overlap, 617 search, 2204 queue, fl32 mode
main debug: using audio filter module "scaletempo"
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: looking for audio resampler module matching "any": 3 candidates
main debug: using audio resampler module "samplerate"
yuvp debug: YUVP to YUVA converter
main debug: using video converter module "yuvp"
main debug: Deinterlacing available
main debug: deinterlace -1, mode auto, is_needed 0
main debug: looking for vout window module matching "qt,any": 3 candidates
qt debug: requesting video window...
main debug: resized to 81773056x62203336
main debug: resized to 81773056x62203336
main debug: resized to 81772800x62203336
main debug: resized to 81772800x62203336
main debug: resized to 81772544x62203336
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: using vout window module "qt"
main debug: looking for inhibit module matching "any": 1 candidates
main debug: using inhibit module "inhibit"
main debug: Opening vout display wrapper
main debug: looking for vout display module matching "any": 12 candidates
direct3d11 debug: creating Vout EventThread
direct3d11 debug: Win32VoutCreateWindow
main debug: VoutDisplayEvent 'resize' 1280x720
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
direct3d11 debug: Created the D3D11 device type 1 level b100.
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:STUDIO
direct3d11 debug: Output max luminance: 400.0, colorspace RGB Rec.709 gamma:22 range:FULL, bits per pixel 10
direct3d11 debug: using colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: Using pixel format VA_NV12 for chroma DX11
main debug: VoutDisplayEvent 'resize' 1302x776
direct3d11 debug: using GPU render fence
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0b2400e0
main debug: original format sz 1280x736, of (0,0), vsz 1280x720, 4cc DX11, sar 1:1, msk r0x0 g0x0 b0x0
main debug: removing module "freetype"
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: looking for text renderer module matching "any": 3 candidates
direct3d11 debug: Detected size change 1280x720
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
main debug: using text renderer module "freetype"
main debug: looking for hw decoder module matching "any": 2 candidates
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
d3d11va debug: Reusing Direct3D11 device
d3d11va debug: CreateDevice succeed
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va debug: - 'MPEG-2 & MPEG-1 variable-length decoder' is supported
d3d11va debug: - 'MPEG-2 variable-length decoder' is supported
d3d11va debug: - 'MPEG-1 variable-length decoder, no D pictures' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'Unknown decoder 0x32fcfe3f-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'H.264 stereo high profile, mbs flag set' is supported
d3d11va debug: - 'H.264 stereo high profile' is supported
d3d11va debug: - 'H.264 variable-length decoder, no film grain technology' is supported
d3d11va debug: - 'HEVC Main profile' is supported
d3d11va debug: - 'HEVC Main 10 profile' is supported
d3d11va debug: - 'Unknown decoder 0x1a72925f-0x0c2c-0x4f15-0x96fbb17d1473603f' is supported
d3d11va debug: - 'Unknown decoder 0x15df9b21-0x06c4-0x47f1-0x841ea67c97d7f312' is supported
d3d11va debug: - 'Unknown decoder 0x4008018f-0xf537-0x4b36-0x98cf61af8a2c1a33' is supported
d3d11va debug: - 'Unknown decoder 0x0dabeffa-0x4458-0x4602-0xbc030795659d617c' is supported
d3d11va debug: - 'Unknown decoder 0x9798634d-0xfe9d-0x48e5-0xb4dadbec45b3df01' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple Profile' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC' is supported
d3d11va debug: - 'MPEG-4 Part 2 nVidia bitstream decoder' is supported
d3d11va debug: - 'Unknown decoder 0x33fcfe41-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'VP9 profile 0' is supported
d3d11va debug: - 'VP9 profile 2' is supported
d3d11va debug: - 'Unknown decoder 0xdda19dc7-0x93b5-0x49f5-0xa9b32bda28a2ce6e' is supported
d3d11va debug: - 'AV1 Main profile 8' is supported
d3d11va debug: - 'Unknown decoder 0x6affd11e-0x1d96-0x42b1-0xa21593a31f09a53d' is supported
d3d11va debug: - 'Unknown decoder 0x914c84a3-0x4078-0x4fa9-0x984ce2f262cb5c9c' is supported
d3d11va debug: - 'Unknown decoder 0x8a1a1031-0x29bc-0x46d0-0xa007e9b092ca6767' is supported
d3d11va debug: Trying to use 'HEVC Main profile' as input
d3d11va debug: NV12 output is supported for decoder HEVC Main profile.
d3d11va debug: Using output format NV12 for decoder HEVC Main profile
d3d11va debug: va_pool_SetupDecoder id 173 1280x736 count: 28
d3d11va warning: not enough decoding slices in the texture (6/28)
d3d11va debug: ID3D11VideoDecoderOutputView succeed with 28 surfaces (1280x768)
d3d11va debug: we got 2 decoder configurations
d3d11va debug: configuration[0] ConfigBitstreamRaw 1
d3d11va debug: configuration[1] ConfigBitstreamRaw 1
d3d11va debug: DxCreateDecoderSurfaces succeed
main debug: using hw decoder module "d3d11va"
avcodec: Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU, vendor 10de(NVIDIA), device 28e0, revision a1) for hardware decoding
main debug: Received first picture
main debug: Decoder wait done in 1053 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (142872): up-sampling
main warning: picture is too late to be displayed (missing 104 ms)
main warning: picture is too late to be displayed (missing 71 ms)
main warning: picture is too late to be displayed (missing 38 ms)
main debug: picture might be displayed late (missing 5 ms)
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main debug: auto hiding mouse cursor
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main warning: picture is too late to be displayed (missing 1312 ms)
main warning: picture is too late to be displayed (missing 1279 ms)
main warning: picture is too late to be displayed (missing 1246 ms)
main warning: picture is too late to be displayed (missing 1212 ms)
main warning: picture is too late to be displayed (missing 1179 ms)
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main warning: buffer too late (-480405 us): dropped
main warning: buffer too late (-457185 us): dropped
main warning: buffer too late (-433965 us): dropped
main warning: buffer too late (-410745 us): dropped
main warning: buffer too late (-387525 us): dropped
main warning: buffer too late (-364305 us): dropped
main warning: buffer too late (-341085 us): dropped
main warning: buffer too late (-317865 us): dropped
main warning: buffer too late (-294645 us): dropped
main warning: buffer too late (-271425 us): dropped
main warning: buffer too late (-248206 us): dropped
main warning: buffer too late (-224986 us): dropped
main warning: buffer too late (-201766 us): dropped
main warning: buffer too late (-178546 us): dropped
main warning: buffer too late (-157326 us): dropped
main warning: buffer too late (-134106 us): dropped
main warning: buffer too late (-110886 us): dropped
main warning: buffer too late (-87666 us): dropped
main warning: buffer too late (-64446 us): dropped
main debug: playback too late (1063848): flushing buffers
mmdevice debug: state changed: 0
wasapi debug: reset
mmdevice debug: state changed: 1
main warning: playback too late (68016): up-sampling
main debug: resampling stopped (drift: 11268 us)
main debug: accepted socket 3076 (from socket 1180)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1c074e08) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1c075358) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1c0758a8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1c0750b0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1c074e08 removed from chain
main debug: removing module "swscale"
main debug: Filter 1c075358 removed from chain
main debug: Filter 1c0758a8 removed from chain
main debug: removing module "swscale"
main debug: Filter 1c0750b0 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r375-native-2026-11-02-10h20m09s285.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1c0750b0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1c075358) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1c0750b0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1c075358 removed from chain
main error: write error: No error
main error: read error: No error
main debug: accepted socket 3464 (from socket 1180)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04f6b570) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6b818) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (04f6bac0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6b020) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04f6b570 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6b818 removed from chain
main debug: Filter 04f6bac0 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6b020 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r375-native-2026-11-02-10h21m49s929.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04f6b818) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6bac0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04f6b818 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6bac0 removed from chain
main debug: accepted socket 3276 (from socket 1180)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04f6b570) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6b818) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (04f6b2c8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6bac0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04f6b570 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6b818 removed from chain
main debug: Filter 04f6b2c8 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6bac0 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r375-native-2026-11-02-10h22m39s945.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04f6b818) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6bac0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04f6b818 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6bac0 removed from chain
main debug: accepted socket 3132 (from socket 1180)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04f6bac0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6b570) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (04f6b818) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6b020) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04f6bac0 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6b570 removed from chain
main debug: Filter 04f6b818 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6b020 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r375-native-2026-11-02-10h24m12s696.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04f6b2c8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04f6bac0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04f6b2c8 removed from chain
main debug: removing module "swscale"
main debug: Filter 04f6bac0 removed from chain
main debug: accepted socket 3132 (from socket 1180)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
ts debug: Can't read TS packet at 203802716
main debug: EOF reached
main error: read error: No error
main warning: picture is too late to be displayed (missing 310 ms)
main warning: picture is too late to be displayed (missing 277 ms)
main warning: picture is too late to be displayed (missing 244 ms)
main warning: picture is too late to be displayed (missing 210 ms)
main warning: picture is too late to be displayed (missing 177 ms)
main debug: killing decoder fourcc `hevc'
main debug: removing module "avcodec"
main debug: saving a free vout
main debug: reusing provided vout
main debug: removing module "hevc"
mmdevice debug: state changed: 0
wasapi debug: reset
main debug: killing decoder fourcc `mp4a'
main debug: removing module "faad"
wasapi debug: reset
main debug: removing module "samplerate"
main debug: removing module "scaletempo"
main debug: removing module "float_mixer"
main debug: keeping audio output
main debug: removing module "mpeg4audio"
main debug: removing module "ts"
main debug: Program doesn't contain anymore ES
main debug: removing module "record"
main debug: removing module "prefetch"
main debug: removing module "udp"
main debug: dead input
qt debug: IM: Deleting the input
main debug: nothing to play
main debug: destroying useless vout
main debug: removing module "direct3d11"
direct3d11 debug: Direct3D11 resources destroyed
direct3d11 debug: Direct3D11 device adapter closed
direct3d11 error: SetThumbNailClip failed: 0x800706f4
direct3d11 debug: Win32 Vout EventThread terminating
direct3d11 debug: Win32VoutCloseWindow
direct3d11 debug: WinProc WM_DESTROY
main debug: removing module "inhibit"
qt debug: releasing video...
qt debug: video widget is released
main debug: removing module "freetype"
main debug: removing module "yuvp"
main debug: removing module "swscale"
main debug: removing module "mmdevice"
qt debug: requesting exit...
qt debug: waiting for UI thread...
qt debug: video widget is orphaned
main debug: exiting
main debug: no exit handler
qt debug: QApp exec() finished
qt debug: video widget is orphaned
qt debug: Killing extension dialog provider
qt debug: ExtensionsDialogProvider is quitting...
main debug: removing module "lua"
main debug: removing module "win32"
main debug: removing module "hotkeys"
main debug: removing module "oldrc"
main debug: destroying
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp17628
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

## 本轮逐项清理清单

- composition-r375-cli-error.log (1966 bytes)
- composition-r375-cli.log (1234534 bytes)
- composition-r375-monitor.log (14955 bytes)
- composition-r375-native-2026-11-02-10h20m09s285.png (881947 bytes)
- composition-r375-native-2026-11-02-10h21m49s929.png (12152 bytes)
- composition-r375-native-2026-11-02-10h22m39s945.png (305642 bytes)
- composition-r375-native-2026-11-02-10h24m12s696.png (12152 bytes)
- composition-r375-reentry-stdout.log (540 bytes)
- composition-r375-reentry.log (29521 bytes)
- composition-r375-source0-stdout.log (540 bytes)
- composition-r375-source0.log (29589 bytes)
- composition-r375-source1-stdout.log (335 bytes)
- composition-r375-source1.log (28926 bytes)
- composition-r375-source2-stdout.log (335 bytes)
- composition-r375-source2.log (29062 bytes)
- composition-r375-vlc.log (43559 bytes)

清理完成：上述16项均按清单逐项删除，本轮产物残留0；指定120秒测试源保留。未运行远程测试，未产生远程本轮产物。本轮全部精确PID与端口已在同启动环境核验释放。

## 独立时序复核

Canvas cursor1678 compose12.7136ms、同次batch completion12.5574ms，threadCPU0且输入query全ready；不能等同纯GPU时间。Encoder pts3675调用20.5016ms，最大提交间隙63.3754ms属于pts6907到6908，是另一事件。Scheduler video543等待audio49.5982ms，恢复时剩余dispatch93.671334ms。Sender最迟seq145047没有对应单次trace，现有详细记录是最终seq175637，仅0.175566ms；materializer最差seq91268又是另一事件。不能混合最大值拼因果链；下一证据应保存同一最差sequence的原预约/deadline、入队、wait开始返回、clock采样及socket开始结束。当前未实现该完整最差样本，不将r371/r372判已修复。

### 最差成功提交样本设计范围

依据 Linux 官方 [ftrace tracing_max_latency](https://www.kernel.org/doc/html/latest/trace/ftrace.html) 保存最大延迟对应 trace 的做法，本次仅补足同序号诊断快照，不修改调度算法。采用 sender owner-thread 固定大小值对象，复用 ReservationObservation 与 wait 观测；无媒体引用、无新增线程或热路径日志。选择依据只能是完整 Submitted、scope/commit 与 pacing markSubmitted 成功后权威 maximumSubmitLateness/worstLateGlobalSequence 的更新，不另算 lateness。

样本保存 generation/endpoint/sequence、原 release/deadline/protocol deadline、pacing reservation、已有激活/permit/snapshot/wait 时间与提交区间；该区间含 socket 前检查，不宣称 socket syscall 时间。现无同序号 materialized/入队时间，日志明确 unavailable。start 与 controller 一同清空；generation purge/plan rebind 保留累计样本。partial/error 不记录成功样本；若其更新累计最差统计，最终日志明确成功样本不匹配当前 pacing 最差。仅沿既有代次清理和结束诊断出口输出，格式化失败不得替换生产结果。此项是取证补全，不是 r371/r372 根因修复或验收通过。
