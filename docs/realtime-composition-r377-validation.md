# r377：源域完整拓扑与最差提交样本回归

日期：2026-11-02。四阶段画面与停止观察完成，完整验收尚未通过。

本轮版本：Input/Protocol声明、源域positions与完整实际incident核验21文件，以及最差成功提交样本5文件，分别双独立局部PASS后，合并Release clean-first/all-target779目标构建成功。新实际拓扑校验已通过并进入输出。完整输出域、共享owner费用、联合solver和SourcePresentation仍未完成。

## 实际启动命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r377 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r377-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
```

PID：source0=12304、source1=16088、source2=34312、CLI=25152、VLC=11100。启动前同环境CIM/端口无残留；启动后UDP62024与TCP63020归本轮VLC11100。

## 画面观察

- 已查看 composition-r377-native-2026-11-02-10h42m58s961.png：三格游戏画面、右下黑格。

实际VLC原生快照命令：
```powershell
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
```

- 已查看 composition-r377-native-2026-11-02-10h44m39s166.png：初始源完整结束后全黑，CLI持续输出。
- 重入源PID19712，实际命令：
```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```
- 已查看 composition-r377-native-2026-11-02-10h45m45s947.png：所选源恢复到左下格，其余黑格。期间pressure计数1，workerErrors/errors仍0；按已核验代码该计数表示新waiter背压，不直接等同致命错误。

## 结果与边界

已查看 composition-r377-native-2026-11-02-10h47m04s908.png：重入完整结束后再次全黑。三初始源PID12304/16088/34312及重入PID19712均120秒/3600帧退出0；CLI25152 end_reason=caller_stop、退出0。四阶段画面及调用方停止得到证据，不等于完整验收PASS。

最终workerErrors/errors/stalledIntervals为0，226085次预约/释放匹配，current bytes/objects归零；pressure计数1为容量等待，encoder记录最长payload等待10.0033ms并恢复。平均整机进程CPU1.554059%、峰4.690832%；WS初始208101376、峰271941632、最终262455296 bytes。短时数据不证明长期稳定。

新最差成功提交样本已真实输出：seq165387、matches_pacing_worst=1、sample_available=1，与汇总lateness34.895260ms一致。计划wait211440ns，Windows API区间7079633664200→7079668760700为35.0965ms；due=-2115，timer返回1，PID25152/TID30940。Encoder最慢调用7079630671100→7079668964600(38.2935ms,pts7892)与该wait区间重叠；只能证明并发长停留，不足以归因调度或驱动。sender本轮deadline/partial/ambiguous失败0，175695datagrams、203973796payload bytes，delivery仍not_proven；r376发送期限根因未关闭。

尚缺完整输出域/residence/联合solver、SourcePresentation路径、内容级A/V与静音、Windows完整2～4源及CAPI/RKMPP矩阵。不能把局部运行成功记为整体验收完成。

全部本轮六个PID及UDP62024/TCP63020已在同启动环境核验无残留。实际停止命令：
```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=25152"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r377*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R377Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R377Console]::FreeConsole() | Out-Null
if(-not [R377Console]::AttachConsole(25152)){throw 'AttachConsole failed'}
try {
[R377Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R377Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 25152 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R377Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R377Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 11100){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

## 非周期CLI诊断

~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 10:42:15.794] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-02 10:42:15.796] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-02 10:42:15.796] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-02 10:42:15.892] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:42:15.893] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:42:15.894] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r377.source.0 decoder=h264
[2026-11-02 10:42:20.280] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:42:20.284] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:42:20.296] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:42:20.372] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:42:20.372] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:42:20.372] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r377.source.1 decoder=h264
[2026-11-02 10:42:20.454] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:42:20.454] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:42:20.454] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r377.source.2 decoder=h264
[2026-11-02 10:42:20.545] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:42:20.586] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:42:20.586] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-02 10:42:20.594] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:42:20.594] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:42:20.594] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:42:20.604] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:42:20.605] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:42:20.605] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:42:20.613] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:42:20.613] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:42:20.613] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:42:20.650] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=832028640 high_water=849249435
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-02 10:42:20.652] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-02 10:42:20.653] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.654] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.655] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.656] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.657] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.658] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.658] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.658] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.658] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.658] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.658] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.658] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.659] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.659] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:42:20.659] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.659] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.659] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.659] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.659] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.660] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.661] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.662] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:42:20.663] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:42:20.664] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:42:20.665] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:42:20.665] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-02 10:42:20.665] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:42:20.665] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:42:20.665] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:42:20.665] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:42:20.665] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-02 10:42:20.666] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-02 10:42:20.666] [info] [graph][runtime.node] register node=1 name=composition-r377.output.video.codec_resolver kind=CodecResolver
[2026-11-02 10:42:20.666] [info] [graph][runtime.node] register node=2 name=composition-r377.output.video.encode kind=VideoEncode
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=3 name=composition-r377.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=4 name=composition-r377.output.audio.encode kind=AudioEncode
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=5 name=composition-r377.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=8 name=composition-r377.source_0.input kind=RawRtpInput
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=9 name=composition-r377.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=10 name=composition-r377.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=11 name=composition-r377.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:42:20.667] [info] [graph][runtime.node] register node=12 name=composition-r377.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=13 name=composition-r377.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=14 name=composition-r377.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=15 name=composition-r377.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=16 name=composition-r377.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=17 name=composition-r377.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=18 name=composition-r377.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=20 name=composition-r377.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=21 name=composition-r377.source_0.video.codec_resolver kind=CodecResolver
[2026-11-02 10:42:20.668] [info] [graph][runtime.node] register node=22 name=composition-r377.source_0.video.decode kind=VideoDecode
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=23 name=composition-r377.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=24 name=composition-r377.source_0.video.framerate kind=VideoFrameRate
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=25 name=composition-r377.source_0.video.preparation kind=VideoPreparation
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=26 name=composition-r377.source_0.video.filter kind=VideoFilter
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=27 name=composition-r377.source_1.input kind=RawRtpInput
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=28 name=composition-r377.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=29 name=composition-r377.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=30 name=composition-r377.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=31 name=composition-r377.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:42:20.669] [info] [graph][runtime.node] register node=32 name=composition-r377.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=33 name=composition-r377.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=34 name=composition-r377.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=35 name=composition-r377.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=36 name=composition-r377.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=37 name=composition-r377.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=39 name=composition-r377.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=40 name=composition-r377.source_1.video.codec_resolver kind=CodecResolver
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=41 name=composition-r377.source_1.video.decode kind=VideoDecode
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=42 name=composition-r377.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=43 name=composition-r377.source_1.video.framerate kind=VideoFrameRate
[2026-11-02 10:42:20.670] [info] [graph][runtime.node] register node=44 name=composition-r377.source_1.video.preparation kind=VideoPreparation
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=45 name=composition-r377.source_1.video.filter kind=VideoFilter
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=46 name=composition-r377.source_2.video.input kind=RawRtpInput
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=47 name=composition-r377.source_2.audio.input kind=RawRtpInput
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=48 name=composition-r377.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=49 name=composition-r377.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=50 name=composition-r377.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=51 name=composition-r377.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=52 name=composition-r377.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=53 name=composition-r377.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=54 name=composition-r377.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:42:20.671] [info] [graph][runtime.node] register node=55 name=composition-r377.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=56 name=composition-r377.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=57 name=composition-r377.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=58 name=composition-r377.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=59 name=composition-r377.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=60 name=composition-r377.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=62 name=composition-r377.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=63 name=composition-r377.source_2.video.codec_resolver kind=CodecResolver
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=64 name=composition-r377.source_2.video.decode kind=VideoDecode
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=65 name=composition-r377.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:42:20.672] [info] [graph][runtime.node] register node=66 name=composition-r377.source_2.video.framerate kind=VideoFrameRate
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=67 name=composition-r377.source_2.video.preparation kind=VideoPreparation
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=68 name=composition-r377.source_2.video.filter kind=VideoFilter
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=69 name=composition-r377.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=70 name=composition-r377.source_2.audio.encode.decode kind=AudioDecode
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=71 name=composition-r377.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=72 name=composition-r377.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=73 name=composition-r377.source_2.audio.encode.resample kind=AudioResample
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=74 name=composition-r377.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=75 name=composition-r377.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-02 10:42:20.673] [info] [graph][runtime.node] register node=76 name=composition-r377.output.mpegts.mux kind=FileMux
[2026-11-02 10:42:20.674] [info] [graph][runtime.node] register node=77 name=composition-r377.output.mpegts.plan.source kind=Unknown
[2026-11-02 10:42:20.674] [info] [graph][runtime.node] register node=78 name=composition-r377.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-02 10:42:20.674] [info] [graph][runtime.node] register node=79 name=composition-r377.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-02 10:42:20.674] [info] [graph][runtime.node] register node=80 name=composition-r377.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-02 10:42:20.675] [info] [graph][runtime.node] register node=81 name=composition-r377.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-02 10:42:20.675] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-02 10:42:20.675] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-02 10:42:20.677] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:20.678] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-02 10:42:20.678] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:42:20.680] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-02 10:42:20.681] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-02 10:42:20.681] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:42:20.690] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-02 10:42:20.690] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:42:20.690] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:20.692] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:42:20.692] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 10:42:20.693] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816410431300 group=composition-r377.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5508447100 video_count=27 min_presentation_ns=-4023000000 max_end_ns=-3123000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.693] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816410563800 group=composition-r377.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.695] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:42:20.695] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 10:42:20.697] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816413778600 group=composition-r377.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4013000000 epoch_master_release_ns=5511854400 video_count=15 min_presentation_ns=-4013000000 max_end_ns=-3513000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.697] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816414019100 group=composition-r377.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.701] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2167049797 dts=2167049797 size=317016 generation=1 sequence=15
[2026-11-02 10:42:20.702] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816419044500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.707] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-02 10:42:20.707] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:20.707] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:42:20.707] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:42:20.708] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-02 10:42:20.708] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 10:42:20.708] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-02 10:42:20.708] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-11-02 10:42:20.708] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-02 10:42:20.708] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2497335917 dts=2497335917 size=317016 generation=1 sequence=15
[2026-11-02 10:42:20.709] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816425582000 group=composition-r377.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5524019600 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.709] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816425990700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.709] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816426140700 group=composition-r377.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.709] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=452936633 dts=452936633 size=317016 generation=1 sequence=15
[2026-11-02 10:42:20.710] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816426863400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.710] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-02 10:42:20.711] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-11-02 10:42:20.736] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:42:20.736] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816453050300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.736] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2167049797 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-02 10:42:20.736] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816453399500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.736] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816453523800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.736] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000029483D824A0 stream=Video payload=Frame pts=2167049797 dts=2167049797 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816453713400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000029483D824A0 stream=Video payload=Frame pts=2167049797 dts=2167049797 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000029483D824A0 stream=Video payload=Frame pts=2167049797 dts=2167049797 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000029483D82C80 stream=Video payload=Frame pts=2167049797 dts=2167049797 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816454216900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:42:20.737] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000029483D82C80 stream=Video payload=Frame pts=2167049797 dts=2167049797 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.738] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816455685900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=452936633 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816455941800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816456046100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000029483D83460 stream=Video payload=Frame pts=452936633 dts=452936633 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816456269200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000029483D83460 stream=Video payload=Frame pts=452936633 dts=452936633 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000029483D83460 stream=Video payload=Frame pts=452936633 dts=452936633 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.739] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:42:20.740] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816456689900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.740] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000029483D83850 stream=Video payload=Frame pts=452936633 dts=452936633 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.740] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000029483D83850 stream=Video payload=Frame pts=452936633 dts=452936633 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.740] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:42:20.741] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:42:20.741] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816457827600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.741] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2497335917 format=cuda size=1280x720 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-11-02 10:42:20.741] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816458191600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.741] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816458364700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.741] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000029483D83460 stream=Video payload=Frame pts=2497335917 dts=2497335917 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.742] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000029483D83460 stream=Video payload=Frame pts=2497335917 dts=2497335917 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.742] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:42:20.742] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816458946800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.743] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000029483D83460 stream=Video payload=Frame pts=2497335917 dts=2497335917 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.742] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816459312900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.742] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000029489B886C0 stream=Video payload=Frame pts=2497335917 dts=2497335917 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.743] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:42:20.743] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000029489B886C0 stream=Video payload=Frame pts=2497335917 dts=2497335917 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:42:20.779] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=41
[2026-11-02 10:42:20.804] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=61
[2026-11-02 10:42:20.804] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816521555000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.806] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816522766700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.832] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816549274700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.833] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=92
[2026-11-02 10:42:20.833] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=722350 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816550404900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.833] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816550485200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.834] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=722350 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816550921900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.834] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=722350 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816551120700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.834] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-02 10:42:20.834] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816551512200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.835] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=722350 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816551768900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.836] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6816552588700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.836] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=832445 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816552729100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.836] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=832445 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816553529800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.837] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08b20000 drm_prime=0 software_frame=0
[2026-11-02 10:42:20.836] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=27 audio_count=23 first_video_sequence=15
[2026-11-02 10:42:20.837] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=832445 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816553595000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.836] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=150979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816553552400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.837] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=832445 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816554481200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.837] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816554523000 group=composition-r377.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5719499968 video_count=27 min_presentation_ns=-4023000000 max_end_ns=-3123000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.838] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=722350 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816554525400 group=composition-r377.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.838] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=150979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816554819200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.839] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a90000 drm_prime=0 software_frame=0
[2026-11-02 10:42:20.838] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 10:42:20.838] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816555388500 group=composition-r377.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5719499968 video_count=27 min_presentation_ns=-4023000000 max_end_ns=-3123000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.839] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-02 10:42:20.839] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r377.source.2.input.video generation=1 steady_ns=6816555642400 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:42:20.839] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r377.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=2167049797 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816555641100 group=composition-r377.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5508447100 video_count=27 min_presentation_ns=-4023000000 max_end_ns=-3123000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=27 release_count=29 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.838] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=150979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816554835400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.839] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=832445 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816556307100 group=composition-r377.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.839] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816556314400 group=composition-r377.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4013000000 epoch_master_release_ns=5721442668 video_count=15 min_presentation_ns=-4013000000 max_end_ns=-3513000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.840] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-02 10:42:20.840] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-11-02 10:42:20.841] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5721955751 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:42:20.841] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816558135500 group=composition-r377.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4013000000 epoch_master_release_ns=5721442668 video_count=15 min_presentation_ns=-4013000000 max_end_ns=-3513000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.841] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r377.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable pts=2497335917 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816558142700 group=composition-r377.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4013000000 epoch_master_release_ns=5511854400 video_count=15 min_presentation_ns=-4013000000 max_end_ns=-3513000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.842] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:42:20.841] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=150979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816557582300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.842] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5768395660 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:42:20.841] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r377.source.0.input.video generation=1 steady_ns=6816558187200 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:42:20.843] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 10:42:20.843] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816560288600 group=composition-r377.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5725404468 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.843] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816560499100 group=composition-r377.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5725404468 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.843] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=150979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6816560341100 group=composition-r377.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.844] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r377.source.1.input.video generation=1 steady_ns=6816561146900 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:42:20.844] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r377.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=452936633 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6816561171100 group=composition-r377.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5524019600 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:42:20.844] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-02 10:42:20.844] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:42:20.845] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=362100 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:42:20.845] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=36700 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-02 10:42:20.845] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r377.source.0.input.video generation=1 steady_ns=6816562137100 source_sequence=15 presentation_ns=-4013000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:42:20.845] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-02 10:42:20.845] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=759800 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:42:20.845] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r377.source.1.input.video generation=1 steady_ns=6816562392400 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:42:20.845] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-02 10:42:20.846] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r377.source.2.input.video generation=1 steady_ns=6816562733600 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:42:20.846] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-02 10:42:20.847] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=93500 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-02 10:42:20.847] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=451000 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-02 10:42:20.847] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-02 10:42:20.851] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-02 10:42:20.852] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-02 10:42:20.852] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-02 10:42:20.852] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5726615768 dispatch_master_ns=5726615768 now_ns=5567722000 node=74 identity_kind=output identity="composition-r377.output" canonical_sequence=1 canonical_created_steady_ns=6816569116700 head_observed_steady_ns=6816569181600 codec_receive_begin_steady_ns=6816568663400 codec_receive_end_steady_ns=6816568664000
[2026-11-02 10:42:20.852] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5568006500 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5726615768 present_dispatch_master_ns=5726615768
[2026-11-02 10:42:20.893] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-02 10:42:20.893] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5726615768 dispatch_master_ns=5726615768 now_ns=5608714500 node=74 identity_kind=output identity="composition-r377.output" canonical_sequence=1 canonical_created_steady_ns=6816610141800 head_observed_steady_ns=6816610174000 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 10:42:21.022] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-02 10:42:21.449] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6813293619 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:42:22.470] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7834971624 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:42:23.492] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8856649628 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:42:24.537] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=9901547587 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:42:25.282] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576145 ntp_fraction=1211180777 arrival_ns=6820998833500
[2026-11-02 10:42:25.283] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:42:25.283] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:25.312] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576145 ntp_fraction=1340029796 arrival_ns=6821028838500
[2026-11-02 10:42:25.312] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:42:25.313] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:25.312] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576145 ntp_fraction=1340029796 arrival_ns=6821029165900
[2026-11-02 10:42:25.313] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:42:25.314] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:25.327] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576145 ntp_fraction=1400159338 arrival_ns=6821043525900
[2026-11-02 10:42:25.327] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:42:25.327] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:25.558] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10923225591 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:42:26.580] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11944903596 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:42:27.602] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12966581600 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:42:28.647] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=14011479559 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:42:29.668] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=15033157564 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:42:30.286] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576150 ntp_fraction=1224065679 arrival_ns=6826002492500
[2026-11-02 10:42:30.286] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:42:30.286] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:30.317] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576150 ntp_fraction=1357209665 arrival_ns=6826033448200
[2026-11-02 10:42:30.317] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:42:30.317] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:30.318] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576150 ntp_fraction=1365799600 arrival_ns=6826035367800
[2026-11-02 10:42:30.319] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:42:30.319] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:30.349] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576150 ntp_fraction=1494648619 arrival_ns=6826065206200
[2026-11-02 10:42:30.349] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:42:30.349] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:30.690] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=16054835568 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:42:31.735] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=491 source_master_ns=17099733528 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:42:32.757] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18121411532 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:42:33.779] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19143089537 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:42:34.801] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20164767541 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:42:35.315] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576155 ntp_fraction=1348619730 arrival_ns=6831031129100
[2026-11-02 10:42:35.315] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:42:35.315] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:35.344] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576155 ntp_fraction=1477468749 arrival_ns=6831061109600
[2026-11-02 10:42:35.344] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:42:35.345] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:35.344] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576155 ntp_fraction=1477468749 arrival_ns=6831061338000
[2026-11-02 10:42:35.345] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:42:35.346] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:35.359] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576155 ntp_fraction=1541893259 arrival_ns=6831076381500
[2026-11-02 10:42:35.360] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:42:35.360] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:35.846] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21209665500 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:42:36.867] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22231343505 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:42:37.888] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23253021509 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:42:38.911] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24274699514 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:42:39.956] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25319597473 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:42:40.315] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576160 ntp_fraction=1352914698 arrival_ns=6836031918700
[2026-11-02 10:42:40.315] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:42:40.315] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:40.353] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576160 ntp_fraction=1516123455 arrival_ns=6836069843900
[2026-11-02 10:42:40.354] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:42:40.354] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:40.353] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576160 ntp_fraction=1516123455 arrival_ns=6836070469500
[2026-11-02 10:42:40.354] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:42:40.354] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:40.381] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576160 ntp_fraction=1636382539 arrival_ns=6836098067800
[2026-11-02 10:42:40.381] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:42:40.382] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:40.976] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26341275478 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:42:41.998] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27362953482 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:42:43.043] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=978 source_master_ns=28407851441 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:42:44.066] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29429529446 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:42:45.086] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30451207450 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:42:45.346] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576165 ntp_fraction=1481763717 arrival_ns=6841062411600
[2026-11-02 10:42:45.346] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:42:45.347] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:45.371] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576165 ntp_fraction=1593432866 arrival_ns=6841087822400
[2026-11-02 10:42:45.371] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:42:45.371] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:45.385] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576165 ntp_fraction=1649267441 arrival_ns=6841100911300
[2026-11-02 10:42:45.385] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:42:45.385] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:45.385] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576165 ntp_fraction=1653562408 arrival_ns=6841101830900
[2026-11-02 10:42:45.386] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:42:45.386] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:46.109] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31472885455 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:42:47.154] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32517783414 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:42:48.175] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33539461419 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:42:49.196] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34561139423 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:42:50.241] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1288 source_master_ns=35606037382 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:42:50.348] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576170 ntp_fraction=1494648619 arrival_ns=6846065166500
[2026-11-02 10:42:50.349] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:42:50.349] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:50.379] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576170 ntp_fraction=1627792605 arrival_ns=6846096398600
[2026-11-02 10:42:50.380] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:42:50.380] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:50.393] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576170 ntp_fraction=1687922147 arrival_ns=6846109788400
[2026-11-02 10:42:50.394] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:42:50.394] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:50.409] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576170 ntp_fraction=1752346656 arrival_ns=6846125360600
[2026-11-02 10:42:50.409] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:42:50.409] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:51.263] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36627715387 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:42:52.286] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37649393392 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:42:53.306] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38671071396 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:42:54.351] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39715969355 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:42:55.373] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40737647360 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:42:55.380] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576175 ntp_fraction=1632087572 arrival_ns=6851096976600
[2026-11-02 10:42:55.380] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:42:55.380] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:55.395] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576175 ntp_fraction=1692217114 arrival_ns=6851111688900
[2026-11-02 10:42:55.395] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:42:55.395] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:55.410] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576175 ntp_fraction=1760936591 arrival_ns=6851126707200
[2026-11-02 10:42:55.410] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:42:55.410] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:55.425] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576175 ntp_fraction=1821066133 arrival_ns=6851141346500
[2026-11-02 10:42:55.425] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:42:55.426] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:42:56.394] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41759325364 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:42:57.440] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1598 source_master_ns=42804223324 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:42:58.461] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43825901328 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:42:59.483] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44847579333 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:43:00.395] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576180 ntp_fraction=1696512081 arrival_ns=6856111897500
[2026-11-02 10:43:00.395] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576180 ntp_fraction=1696512081 arrival_ns=6856112041100
[2026-11-02 10:43:00.395] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:00.396] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:00.396] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:00.396] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:00.450] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576180 ntp_fraction=1932735283 arrival_ns=6856167034900
[2026-11-02 10:43:00.451] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:00.451] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:00.450] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576180 ntp_fraction=1932735283 arrival_ns=6856167146700
[2026-11-02 10:43:00.451] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:00.451] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:00.504] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45869257337 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 10:43:01.550] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46914155296 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:43:02.571] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47935833301 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:43:03.593] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48957511305 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:43:04.638] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1908 source_master_ns=50002409265 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:43:05.409] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576185 ntp_fraction=1752346656 arrival_ns=6861125499900
[2026-11-02 10:43:05.409] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:05.409] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:05.409] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576185 ntp_fraction=1756641624 arrival_ns=6861125767400
[2026-11-02 10:43:05.410] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:05.410] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:05.454] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576185 ntp_fraction=1945620185 arrival_ns=6861170573600
[2026-11-02 10:43:05.454] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:05.454] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:05.455] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576185 ntp_fraction=1949915152 arrival_ns=6861171182000
[2026-11-02 10:43:05.455] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:05.456] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:05.659] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=51024087269 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:43:06.681] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=52045765274 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:43:07.703] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=53067443278 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:43:08.750] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=54112341237 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:43:09.769] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=55134019242 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:43:10.416] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576190 ntp_fraction=1786706395 arrival_ns=6866132662200
[2026-11-02 10:43:10.416] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:10.416] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:10.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576190 ntp_fraction=1786706395 arrival_ns=6866133409500
[2026-11-02 10:43:10.417] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:10.417] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:10.458] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576190 ntp_fraction=1962800054 arrival_ns=6866174610100
[2026-11-02 10:43:10.458] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:10.458] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:10.458] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576190 ntp_fraction=1967095021 arrival_ns=6866174914900
[2026-11-02 10:43:10.459] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:10.459] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:10.792] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=56155697246 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:43:11.842] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2218 source_master_ns=57200595206 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:43:12.858] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=58222273210 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:43:13.881] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=59243951215 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:43:14.903] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=60265629219 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:43:15.430] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576195 ntp_fraction=1846835937 arrival_ns=6871147134500
[2026-11-02 10:43:15.430] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:15.430] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:15.447] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576195 ntp_fraction=1915555414 arrival_ns=6871163474300
[2026-11-02 10:43:15.448] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:15.448] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:15.463] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576195 ntp_fraction=1984274890 arrival_ns=6871179080200
[2026-11-02 10:43:15.464] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:15.464] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:15.479] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576195 ntp_fraction=2052994367 arrival_ns=6871194662000
[2026-11-02 10:43:15.479] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:15.479] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:15.947] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=61310527178 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:43:16.967] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=62332205183 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:43:17.989] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=63353883187 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:43:19.035] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2528 source_master_ns=64398781147 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:43:20.062] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65420459151 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:43:20.441] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576200 ntp_fraction=1894080577 arrival_ns=6876157879200
[2026-11-02 10:43:20.441] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:20.442] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:20.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576200 ntp_fraction=2087354105 arrival_ns=6876202753600
[2026-11-02 10:43:20.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576200 ntp_fraction=2087354105 arrival_ns=6876202896300
[2026-11-02 10:43:20.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576200 ntp_fraction=2087354105 arrival_ns=6876203470000
[2026-11-02 10:43:20.487] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:20.487] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:20.487] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:20.487] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:20.487] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:20.488] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:21.079] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66442137156 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:43:22.099] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67463815160 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:43:23.146] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=68508713119 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:43:24.168] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69530391124 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:43:25.188] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70552069128 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:43:25.468] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576205 ntp_fraction=2010044694 arrival_ns=6881185158900
[2026-11-02 10:43:25.469] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:25.469] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:25.514] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576205 ntp_fraction=2207613190 arrival_ns=6881231506100
[2026-11-02 10:43:25.515] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:25.516] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:25.516] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576205 ntp_fraction=2211908157 arrival_ns=6881232207700
[2026-11-02 10:43:25.516] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:25.517] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:25.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576205 ntp_fraction=2211908157 arrival_ns=6881231981000
[2026-11-02 10:43:25.517] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:25.517] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:26.232] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2838 source_master_ns=71596967088 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:43:27.254] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72618645092 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:43:28.276] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73640323097 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:43:29.297] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74662001101 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:43:30.342] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75706899060 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:43:30.479] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576210 ntp_fraction=2057289334 arrival_ns=6886195926700
[2026-11-02 10:43:30.479] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:30.480] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:30.524] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576210 ntp_fraction=2250562863 arrival_ns=6886241292600
[2026-11-02 10:43:30.525] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:30.525] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:30.540] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576210 ntp_fraction=2319282339 arrival_ns=6886256809300
[2026-11-02 10:43:30.540] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576210 ntp_fraction=2319282339 arrival_ns=6886256894400
[2026-11-02 10:43:30.540] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:30.540] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:30.541] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:30.541] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:31.364] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76728577065 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:43:32.386] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77750255070 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:43:33.436] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3148 source_master_ns=78795153029 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:43:34.452] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79816831033 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:43:35.474] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80838509038 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:43:35.483] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576215 ntp_fraction=2074469203 arrival_ns=6891199943500
[2026-11-02 10:43:35.483] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:35.484] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:35.527] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576215 ntp_fraction=2263447764 arrival_ns=6891244366000
[2026-11-02 10:43:35.528] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:35.528] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:35.542] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576215 ntp_fraction=2327872274 arrival_ns=6891258974700
[2026-11-02 10:43:35.543] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:35.542] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576215 ntp_fraction=2327872274 arrival_ns=6891259064400
[2026-11-02 10:43:35.543] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:35.543] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:35.543] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:36.495] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81860187042 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:43:37.541] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82905085002 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:43:38.562] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83926763006 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:43:39.584] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84948441011 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:43:40.487] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576220 ntp_fraction=2091649073 arrival_ns=6896203827800
[2026-11-02 10:43:40.487] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:40.487] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:40.547] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576220 ntp_fraction=2345052143 arrival_ns=6896263248800
[2026-11-02 10:43:40.547] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576220 ntp_fraction=2345052143 arrival_ns=6896263483200
[2026-11-02 10:43:40.547] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:40.547] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:40.547] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:40.547] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:40.562] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576220 ntp_fraction=2409476653 arrival_ns=6896278380800
[2026-11-02 10:43:40.562] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:40.562] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:40.606] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85970119015 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:43:41.650] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=87015016974 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:43:42.672] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=88036694979 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:43:43.694] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=89058372983 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:43:44.738] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3635 source_master_ns=90103270943 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:43:45.509] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576225 ntp_fraction=2186138353 arrival_ns=6901225710000
[2026-11-02 10:43:45.509] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:45.509] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:45.553] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576225 ntp_fraction=2370821947 arrival_ns=6901269650500
[2026-11-02 10:43:45.553] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:45.553] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:45.568] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576225 ntp_fraction=2439541424 arrival_ns=6901285115500
[2026-11-02 10:43:45.568] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:45.569] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:45.568] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576225 ntp_fraction=2439541424 arrival_ns=6901285135200
[2026-11-02 10:43:45.569] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:45.569] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:45.760] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=91124948947 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:43:46.782] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=92146626952 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:43:47.804] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=93168304956 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:43:48.848] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=94213202915 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:43:49.872] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=95234880920 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:43:50.537] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576230 ntp_fraction=2306397437 arrival_ns=6906254368500
[2026-11-02 10:43:50.538] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:50.538] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:50.568] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576230 ntp_fraction=2430951489 arrival_ns=6906283002300
[2026-11-02 10:43:50.568] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:50.568] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:50.582] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576230 ntp_fraction=2495375998 arrival_ns=6906298110600
[2026-11-02 10:43:50.582] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:50.582] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:50.584] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576230 ntp_fraction=2503965933 arrival_ns=6906300279800
[2026-11-02 10:43:50.584] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:50.584] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:50.892] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=96256558924 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:43:51.937] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3945 source_master_ns=97301456884 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:43:52.959] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=98323134888 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:43:53.981] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=99344812893 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:43:55.001] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=100366490897 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:43:55.548] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576235 ntp_fraction=2353642078 arrival_ns=6911264653600
[2026-11-02 10:43:55.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:43:55.548] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:55.577] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576235 ntp_fraction=2473901162 arrival_ns=6911293371100
[2026-11-02 10:43:55.577] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:43:55.577] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:55.591] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576235 ntp_fraction=2534030704 arrival_ns=6911307647000
[2026-11-02 10:43:55.591] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:43:55.591] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:55.621] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576235 ntp_fraction=2667174690 arrival_ns=6911338091500
[2026-11-02 10:43:55.622] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:43:55.622] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:43:56.047] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101411388856 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 10:43:57.069] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102433066861 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 10:43:58.090] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103454744865 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 10:43:59.137] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4255 source_master_ns=104499642825 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 10:44:00.157] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105521320829 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 10:44:00.561] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576240 ntp_fraction=2409476653 arrival_ns=6916278157500
[2026-11-02 10:44:00.562] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:44:00.562] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:00.577] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576240 ntp_fraction=2478196129 arrival_ns=6916294345400
[2026-11-02 10:44:00.578] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:44:00.578] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:00.591] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576240 ntp_fraction=2538325671 arrival_ns=6916308449100
[2026-11-02 10:44:00.592] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:44:00.592] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:00.662] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576240 ntp_fraction=2838973382 arrival_ns=6916378769500
[2026-11-02 10:44:00.662] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:44:00.662] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:01.178] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106542998834 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 10:44:02.200] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107564676838 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 10:44:03.245] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108609574797 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 10:44:04.267] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109631252802 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 10:44:05.294] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110652930807 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 10:44:05.565] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576245 ntp_fraction=2426656522 arrival_ns=6921281863100
[2026-11-02 10:44:05.565] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:44:05.566] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:05.580] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576245 ntp_fraction=2491081031 arrival_ns=6921297285700
[2026-11-02 10:44:05.581] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:44:05.581] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:05.625] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576245 ntp_fraction=2680059592 arrival_ns=6921341685500
[2026-11-02 10:44:05.625] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:44:05.625] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:05.685] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576245 ntp_fraction=2942052597 arrival_ns=6921401862200
[2026-11-02 10:44:05.686] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:44:05.686] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:06.333] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4565 source_master_ns=111697828766 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 10:44:07.355] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112719506770 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 10:44:08.377] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113741184775 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 10:44:09.399] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114762862779 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 10:44:10.443] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115807760738 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 10:44:10.568] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3675620796 ntp_seconds=4002576250 ntp_fraction=2439541424 arrival_ns=6926285030600
[2026-11-02 10:44:10.569] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3675620796
[2026-11-02 10:44:10.569] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:10.583] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3171761075 ntp_seconds=4002576250 ntp_fraction=2503965933 arrival_ns=6926300124600
[2026-11-02 10:44:10.583] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3171761075
[2026-11-02 10:44:10.584] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:10.627] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2315012325 ntp_seconds=4002576250 ntp_fraction=2692944494 arrival_ns=6926344302000
[2026-11-02 10:44:10.628] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2315012325
[2026-11-02 10:44:10.628] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:10.687] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2181379251 ntp_seconds=4002576250 ntp_fraction=2950642532 arrival_ns=6926404343800
[2026-11-02 10:44:10.688] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2181379251
[2026-11-02 10:44:10.688] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:44:11.465] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116829438743 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 10:44:12.487] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117851116748 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 10:44:13.531] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4875 source_master_ns=118896014707 raw_phase_ns=6803 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 10:44:14.553] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119917692711 raw_phase_ns=6802 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 10:44:14.751] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6930467950000
[2026-11-02 10:44:14.751] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:44:14.751] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6930468263700
[2026-11-02 10:44:14.751] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:44:14.751] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.1 phase=purging old=1 next=2 transition=1
[2026-11-02 10:44:14.751] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r377.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=3171761075
[2026-11-02 10:44:14.752] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r377.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=2181379251
[2026-11-02 10:44:14.752] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-02 10:44:14.755] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.0 phase=purging old=1 next=2 transition=1
[2026-11-02 10:44:14.755] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3608
[2026-11-02 10:44:14.756] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:44:14.762] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-02 10:44:14.762] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3608
[2026-11-02 10:44:14.763] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:44:14.765] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6930482082500
[2026-11-02 10:44:14.765] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:44:14.766] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r377.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2315012325 audio_raw_generation=0 audio_ssrc=3675620796
[2026-11-02 10:44:14.766] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r377.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2315012325 audio_raw_generation=0 audio_ssrc=3675620796
[2026-11-02 10:44:14.766] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.2 phase=purging old=1 next=2 transition=1
[2026-11-02 10:44:14.767] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6930483889600
[2026-11-02 10:44:14.767] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:44:14.769] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-02 10:44:14.769] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-02 10:44:14.769] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5170
[2026-11-02 10:44:14.769] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3608
[2026-11-02 10:44:14.770] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-02 10:44:54.622] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r377.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=3053212208 raw_generation=1 raw_sequence=5172 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-02 10:44:54.622] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576294 ntp_fraction=2667174690 arrival_ns=6970338654300
[2026-11-02 10:44:54.623] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:44:54.623] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:44:54.623] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576294 ntp_fraction=2667174690 arrival_ns=6970338640900
[2026-11-02 10:44:54.623] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:44:54.624] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:44:55.158] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-02 10:44:55.666] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=6971382674700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3543 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.666] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6971382946000 group=composition-r377.source.2 release_identity=15757 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=160480990400 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.666] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6971383183700 group=composition-r377.source.2 release_identity=15757 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=160480990400 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.666] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=6971383213600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=27 release_count=3571 release_committed_count=3543 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.666] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6971383481400 group=composition-r377.source.2 release_identity=15757 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=160480990400 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.667] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=6971383692100 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:44:55.667] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6971383806800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:44:55.667] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6971383684000 group=composition-r377.source.2 release_identity=15757 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=160480990400 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.667] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=160482455252 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:44:55.667] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=160528895161 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:44:55.673] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6971389755400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:44:55.673] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6971390119200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:44:55.673] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6971390419300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:44:55.673] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=6971390534500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.674] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6971390716600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.674] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:44:55.674] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=6971391165100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3448 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.674] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=6971391217700 input_count=3448 accepted_count=3448 would_block_count=0 resolved_count=3448 output_attempt_count=0 published_count=3448 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.674] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6971391271800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.674] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6971391360700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.675] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=1493530319 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6971391592300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.675] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=497843 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6971391856600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.675] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=497843 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6971391993100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:44:55.675] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=6971392009000 input_count=3439 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3439 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:44:55.675] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=497843 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6971392184400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:44:55.675] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=497843 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6971392289200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:44:55.675] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r377.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=497843 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6971392375400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:44:55.681] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r377.source.2.input.video generation=2 steady_ns=6971398036800 source_sequence=4 presentation_ns=1094000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:44:55.713] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r377.source.2.input.video generation=2 steady_ns=6971429689700 source_sequence=4 presentation_ns=1094000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:44:56.210] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=161573793121 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:44:57.234] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=162595471125 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:44:58.253] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=163617149130 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:44:59.299] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=164662047089 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:44:59.625] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576299 ntp_fraction=2684354560 arrival_ns=6975342201800
[2026-11-02 10:44:59.626] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:44:59.626] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:44:59.627] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576299 ntp_fraction=2688649527 arrival_ns=6975343612100
[2026-11-02 10:44:59.627] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:44:59.627] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:00.320] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=165683725093 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:45:01.341] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=166705403098 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:45:02.364] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=167727081102 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:45:03.408] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=168771979062 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:45:04.430] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=169793657066 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:45:04.654] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576304 ntp_fraction=2808908611 arrival_ns=6980371003500
[2026-11-02 10:45:04.654] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:04.654] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:04.658] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576304 ntp_fraction=2826088480 arrival_ns=6980374734400
[2026-11-02 10:45:04.658] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:04.658] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:05.451] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=170815335071 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:45:06.497] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=171860233030 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:45:07.518] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=172881911034 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:45:08.539] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=173903589039 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:45:09.563] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=174925267043 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:45:09.669] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576309 ntp_fraction=2869038153 arrival_ns=6985385374800
[2026-11-02 10:45:09.669] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:09.669] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:09.670] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576309 ntp_fraction=2869038153 arrival_ns=6985385365500
[2026-11-02 10:45:09.670] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:09.670] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:10.615] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=175970165003 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:45:11.628] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=176991843007 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:45:12.651] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=178013521012 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:45:13.695] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=179058418971 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:45:14.677] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576314 ntp_fraction=2903397892 arrival_ns=6990393440400
[2026-11-02 10:45:14.677] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:14.677] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:14.679] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576314 ntp_fraction=2907692859 arrival_ns=6990394635700
[2026-11-02 10:45:14.679] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:14.679] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:14.725] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=180080096975 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:45:15.738] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=181101774980 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:45:16.760] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=182123452985 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:45:17.805] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=183168350944 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:45:18.826] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=184190028948 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:45:19.709] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576319 ntp_fraction=3045131812 arrival_ns=6995425775300
[2026-11-02 10:45:19.709] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:19.709] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:19.711] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576319 ntp_fraction=3049426780 arrival_ns=6995427494700
[2026-11-02 10:45:19.711] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:19.711] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:19.848] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=185211706953 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:45:20.893] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=186256604912 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:45:21.914] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=187278282917 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:45:22.936] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=188299960921 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:45:23.958] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=189321638926 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:45:24.711] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576324 ntp_fraction=3053721747 arrival_ns=7000428181200
[2026-11-02 10:45:24.712] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:24.712] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:24.726] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576324 ntp_fraction=3118146256 arrival_ns=7000442885800
[2026-11-02 10:45:24.726] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:24.726] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:25.002] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=190366536885 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:45:26.024] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=191388214889 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:45:27.047] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=192409892894 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:45:28.068] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1420 source_master_ns=193431570898 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:45:29.113] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=194476468858 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:45:29.731] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576329 ntp_fraction=3139621093 arrival_ns=7005447697300
[2026-11-02 10:45:29.731] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:29.731] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:29.732] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576329 ntp_fraction=3139621093 arrival_ns=7005448567000
[2026-11-02 10:45:29.732] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:29.732] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:30.134] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=195498146862 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:45:31.158] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=196519824867 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:45:32.200] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=197564722826 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:45:33.222] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=198586400830 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:45:34.244] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=199608078835 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:45:34.736] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576334 ntp_fraction=3156800962 arrival_ns=7010452641700
[2026-11-02 10:45:34.736] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:34.736] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:34.765] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576334 ntp_fraction=3281355014 arrival_ns=7010481052300
[2026-11-02 10:45:34.765] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:34.765] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:35.266] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1730 source_master_ns=200629756839 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 10:45:36.310] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=201674654799 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:45:37.332] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=202696332803 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:45:38.355] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=203718010808 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:45:39.399] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=204762908767 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:45:39.749] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576339 ntp_fraction=3212635537 arrival_ns=7015465629500
[2026-11-02 10:45:39.749] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:39.750] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:39.788] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576339 ntp_fraction=3384434229 arrival_ns=7015505370200
[2026-11-02 10:45:39.789] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:39.789] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:40.421] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=205784586771 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:45:41.442] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=206806264776 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:45:42.464] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2040 source_master_ns=207827942780 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:45:43.509] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=208872840740 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:45:44.531] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=209894518744 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:45:44.751] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576344 ntp_fraction=3225520439 arrival_ns=7020468282500
[2026-11-02 10:45:44.751] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:44.752] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:44.792] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576344 ntp_fraction=3397319131 arrival_ns=7020508599500
[2026-11-02 10:45:44.792] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:44.792] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:45.552] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=210916196749 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:45:46.597] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=211961094708 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:45:47.619] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=212982772712 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:45:48.640] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=214004450717 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:45:49.662] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=215026128721 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:45:49.770] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576349 ntp_fraction=3307124817 arrival_ns=7025487401100
[2026-11-02 10:45:49.771] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:49.771] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:49.799] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576349 ntp_fraction=3431678869 arrival_ns=7025516350900
[2026-11-02 10:45:49.799] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:49.800] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:50.707] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=216071026681 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:45:51.728] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2439 source_master_ns=217092704685 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:45:52.750] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=218114382690 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:45:53.795] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=219159280649 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:45:54.781] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576354 ntp_fraction=3350074490 arrival_ns=7030497724300
[2026-11-02 10:45:54.781] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:54.781] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:54.817] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=220180958653 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:45:54.826] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576354 ntp_fraction=3547642986 arrival_ns=7030543071300
[2026-11-02 10:45:54.826] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:54.827] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:55.839] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=221202636658 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:45:56.860] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=222224314663 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:45:57.911] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=223269212622 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:45:58.927] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=224290890626 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:45:59.788] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576359 ntp_fraction=3380139261 arrival_ns=7035504453900
[2026-11-02 10:45:59.788] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:45:59.788] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:59.831] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576359 ntp_fraction=3569117822 arrival_ns=7035548238300
[2026-11-02 10:45:59.831] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:45:59.832] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:45:59.948] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=225312568631 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:46:00.993] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=226357466590 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:46:02.015] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=227379144595 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:46:03.037] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=228400822599 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:46:04.058] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=229422500604 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:46:04.793] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576364 ntp_fraction=3405909065 arrival_ns=7040510024600
[2026-11-02 10:46:04.793] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:04.793] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:04.868] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576364 ntp_fraction=3728031612 arrival_ns=7040584964200
[2026-11-02 10:46:04.869] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:04.869] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:05.103] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=230467398563 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:46:06.126] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=231489076567 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:46:07.154] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=232510754572 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:46:08.192] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=233555652531 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:46:09.213] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=234577330536 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:46:09.805] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576369 ntp_fraction=3457448673 arrival_ns=7045521892300
[2026-11-02 10:46:09.805] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:09.805] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:09.895] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576369 ntp_fraction=3843995729 arrival_ns=7045612298300
[2026-11-02 10:46:09.896] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:09.896] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:10.235] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=235599008540 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:46:11.257] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=236620686545 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:46:12.301] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=237665584504 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:46:13.323] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=238687262508 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:46:14.345] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=239708940513 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:46:14.832] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576374 ntp_fraction=3573412790 arrival_ns=7050548771000
[2026-11-02 10:46:14.832] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:14.832] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:14.935] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576374 ntp_fraction=4015794421 arrival_ns=7050651913200
[2026-11-02 10:46:14.935] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:14.936] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:15.400] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3458 source_master_ns=240753838472 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:46:16.411] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=241775516477 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:46:17.433] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=242797194481 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:46:18.455] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=243818872486 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:46:19.500] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=244863770445 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:46:19.861] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576379 ntp_fraction=3697966841 arrival_ns=7055578267500
[2026-11-02 10:46:19.862] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:19.862] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:19.974] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576379 ntp_fraction=4179003179 arrival_ns=7055690544100
[2026-11-02 10:46:19.974] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:19.974] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:20.521] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=245885448449 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:46:21.543] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=246907126454 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:46:22.588] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3768 source_master_ns=247952024413 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:46:23.610] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=248973702418 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:46:24.631] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=249995380422 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:46:24.867] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576384 ntp_fraction=3719441678 arrival_ns=7060583538900
[2026-11-02 10:46:24.867] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:24.867] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:24.997] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576384 ntp_fraction=4277787426 arrival_ns=7060713495600
[2026-11-02 10:46:24.998] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:24.998] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:25.653] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=251017058427 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:46:26.697] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=252061956386 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:46:27.720] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=253083634390 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:46:28.742] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=254105312395 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:46:29.763] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4077 source_master_ns=255126990400 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:46:29.870] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576389 ntp_fraction=3732326580 arrival_ns=7065586636300
[2026-11-02 10:46:29.870] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:29.870] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:30.023] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576390 ntp_fraction=98784247 arrival_ns=7065739913900
[2026-11-02 10:46:30.023] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:30.023] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:30.807] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4122 source_master_ns=256171888359 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 10:46:31.830] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4166 source_master_ns=257193566363 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 10:46:32.851] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4210 source_master_ns=258215244368 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 10:46:33.896] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4255 source_master_ns=259260142327 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 10:46:34.878] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576394 ntp_fraction=3770981285 arrival_ns=7070595326400
[2026-11-02 10:46:34.879] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:34.879] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:34.917] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4299 source_master_ns=260281820331 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 10:46:35.067] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576395 ntp_fraction=287762808 arrival_ns=7070784279800
[2026-11-02 10:46:35.067] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:35.068] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:35.942] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4343 source_master_ns=261303498336 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 10:46:36.961] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4387 source_master_ns=262325176341 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 10:46:38.006] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4432 source_master_ns=263370074300 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 10:46:39.027] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4476 source_master_ns=264391752304 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 10:46:39.881] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576399 ntp_fraction=3783866187 arrival_ns=7075597815100
[2026-11-02 10:46:39.881] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:39.882] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:40.049] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4520 source_master_ns=265413430309 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 10:46:40.090] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576400 ntp_fraction=386547056 arrival_ns=7075807215600
[2026-11-02 10:46:40.091] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:40.091] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:41.094] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4565 source_master_ns=266458328268 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 10:46:42.115] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4609 source_master_ns=267480006273 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 10:46:43.137] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4653 source_master_ns=268501684277 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 10:46:44.159] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4697 source_master_ns=269523362282 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 10:46:44.891] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576404 ntp_fraction=3826815860 arrival_ns=7080607897300
[2026-11-02 10:46:44.891] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:44.892] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:45.094] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576405 ntp_fraction=403726925 arrival_ns=7080811389200
[2026-11-02 10:46:45.095] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:45.095] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:45.204] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4742 source_master_ns=270568260241 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 10:46:46.226] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4786 source_master_ns=271589938245 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 10:46:47.247] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4830 source_master_ns=272611616250 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 10:46:48.293] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4875 source_master_ns=273656514209 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 10:46:49.314] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4919 source_master_ns=274678192214 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 10:46:49.901] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3053212208 ntp_seconds=4002576409 ntp_fraction=3865470566 arrival_ns=7085617456400
[2026-11-02 10:46:49.902] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3053212208
[2026-11-02 10:46:49.902] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:50.134] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3557971131 ntp_seconds=4002576410 ntp_fraction=575525617 arrival_ns=7085850753600
[2026-11-02 10:46:50.134] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3557971131
[2026-11-02 10:46:50.134] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:46:50.335] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=275699870218 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-02 10:46:51.357] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=276721548223 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-02 10:46:52.402] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=277766446182 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-02 10:46:53.424] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=278788124186 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-11-02 10:46:54.105] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=7089821732200
[2026-11-02 10:46:54.105] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-02 10:46:54.105] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r377.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=3557971131 audio_raw_generation=1 audio_ssrc=3053212208
[2026-11-02 10:46:54.105] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r377.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=3557971131 audio_raw_generation=1 audio_ssrc=3053212208
[2026-11-02 10:46:54.105] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.2 phase=purging old=2 next=3 transition=2
[2026-11-02 10:46:54.106] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=2 cutoff=10363
[2026-11-02 10:46:54.106] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=2 cutoff=7203
[2026-11-02 10:46:54.106] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=2 cutoff=0
[2026-11-02 10:46:54.106] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=2 cutoff=0
[2026-11-02 10:46:54.106] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=7089822608700
[2026-11-02 10:46:54.106] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-02 10:46:54.106] [info] [graph][runtime.node] av_reacquisition group=composition-r377.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-02 10:47:20.136] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-11-02 10:47:20.136] [info] [graph][runtime.lifecycle] worker.finished node=2
[2026-11-02 10:47:20.137] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=3093750000 process_calls=26942
[2026-11-02 10:47:20.136] [info] [graph][runtime.lifecycle] worker.finished node=4
[2026-11-02 10:47:20.136] [info] [graph][runtime.lifecycle] worker.finished node=5
[2026-11-02 10:47:20.136] [info] [graph][runtime.lifecycle] worker.finished node=77
[2026-11-02 10:47:20.137] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=7
[2026-11-02 10:47:20.137] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=6468750000 process_calls=40089
[2026-11-02 10:47:20.137] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=328125000 process_calls=38677
[2026-11-02 10:47:20.136] [info] [graph][runtime.lifecycle] worker.finished node=80
[2026-11-02 10:47:20.138] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=7
[2026-11-02 10:47:20.166] [info] [graph][runtime.lifecycle] worker.finished node=74
[2026-11-02 10:47:20.166] [info] [graph][runtime.lifecycle] worker.finished node=75
[2026-11-02 10:47:20.166] [info] [graph][runtime.lifecycle] worker.finished node=78
[2026-11-02 10:47:20.166] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=343750000 process_calls=65063
[2026-11-02 10:47:20.166] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=218750000 process_calls=65553
[2026-11-02 10:47:20.166] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=593750000 process_calls=65588
[2026-11-02 10:47:20.193] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=175695 produced_ts_payload_bytes=203973796 pending_bytes=0 peak_pending_bytes=110920 access_units=21869 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=44355863 maximum_mux_input_after_emit_ns=59037307 maximum_mux_input_after_dispatch_ns=-40962693 worst_mux_input_ready_ns=111707966000 worst_mux_input_emit_ns=111648928693 worst_mux_input_dispatch_ns=111748928693 maximum_protocol_batch_produced_after_release_ns=61556804 worst_protocol_batch_produced_ns=250421510800 worst_protocol_batch_release_ns=250359953996 worst_protocol_batch_deadline_ns=250426620664 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-11-02 10:47:20.194] [info] [graph][runtime.lifecycle] worker.finished node=76
[2026-11-02 10:47:20.194] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=2875000000 process_calls=132451
[2026-11-02 10:47:20.194] [info] [graph][runtime.lifecycle] worker.finished node=79
[2026-11-02 10:47:20.194] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=1703125000 process_calls=105084
[2026-11-02 10:47:20.194] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=175695 last_sequence=175695 maximum_submit_interval_ns=87500 maximum_submit_interval_sequence=175695 maximum_start_lateness_ns=272968 maximum_start_lateness_sequence=175695 first_release_master_ns=304908611232 first_deadline_master_ns=305008611232 last_release_master_ns=304908611232 last_deadline_master_ns=305008611232 first_submitted_master_ns=304908971700 last_submitted_master_ns=304908971700 worst_wait_started_master_ns=304893798200 worst_wait_completed_master_ns=304908881000 worst_submit_observed_master_ns=304908884200 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=15079200 wait_call_max_ns=15079200 wait_before_call_count=1 wait_before_call_total_ns=200 wait_before_call_max_ns=200 wait_arm_count=1 wait_arm_total_ns=2200 wait_arm_max_ns=2200 wait_api_wall_count=1 wait_api_wall_total_ns=15073900 wait_api_wall_max_ns=15073900 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=3500 wait_return_to_next_clock_max_ns=3500 wait_target_lateness_count=1 wait_target_lateness_total_ns=269768 wait_target_lateness_max_ns=269768 worst_wait_metric=call_wall worst_wait_sequence=175695 worst_wait_target_master_ns=304908611232 worst_wait_sampled_master_ns=304893798200 worst_wait_sampled_steady_ns=7115895257600 worst_wait_timeout_ns=14813032 worst_wait_entered_steady_ns=7115895257800 worst_wait_returned_steady_ns=7115910337000 worst_wait_arm_before_steady_ns=7115895258200 worst_wait_arm_after_steady_ns=7115895260400 worst_wait_api_before_steady_ns=7115895260500 worst_wait_api_returned_steady_ns=7115910334400 worst_wait_windows_due_100ns=-148131 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=25152 worst_wait_windows_tid=30940 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=304908881000 worst_wait_next_steady_ns=7115910340500 worst_wait_outcome=deadline
[2026-11-02 10:47:20.195] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=165387 pacing_lateness_ns=34895260 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=268626615768 deadline_master_ns=268693253136 original_protocol_deadline_master_ns=268726615768 prefix_completion_limit_master_ns=268693253136 reserved_at_master_ns=268632200400 not_before_master_ns=268632412840 not_after_master_ns=268693038096 service_duration_ns=215040 wire_bytes_per_second=6250000 submit_started_master_ns=268667308100 submit_completed_master_ns=268667400200 activation_started_master_ns=268619688600 activation_completed_master_ns=268619689200 permit_started_master_ns=268632200100 permit_completed_master_ns=268632200200 snapshot_started_master_ns=268632200300 snapshot_sampled_master_ns=268632200400 snapshot_completed_master_ns=268632200500 reservation_completed_master_ns=268632201200 wait_started_master_ns=268632201400 wait_completed_master_ns=268667305600 submit_observed_master_ns=268667308100 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=35101200 wait_call_max_ns=35101200 wait_before_call_count=1 wait_before_call_total_ns=100 wait_before_call_max_ns=100 wait_arm_count=1 wait_arm_total_ns=3000 wait_arm_max_ns=3000 wait_api_wall_count=1 wait_api_wall_total_ns=35096500 wait_api_wall_max_ns=35096500 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=2900 wait_return_to_next_clock_max_ns=2900 wait_target_lateness_count=1 wait_target_lateness_total_ns=34892760 wait_target_lateness_max_ns=34892760 worst_wait_metric=call_wall worst_wait_sequence=165387 worst_wait_target_master_ns=268632412840 worst_wait_sampled_master_ns=268632201400 worst_wait_sampled_steady_ns=7079633660900 worst_wait_timeout_ns=211440 worst_wait_entered_steady_ns=7079633661000 worst_wait_returned_steady_ns=7079668762200 worst_wait_arm_before_steady_ns=7079633661200 worst_wait_arm_after_steady_ns=7079633664200 worst_wait_api_before_steady_ns=7079633664200 worst_wait_api_returned_steady_ns=7079668760700 worst_wait_windows_due_100ns=-2115 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=25152 worst_wait_windows_tid=30940 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=268667305600 worst_wait_next_steady_ns=7079668765100 worst_wait_outcome=deadline
[2026-11-02 10:47:20.195] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=32354 committed_datagrams=175695 committed_payload_bytes=203973796 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=175695 pacing_cancelled=0 pacing_submitted=175695 pacing_maximum_submit_lateness_ns=34895260 pacing_worst_late_global_sequence=165387 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=73088800 last_materialized_sequence=175695 last_scheduled_sequence=175695 last_submitted_sequence=175695 last_committed_sequence=175695 submitted_service_generation=1 submitted_service_sequence=175695 submitted_service_completed_ns=304908971700 submitted_service_duration_ns=34560 submitted_service_available_ns=304909006260 submitted_scope_available_ns=304909006260 shared_scope_charged_datagrams=175695 shared_scope_charged_wire_bytes=208893256 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=175695 endpoint_1_committed_payload_bytes=203973796
[2026-11-02 10:47:20.195] [info] [graph][runtime.lifecycle] worker.finished node=81
[2026-11-02 10:47:20.195] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=10640625000 process_calls=63490
[2026-11-02 10:47:20.394] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-11-02 10:47:20.394] [info] [graph][runtime.lifecycle] stop.begin
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=406250000 process_calls=60102
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=296875000 process_calls=83301
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=500000000 process_calls=185463
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=687500000 process_calls=187198
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=80
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=78
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=62500000 process_calls=310
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=15625000 process_calls=102
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=105
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=458
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=78125000 process_calls=10608
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=81
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=46875000 process_calls=10596
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=81
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=187500000 process_calls=21390
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=359375000 process_calls=30752
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=313
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=105
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=105
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=449
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=140625000 process_calls=10754
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=79
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=46875000 process_calls=10756
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=140625000 process_calls=21640
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=328125000 process_calls=31071
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=298
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=125000000 process_calls=10730
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=78125000 process_calls=10699
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=125000000 process_calls=21568
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=250000000 process_calls=30885
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=62500000 process_calls=10846
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=140625000 process_calls=10824
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=343750000 process_calls=52575
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=31250000 process_calls=10678
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=78125000 process_calls=10679
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=421875000 process_calls=51806
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=93750000 process_calls=10700
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=156250000 process_calls=10671
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=640625000 process_calls=51509
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=46875000 process_calls=10584
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=125000000 process_calls=10588
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=578125000 process_calls=51823
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1125000000 process_calls=14194
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=984375000 process_calls=14403
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2515625000 process_calls=34929
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=984375000 process_calls=42480
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=15625000 process_calls=10474
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=62500000 process_calls=10467
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=187500000 process_calls=21321
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=281250000 process_calls=31050
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=125000000 process_calls=11048
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=203125000 process_calls=10917
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=343750000 process_calls=21886
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=578125000 process_calls=30997
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=671875000 process_calls=13736
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=906250000 process_calls=13713
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1421875000 process_calls=25202
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=671875000 process_calls=50330
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=171875000 process_calls=10562
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=78125000 process_calls=10545
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=343750000 process_calls=21214
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=2718750000 process_calls=48943
[2026-11-02 10:47:20.395] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1578125000 process_calls=50308
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=61573704 worst_materialized_at_ns=250421527700 worst_release_ns=250359953996 worst_deadline_ns=250426620664 worst_global_sequence=152358
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115859498400 maximum_inter_event_gap_ns=49676000 gap_begin_generation=1 gap_begin_begin=11598848 gap_begin_end=11599872 gap_begin_rate=44100 gap_begin_steady_ns=7079621006700 gap_end_generation=1 gap_end_begin=11599872 gap_end_end=11600896 gap_end_rate=44100 gap_end_steady_ns=7079670682700
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=7892 missing=audio begin_steady_ns=7079621024000 end_steady_ns=7079670684300 observed_wait_ns=49660300 held_dispatch_master_ns=268759949101 begin_master_ns=268619564400 end_master_ns=268669224600 dispatch_remaining_at_end_ns=90724501 resumed_dispatch_master_ns=268762262026
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115853372600 maximum_inter_event_gap_ns=49693700 gap_begin_generation=1 gap_begin_begin=11598848 gap_begin_end=11599872 gap_begin_rate=44100 gap_begin_steady_ns=7079620949500 gap_end_generation=1 gap_end_begin=11599872 gap_end_end=11600896 gap_end_rate=44100 gap_end_steady_ns=7079670643200
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115853376800 maximum_inter_event_gap_ns=49691800 gap_begin_generation=1 gap_begin_begin=11598848 gap_begin_end=11599872 gap_begin_rate=44100 gap_begin_steady_ns=7079620960700 gap_end_generation=1 gap_end_begin=11599872 gap_end_end=11600896 gap_end_rate=44100 gap_end_steady_ns=7079670652500
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115843670600 maximum_inter_event_gap_ns=49556900 gap_begin_generation=1 gap_begin_begin=11600896 gap_begin_end=11601920 gap_begin_rate=44100 gap_begin_steady_ns=7079620665900 gap_end_generation=1 gap_end_begin=11601920 gap_end_end=11602944 gap_end_rate=44100 gap_end_steady_ns=7079670222800
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115843748000 maximum_inter_event_gap_ns=49723900 gap_begin_generation=1 gap_begin_begin=11600896 gap_begin_end=11601920 gap_begin_rate=44100 gap_begin_steady_ns=7079620883900 gap_end_generation=1 gap_end_begin=11601920 gap_end_end=11602944 gap_end_rate=44100 gap_end_steady_ns=7079670607800
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115853340200 maximum_inter_event_gap_ns=49728000 gap_begin_generation=1 gap_begin_begin=11598848 gap_begin_end=11599872 gap_begin_rate=44100 gap_begin_steady_ns=7079620892100 gap_end_generation=1 gap_end_begin=11599872 gap_end_end=11600896 gap_end_rate=44100 gap_end_steady_ns=7079670620100
[2026-11-02 10:47:20.407] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115853343400 maximum_inter_event_gap_ns=49726000 gap_begin_generation=1 gap_begin_begin=11598848 gap_begin_end=11599872 gap_begin_rate=44100 gap_begin_steady_ns=7079620903400 gap_end_generation=1 gap_end_begin=11599872 gap_end_end=11600896 gap_end_rate=44100 gap_end_steady_ns=7079670629400
[2026-11-02 10:47:20.407] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-02 10:47:20.408] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115843640800 maximum_inter_event_gap_ns=49558100 gap_begin_generation=1 gap_begin_begin=11600896 gap_begin_end=11601920 gap_begin_rate=44100 gap_begin_steady_ns=7079620605000 gap_end_generation=1 gap_end_begin=11601920 gap_end_end=11602944 gap_end_rate=44100 gap_end_steady_ns=7079670163100
[2026-11-02 10:47:20.408] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=12890 last_generation=1 last_begin=13198336 last_end=13199360 last_rate=44100 last_steady_ns=7115843654000 maximum_inter_event_gap_ns=49680300 gap_begin_generation=1 gap_begin_begin=11600896 gap_begin_end=11601920 gap_begin_rate=44100 gap_begin_steady_ns=7079620990400 gap_end_generation=1 gap_end_begin=11601920 gap_end_end=11602944 gap_end_rate=44100 gap_end_steady_ns=7079670670700
[2026-11-02 10:47:20.408] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r377.output audio_commits=12890 real_samples=10248192 silence_samples=2951168
[2026-11-02 10:47:20.408] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:47:20.408] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-02 10:47:20.408] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=8979 maximum_call_ns=38293500 maximum_begin_steady_ns=7079630671100 maximum_end_steady_ns=7079668964600 maximum_encoder_pts=7892 maximum_native_result=0 maximum_between_submissions_ns=50374700 before_gap_begin_steady_ns=7037462695200 before_gap_end_steady_ns=7037465953100 before_gap_encoder_pts=6627 before_gap_native_result=0 after_gap_begin_steady_ns=7037516327800 after_gap_end_steady_ns=7037520004500 after_gap_encoder_pts=6628 after_gap_native_result=0 payload_wait_maximum_ns=10003300 payload_wait_begin_steady_ns=6922700175800 payload_wait_end_steady_ns=6922710179100
[2026-11-02 10:47:20.408] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=48943 maximum_ns=18292700 begin_steady_ns=7052731868200 end_steady_ns=7052750160900 video_cursor=7085 audio_cursor=not_owned next_video_cursor=7085 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=48943 maximum_ns=2159900 begin_steady_ns=6932430543300 end_steady_ns=6932432703200 video_cursor=3476 audio_cursor=not_owned next_video_cursor=3476 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=48938 maximum_ns=494600 begin_steady_ns=6844462795800 end_steady_ns=6844463290400 video_cursor=838 audio_cursor=not_owned next_video_cursor=838 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=48938 maximum_ns=12139900 begin_steady_ns=7071678619000 end_steady_ns=7071690758900 video_cursor=7654 audio_cursor=not_owned next_video_cursor=7654 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=14 maximum_ns=19300 begin_steady_ns=6816561408100 end_steady_ns=6816561427400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8979 maximum_ns=18095900 begin_steady_ns=7032729145500 end_steady_ns=7032747241400 video_cursor=6485 audio_cursor=not_owned next_video_cursor=6485 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8979 maximum_ns=18073300 begin_steady_ns=7032729150100 end_steady_ns=7032747223400 video_cursor=6485 audio_cursor=not_owned next_video_cursor=6485 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=8980 maximum_ns=12848300 begin_steady_ns=7071665768600 end_steady_ns=7071678616900 video_cursor=7653 audio_cursor=not_owned next_video_cursor=7653 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=48942 maximum_ns=130016900 begin_steady_ns=6816424539300 end_steady_ns=6816554556200 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:47:20.409] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r377.output output_generation=1 video_commits=8979 source[0].group=composition-r377.source.0 source[0].identity=composition-r377.source.0.input.video source[0].real_frames=3418 source[0].black_frames=5561 source[0].contribution_generation=0 source[1].group=composition-r377.source.1 source[1].identity=composition-r377.source.1.input.video source[1].real_frames=3418 source[1].black_frames=5561 source[1].contribution_generation=0 source[2].group=composition-r377.source.2 source[2].identity=composition-r377.source.2.input.video source[2].real_frames=6970 source[2].black_frames=2009 source[2].contribution_generation=0  source[2].first_interval_miss_first_candidate_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_candidate_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_candidate_final_source_sequence=4  source[2].first_interval_miss_first_candidate_final_presentation_ns=1094000000  source[2].first_interval_miss_first_candidate_final_duration_ns=33333333  source[2].first_interval_miss_first_candidate_final_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_final_source_sequence=4  source[2].first_interval_miss_first_real_final_presentation_ns=1094000000  source[2].first_interval_miss_first_real_final_duration_ns=33333333  source[2].first_interval_miss_first_real_final_decoded_sequence=unavailable source[2].first_interval_miss_cursor=4645 source[2].first_interval_miss_target_ns=160559949101 source[2].first_interval_miss_steady_ns=6971394826700 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable source[2].first_interval_miss_epoch_source_start_ns=994000000 source[2].first_interval_miss_epoch_master_release_ns=160480990400 source[2].first_interval_miss_prepare_expired=2 source[2].first_interval_miss_prepare_old_generation=0 source[2].first_interval_miss_prepare_incomplete=0 source[2].first_interval_miss_first_real_cursor_at_miss=unavailable source[2].first_interval_miss_first_real_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_begin_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_end_ns_at_miss=unavailable source[2].first_interval_miss_prepare_last_begin_ns=160514323733 source[2].first_interval_miss_prepare_last_end_ns=160547657066 source[2].first_interval_miss_first_candidate_steady_ns_final=6971398036800 source[2].first_interval_miss_first_candidate_begin_ns_final=160580990400 source[2].first_interval_miss_first_candidate_end_ns_final=160614323733 source[2].first_interval_miss_first_real_cursor=4646 source[2].first_interval_miss_first_real_steady_ns=6971429689700 source[2].first_interval_miss_cursor_committed_generation=0 source[2].first_interval_miss_cursor_committed_steady_ns=6971394904500
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6974 operation=copy maximum_ns=1422900 begin_steady_ns=6921895399500 end_steady_ns=6921896822400 native_result=0 thread_cpu_ns=0
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=20782 operation=copy maximum_ns=447200 begin_steady_ns=6920198523900 end_steady_ns=6920198971100 native_result=0 thread_cpu_ns=0
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41566 operation=copy maximum_ns=3914900 begin_steady_ns=7068203059700 end_steady_ns=7068206974600 native_result=0 thread_cpu_ns=0
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=79200 begin_steady_ns=6816304017700 end_steady_ns=6816304096900 native_result=0 thread_cpu_ns=0
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6974 operation=copy maximum_ns=985600 begin_steady_ns=6830529653500 end_steady_ns=6830530639100 native_result=0 thread_cpu_ns=0
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6972 operation=copy maximum_ns=17862500 begin_steady_ns=7032729347600 end_steady_ns=7032747210100 native_result=0 thread_cpu_ns=0
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=7032729150700 end_steady_ns=7032747222400 wall_ns=18071700 thread_cpu_ns=0 surface_count=1 surface_total_ns=600 surface_maximum_ns=600 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=800 validate_maximum_ns=800 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=140400 black_maximum_ns=140400 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=48800 tile_maximum_ns=48800 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=5800 publish_maximum_ns=5800 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=1800 context_push_maximum_ns=1800 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=2 source_wait_total_ns=3100 source_wait_maximum_ns=3100 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=167900 plane_copy_maximum_ns=68400 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=2600 context_pop_maximum_ns=2600 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=17859500 batch_completion_maximum_ns=17859500 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=1 source_query_ready_maximum_ns=7500 source_query_ready_begin_steady_ns=7032729298500 source_query_ready_end_steady_ns=7032729306000 source_query_ready_tile=2 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-02 10:47:20.410] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=7116127560700 input_count=3569 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3569 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:47:20.411] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=7116127693600 input_count=3432 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3432 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:47:20.411] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=7116127824300 input_count=3441 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3441 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=10 incomplete=0
[2026-11-02 10:47:20.411] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-02 10:47:20.411] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:47:20.411] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=7116128384800 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.414] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:47:20.415] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=7116131698400 input_count=3439 accepted_count=3439 would_block_count=0 resolved_count=3439 output_attempt_count=0 published_count=3438 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:47:20.420] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:47:20.420] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=7116137582800 input_count=3451 accepted_count=3451 would_block_count=0 resolved_count=3451 output_attempt_count=0 published_count=3451 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=7116143812500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=7116143961900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=7116144038100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3457 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3609 download=0 upload=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=7116144169600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] hardware_transfer.summary forwarded=41 download=0 upload=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=7116144291700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] hardware_transfer.summary forwarded=41 download=0 upload=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=7116144413000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.427] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-02 10:47:20.428] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:47:20.428] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=7116144661000 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:47:20.428] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-02 10:47:20.439] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:47:20.439] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=7116155811000 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:47:20.439] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-02 10:47:20.454] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:47:20.455] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=7116171691800 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:47:20.455] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=7116192588600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=7116192786200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=7116192864300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=7116192933700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=7116193001000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=7116193076000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:47:20.476] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10340 completion_waits=10340 completed_datagrams=10462 drained_datagrams=150 would_block=32 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:47:20.477] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=148005 completion_waits=148005 completed_datagrams=212075 drained_datagrams=64101 would_block=32 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:47:20.477] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=76472 completion_waits=76472 completed_datagrams=105380 drained_datagrams=29001 would_block=92 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:47:20.478] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=67623 completion_waits=67623 completed_datagrams=105379 drained_datagrams=37846 would_block=92 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:47:20.478] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-02 10:47:20.479] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-02 10:47:20.528] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=916, threads=81, processThreads=130, workers=0, workerProgress=575345, workerProcessCalls=2135197, workerWaits=1559835, workerWakeups=1537063, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=955, logicalProcessors=22, averageSystemMachineCpuPercent=7.053506, averageProcessMachineCpuPercent=1.554059, peakProcessMachineCpuPercent=4.690832, averageProcessSingleCoreCpuPercent=34.189296, peakProcessSingleCoreCpuPercent=103.198294, initialWorkingSetBytes=208101376, workingSetBytes=262455296, peakWorkingSetBytes=271941632, totalPushed=494160, totalPopped=494124, droppedBuffers=0, encodedPacketsPushed=100372, encodedPacketsPopped=100372, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15522906, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=748, graphPayloadReservations=226085, graphPayloadReleases=226085, graphPayloadPressureFailures=1, lastInputReceivedAtNs=7089820906700
[CLI] realtime video validation stopped successfully
~~~

## 首次背压变化

~~~text
[CLI] runtime report: queued=630, peakQueued=907, threads=81, processThreads=129, workers=75, workerProgress=289850, workerProcessCalls=1119371, workerWaits=829514, workerWakeups=821382, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=337, logicalProcessors=22, averageSystemMachineCpuPercent=7.649239, averageProcessMachineCpuPercent=1.983848, peakProcessMachineCpuPercent=4.690832, averageProcessSingleCoreCpuPercent=43.644649, peakProcessSingleCoreCpuPercent=103.198294, initialWorkingSetBytes=208101376, workingSetBytes=270520320, peakWorkingSetBytes=271941632, totalPushed=251507, totalPopped=250877, droppedBuffers=0, encodedPacketsPushed=35578, encodedPacketsPopped=35573, backpressureItems=123, graphPayloadCurrentBytes=10536831, graphPayloadHighWaterBytes=15522906, graphPayloadCurrentObjects=744, graphPayloadHighWaterObjects=748, graphPayloadReservations=99832, graphPayloadReleases=99088, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6922677929200
[CLI] runtime report: queued=624, peakQueued=908, threads=81, processThreads=129, workers=75, workerProgress=290700, workerProcessCalls=1122814, workerWaits=832105, workerWakeups=823951, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=338, logicalProcessors=22, averageSystemMachineCpuPercent=7.651207, averageProcessMachineCpuPercent=1.982632, peakProcessMachineCpuPercent=4.690832, averageProcessSingleCoreCpuPercent=43.617910, peakProcessSingleCoreCpuPercent=103.198294, initialWorkingSetBytes=208101376, workingSetBytes=270520320, peakWorkingSetBytes=271941632, totalPushed=252234, totalPopped=251610, droppedBuffers=0, encodedPacketsPushed=35680, encodedPacketsPopped=35678, backpressureItems=123, graphPayloadCurrentBytes=10793696, graphPayloadHighWaterBytes=15522906, graphPayloadCurrentObjects=738, graphPayloadHighWaterObjects=748, graphPayloadReservations=100124, graphPayloadReleases=99386, graphPayloadPressureFailures=1, lastInputReceivedAtNs=6922988036900
~~~

## 完整stderr

~~~text
[in @ 00000294feec3f40] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000294feec3f40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27748.176856
[in @ 00000294feec3900] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000294feec3900] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5032.629256
[in @ 0000029489d92300] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000029489d92300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 24078.331078
[in @ 00000294f0224640] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000294f0224640] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 24078.331078
[in @ 0000029484a15c00] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000029484a15c00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27748.176856
[in @ 0000029489919580] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000029489919580] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5032.629256
[aac @ 00000294843827c0] Qavg: 15142.451

~~~

## source0 完成日志

~~~text
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.66    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.67 bitrate=8114.6kbits/s speed=   1x elapsed=0:01:59.17    
[out#0/rtp @ 0000018161407b00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 00000181631a9a80] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## source1 完成日志

~~~text
frame= 3558 fps= 30 q=-1.0 size=  117493KiB time=00:01:58.60 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.09    
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.60    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.13    
[out#0/rtp @ 000001be9fd8ac80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

## source2 完成日志

~~~text
frame= 3558 fps= 30 q=-1.0 size=  117493KiB time=00:01:58.60 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.09    
frame= 3573 fps= 30 q=-1.0 size=  118036KiB time=00:01:59.10 bitrate=8118.8kbits/s speed=   1x elapsed=0:01:58.60    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.13    
[out#0/rtp @ 0000022a40548a80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

## reentry 完成日志

~~~text
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.16 bitrate=8115.1kbits/s speed=   1x elapsed=0:01:58.63    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 000001cc98937c00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001cc9a5ee680] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## CPU与内存逐秒采样

列：elapsed_seconds,cpu_machine_percent,working_set_bytes,private_bytes。

~~~csv
1.0128364,0.560980843680004,142270464,567267328
2.0280306,0.0699597449165167,144994304,569995264
3.0607427,0.7565031919351,149000192,573759488
4.0902514,0.620883092541662,151994368,576696320
5.1235069,0.618631640919933,196620288,680816640
6.1442681,3.20059721563227,254808064,1256288256
7.1820609,2.18996245948832,255979520,1257054208
8.2180108,2.26241635816558,257290240,1258110976
9.2407263,1.6666858520727,259366912,1259679744
10.2764233,1.98866955384547,259436544,1259683840
11.3058574,2.89766440168879,259678208,1259687936
12.331009,1.80128569188295,261890048,1261711360
13.3626287,1.65230021736252,262057984,1261719552
14.3837906,1.9474251474094,262119424,1261719552
15.4151258,1.51502634642937,262119424,1261719552
16.4489168,1.99233606300412,262119424,1261719552
17.4807672,2.68438754652454,264601600,1263661056
18.5100139,1.58710513939246,264658944,1263661056
19.5404822,1.58522365731457,264720384,1263665152
20.5767352,1.91906451767701,264777728,1263665152
21.616559,2.66380358252655,264949760,1263665152
22.6398164,1.94343707031717,265007104,1263665152
23.6745983,1.85315730432049,265072640,1263673344
24.7195881,2.17487986268122,265072640,1263673344
25.7421672,1.94472619637577,265072640,1263673344
26.7755094,1.58081488133624,265084928,1263673344
27.8052154,2.89689925615131,265084928,1263673344
28.8336605,1.65740053070937,265863168,1264451584
29.8649013,2.61710323754029,265093120,1263677440
30.896024,1.65309662423827,265068544,1263640576
31.9266267,1.17153425237132,265076736,1263648768
32.9536946,2.62773633209999,265076736,1263648768
33.9830672,2.06988394501837,265568256,1263992832
35.0040249,3.06085159061929,265568256,1263992832
36.0226128,2.02207300018888,265572352,1263996928
37.0526747,1.7926989718685,265572352,1263996928
38.083269,1.92960155478868,264114176,1262481408
39.1077481,2.42639938144707,264114176,1262481408
40.1385913,1.72244254200657,264241152,1262481408
41.16383,1.93968132849099,265355264,1263661056
42.1923798,2.00248844626589,266768384,1264812032
43.2255638,2.47469780970105,267579392,1265614848
44.2571299,2.89167562355388,266821632,1264852992
45.2844357,2.28145309799672,266821632,1264852992
46.3136618,2.13918452461957,266956800,1264852992
47.3341332,2.08793878807561,266956800,1264852992
48.3732675,2.25548324215647,266960896,1264852992
49.4003609,2.07447717820191,266960896,1264852992
50.4252975,1.52448453884855,267132928,1264852992
51.452749,2.07375415596923,267132928,1264852992
52.4823587,1.79348631728208,267132928,1264852992
53.5067921,2.07986367701582,267132928,1264852992
54.5390497,2.20170553622203,267190272,1264852992
55.5652431,2.0762965520747,267194368,1264852992
56.6005644,1.92079151045802,267194368,1264852992
57.633241,1.7193845409281,267194368,1264852992
58.6646379,1.51493571485429,267194368,1264852992
59.701271,1.84984797066932,267206656,1264852992
60.7337955,1.44449577005415,267173888,1264779264
61.7604024,2.14464226322125,267173888,1264779264
62.7880917,2.4879291647954,268001280,1265549312
63.8142042,1.799598883252,267296768,1264779264
64.8456059,1.65264945224102,267350016,1264779264
65.8688501,1.24936851917566,267546624,1264979968
66.8992868,2.48129572812982,269885440,1267322880
67.9254646,1.93790624162437,269889536,1267322880
68.9495334,1.52577639314857,269885440,1267322880
69.9771042,2.211747621407,268324864,1265762304
71.0061024,1.86357336326111,267259904,1264521216
72.0359707,1.79303597274615,267268096,1264521216
73.0715356,1.92033967512451,267259904,1264521216
74.1037997,1.51366302480151,267812864,1265078272
75.1337806,2.34448301640616,269271040,1266561024
76.1642247,1.65418527268528,270036992,1267331072
77.1947282,2.68789612421149,269266944,1266561024
78.2204932,1.17705942748715,269332480,1266561024
79.2362835,1.67804856430059,269447168,1266561024
80.2563729,1.11398435898229,269447168,1266561024
81.2832019,2.21334542823322,269447168,1266561024
82.3140704,2.06688032293333,269467648,1266561024
83.3484576,1.92252607499046,270057472,1267752960
84.3896955,2.25092651736938,270901248,1268539392
85.4198561,1.93041392151511,270127104,1267765248
86.4449768,1.66277537322722,268939264,1266577408
87.4681296,2.01305131639097,269045760,1266577408
88.4961928,2.14160427632711,269828096,1267355648
89.5238441,3.17913815176943,269295616,1266585600
90.5562394,2.4765883589534,269406208,1266585600
91.5778274,1.80756910720456,269635584,1266585600
92.5981618,1.32253878550191,269680640,1266585600
93.6344336,1.71341937686444,269680640,1266585600
94.6585779,1.04022539508437,269705216,1266585600
95.6840186,2.00855992053864,269705216,1266585600
96.7040393,1.94960392826965,267993088,1264873472
97.7258336,1.87671201176562,268193792,1265074176
98.7501497,1.59474475435145,269803520,1266659328
99.7697922,1.95032706427631,271761408,1268600832
100.7914094,1.94655724633096,271773696,1268600832
101.8272214,2.53698635378903,271822848,1268600832
102.8619114,1.37283103678836,270413824,1267130368
103.8909043,1.86358296190734,269369344,1266028544
104.9226506,2.34047141944947,269369344,1266028544
105.9503806,2.55693704483755,269373440,1266028544
106.9856344,1.64650007036484,270262272,1266798592
108.0174556,1.72080994441495,271941632,1268568064
109.0519691,1.6476783092202,271941632,1268568064
110.0835715,1.92771591422853,271949824,1268568064
111.1149169,1.85933212710664,270458880,1266995200
112.1460467,1.51532813812577,270520320,1266995200
113.1699451,2.01158541795658,270577664,1266995200
114.1996397,1.37949110877589,269467648,1265885184
115.2227628,1.80485701973782,269467648,1265885184
116.2480403,1.80106450116276,269586432,1265885184
117.2831375,2.47012375438576,269594624,1265885184
118.3058788,2.08330475965115,269647872,1265885184
119.3338579,1.17452423267785,269647872,1265885184
120.3708783,1.36974600061343,246239232,1249099776
121.4059755,1.23506187719288,246239232,1249099776
122.436075,0.965264211678752,246239232,1249148928
123.4668667,1.17131944663153,246239232,1249148928
124.4963489,0.965843005171122,246239232,1249148928
125.5308477,0.892505099614866,246239232,1249243136
126.5672776,0.616736882498801,246239232,1249243136
127.5998055,1.0317792953497,246239232,1249243136
128.6204682,0.556679320388427,246239232,1249243136
129.6607186,0.95584503675095,246239232,1249243136
130.6957434,0.892051528181211,246239232,1249243136
131.7310145,1.0976483709085,246239232,1249243136
132.7718262,1.22828086089836,246239232,1249243136
133.8107971,0.683587261902394,246239232,1249243136
134.8474848,0.959129911368866,246239232,1249243136
135.8833179,1.09705283251097,246239232,1249243136
136.9109255,0.898490293907393,246239232,1249243136
137.9391364,1.03611127745378,246243328,1249243136
138.9706077,0.826268968678754,246239232,1249243136
140.0018387,1.17082046955177,246239232,1249243136
141.046068,0.952202913496296,246239232,1249243136
142.0868402,1.09184664652228,246239232,1249243136
143.1108246,1.10974701993863,246239232,1249243136
144.1344818,0.832576303153756,246239232,1249243136
145.1653564,1.30901645862825,246239232,1249243136
146.1986926,1.09970369407716,246239232,1249243136
147.2244157,0.900141036645694,246239232,1249243136
148.2491274,1.17826932554433,246239232,1249243136
149.2726963,0.693873439030106,246239232,1249243136
150.3100264,0.890069086538087,246239232,1249243136
151.3370895,0.968117910007856,246239232,1249243136
152.3697307,0.894110611261156,246239232,1249243136
153.3953793,0.900206420157396,246239232,1249243136
154.4394759,1.02034707237905,246239232,1249243136
155.4707481,1.17077369450698,246239232,1249243136
156.4884474,0.697875367239881,246239232,1249243136
157.5188388,1.03391867312841,246239232,1249243136
158.5596945,1.09175905590336,246239232,1249243136
159.5992117,1.0931648234042,248610816,1250275328
160.6345588,1.37195974707859,253714432,1254010880
161.6695834,1.78410340110847,255655936,1255997440
162.6958069,1.38415710169814,255778816,1256214528
163.726443,1.03367319375959,256499712,1256775680
164.7558282,1.58689160022188,256868352,1256906752
165.781757,1.45378243863246,256995328,1257005056
166.8095915,1.45108699185254,257269760,1257050112
167.8340633,1.3865238120313,258478080,1257668608
168.8745105,1.22871116468867,259104768,1258192896
169.9092842,1.92180798916361,259264512,1258295296
170.938547,1.38006983780483,259481600,1258442752
171.9671723,1.44997140623246,259670016,1258557440
172.9926796,1.2466113999472,259784704,1258672128
174.0202199,1.79709828324097,259870720,1258672128
175.0417727,1.32096140129206,259878912,1258680320
176.0690288,1.4519040312608,259985408,1258725376
177.1007904,1.44556385188914,259837952,1258266624
178.1254958,0.901035023866801,259899392,1258266624
179.1609951,1.92046133071878,259907584,1258266624
180.1933502,1.44473279855671,260173824,1258496000
181.2237907,1.37849254319347,260210688,1258532864
182.2564002,1.30681716387638,260214784,1258532864
183.2832173,1.86753184804154,260444160,1258881024
184.3067849,1.59591093668138,260489216,1258881024
185.3421007,1.16620104091559,260739072,1259126784
186.3643463,1.32006615453452,260759552,1259126784
187.385819,1.52965419438032,260812800,1259139072
188.422426,1.37029225680951,260820992,1259147264
189.4520476,1.44856836018912,260923392,1259167744
190.4887375,1.43869181394285,260931584,1259167744
191.5285524,1.70758101448461,260923392,1259167744
192.5526012,1.24838688440345,260923392,1259167744
193.5787613,1.31503048908433,260976640,1259167744
194.6107341,1.16997886343163,260976640,1259167744
195.6457997,1.30371622647089,260980736,1259167744
196.6801757,1.23592300179923,261009408,1259188224
197.7205342,1.36535102606895,261029888,1259282432
198.7458283,1.24687062074101,261124096,1259290624
199.7707174,1.38595926667046,261124096,1259290624
200.8067495,2.33078948738435,261287936,1259290624
201.8396985,1.71893112033428,261263360,1259257856
202.8676542,1.45091590301728,261263360,1259257856
203.9005715,1.16890903428218,261312512,1259257856
204.9361607,1.23447510934753,261312512,1259257856
205.9654868,1.79398045875925,261390336,1259335680
206.9971366,1.44572050779952,261390336,1259335680
208.0211623,1.87262256832389,261390336,1259798528
209.050208,1.03527074559555,261414912,1259798528
210.079716,1.37974114378378,261492736,1259798528
211.1107177,1.85995196357446,261492736,1259835392
212.1390492,2.00291354578668,261533696,1259835392
213.1598217,1.46112603222289,262295552,1260597248
214.1945311,1.85328715131383,261533696,1259835392
215.2280188,1.44314951472309,261681152,1259929600
216.2644741,1.30196817767425,261681152,1259929600
217.2898234,1.87020524260724,261681152,1259929600
218.3167282,1.65988653918595,261787648,1259929600
219.3373809,1.25254074271208,261787648,1259929600
220.3611068,1.59566415900267,261795840,1259929600
221.3879503,2.28248024163368,261832704,1259929600
222.4182062,1.65448744777436,261832704,1259929600
223.4454555,1.52105238718582,261885952,1259962368
224.4715946,2.00719287561414,261947392,1259962368
225.5001465,1.3810236950168,261947392,1259962368
226.5248403,1.52484576368083,261951488,1259962368
227.5453411,1.46151504509086,262713344,1260724224
228.5658344,1.67031518437745,262012928,1259962368
229.5977637,1.30767855722462,262012928,1259962368
230.6234745,1.03863672790705,262012928,1259962368
231.6511093,1.58959459846309,262012928,1259962368
232.6798825,1.51879928442928,262012928,1259962368
233.7126107,1.58175474173429,262012928,1259962368
234.740639,1.51989979264191,262090752,1259962368
235.7664913,0.969260566865383,262090752,1259962368
236.7996332,2.19982102432137,262090752,1259962368
237.8343981,1.44136824966451,262090752,1259962368
238.8555537,1.39102654429406,262090752,1259962368
239.886169,1.86064929985383,262111232,1259962368
240.9238487,0.889769217365863,262127616,1259962368
241.9602895,1.57608879086269,262119424,1259962368
242.9835749,1.24931821651034,262201344,1259962368
244.0058152,1.66746062989834,262201344,1259962368
245.0352,1.37990627552937,262205440,1259962368
246.0639495,1.44979635249132,262221824,1259982848
247.1022139,1.36810483481331,262221824,1259982848
248.128114,1.24613409327973,262221824,1259982848
249.1571891,1.44933763602605,262221824,1259982848
250.1881048,1.24007141506247,262221824,1259982848
251.2158216,1.72768235550705,262221824,1259982848
252.2415685,1.17708019749937,262221824,1259982848
253.2685482,1.65976547982931,262221824,1259982848
254.3051789,1.02769569634675,262221824,1259982848
255.3383662,1.16860356649405,262221824,1259982848
256.3695401,1.79076575647513,262221824,1259982848
257.3943125,1.52472880807482,262221824,1259982848
258.4189386,1.1783677613096,262221824,1259982848
259.450634,1.51449739913546,262217728,1259982848
260.4853494,1.44143720362838,262217728,1259982848
261.5153652,1.17220178917291,262217728,1259982848
262.5403747,1.24721682180424,262217728,1259982848
263.5649716,1.45567224801018,262217728,1259982848
264.5941668,1.03512036306709,262217728,1259982848
265.6302641,1.1653214072041,262217728,1259982848
266.6628576,1.10049466354729,262279168,1259982848
267.6885565,1.24637853361165,262283264,1259982848
268.7141399,1.1080167993784,262283264,1259982848
269.7501287,1.98810941866275,262291456,1259982848
270.7832301,1.44368914099549,262283264,1259982848
271.8130532,0.896557335473894,262283264,1259982848
272.8513766,1.36802709584951,263045120,1260744704
273.8870702,1.50865082105364,262283264,1259982848
274.9232201,1.78216579385947,262283264,1259982848
275.9662669,1.36183203424288,262287360,1259982848
277.0018213,1.57743787025838,262287360,1259982848
278.0411596,1.50336035918238,262287360,1259982848
279.0699517,1.10456100543896,263049216,1260744704
280.0961553,1.24576554877517,262287360,1259982848
281.1230793,0.82992775246539,262287360,1259982848
282.144696,0.903759163828684,262287360,1259982848
283.1818469,0.753265508423144,262287360,1259982848
284.2190475,1.02713101890888,262287360,1259982848
285.2521164,0.687492647128648,262287360,1259982848
286.2816689,1.17272928154352,262287360,1259982848
287.3069183,1.24692498323733,262287360,1259982848
288.3502412,1.157250898678,262287360,1259982848
289.3776249,1.17520490507723,262287360,1259982848
290.4158137,1.29979423605978,262287360,1259982848
291.4354802,1.25375217378342,263049216,1260744704
292.4657691,1.37869537898984,262295552,1259982848
293.4980599,1.03201627786561,262287360,1259982848
294.5260037,0.898196433059329,262287360,1259982848
295.5599808,0.618199905447167,262287360,1259982848
296.5860368,1.38438306043195,262287360,1259982848
297.6099463,0.901735411718958,262287360,1259982848
298.6379352,1.10542403362878,262287360,1259982848
299.6786387,1.02367380247198,262287360,1259982848
300.7032386,1.386350462707,262455296,1260019712
301.7343347,1.10209284698457,262455296,1260019712
302.7710726,0.822071545057519,262455296,1260019712
303.8081072,1.02729543362482,262463488,1260019712
304.8358629,0.898360821103161,262230016,1259687936

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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r377-vlc.log'
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
main debug: stream filter added to 03e4bc38
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
mmdevice debug: display name changed: VLC media player (LibVLC 3.0.23)
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
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
qt debug: IM: Setting an input
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: udp://@127.0.0.1:62024
main debug:  (path: \\@127.0.0.1:62024)
main debug: looking for access module matching "udp": 27 candidates
udp debug: opening server=:0 local=127.0.0.1:62024
main debug: net: opening 127.0.0.1 datagram port 62024
main debug: looking for meta fetcher module matching "any": 1 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
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
main debug: Buffering 13%
main debug: reusing audio output
main debug: looking for aout stream module matching "any": 2 candidates
main debug: restarting module due to input format change
main debug: removing module "avcodec"
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
main debug: using text renderer module "freetype"
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 32x32 (32x32) chroma: YUVA -> 16x16 (16x16) chroma: RGBA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: looking for video converter module matching "any": 24 candidates
yuvp debug: YUVP to YUVA converter
main debug: using video converter module "yuvp"
main debug: Deinterlacing available
main debug: deinterlace -1, mode auto, is_needed 0
main debug: looking for vout window module matching "qt,any": 3 candidates
qt debug: requesting video window...
main debug: resized to 85508864x65217224
main debug: resized to 85508864x65217224
main debug: resized to 85508608x65217224
main debug: resized to 85508608x65217224
main debug: resized to 85508352x65217224
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
main debug: Buffering 19%
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
wasapi debug: modified format
main debug: Buffering 26%
main debug: Buffering 33%
main debug: Buffering 39%
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
main debug: Buffering 46%
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0b4200e0
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
main debug: Buffering 53%
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
main debug: Buffering 59%
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 938 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (77367): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 33 ms)
main debug: picture might be displayed late (missing 0 ms)
main debug: resampling stopped (drift: -19554 us)
main debug: accepted socket 1160 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (06297508) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (060ce200) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1b30f2c8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b8550f0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06297508 removed from chain
main debug: removing module "swscale"
main debug: Filter 060ce200 removed from chain
main debug: Filter 1b30f2c8 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b8550f0 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r377-native-2026-11-02-10h42m58s961.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1b30f2c8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06297508) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1b30f2c8 removed from chain
main debug: removing module "swscale"
main debug: Filter 06297508 removed from chain
main error: read error: No error
main debug: accepted socket 3992 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (06137280) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (061377d0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (06136fd8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06137528) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06137280 removed from chain
main debug: removing module "swscale"
main debug: Filter 061377d0 removed from chain
main debug: Filter 06136fd8 removed from chain
main debug: removing module "swscale"
main debug: Filter 06137528 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r377-native-2026-11-02-10h44m39s166.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (06136fd8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (061367e0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06136fd8 removed from chain
main debug: removing module "swscale"
main debug: Filter 061367e0 removed from chain
main debug: accepted socket 4040 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (06137d20) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06135fe8) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (061377d0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06137528) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06137d20 removed from chain
main debug: removing module "swscale"
main debug: Filter 06135fe8 removed from chain
main debug: Filter 061377d0 removed from chain
main debug: removing module "swscale"
main debug: Filter 06137528 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r377-native-2026-11-02-10h45m45s947.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (06136538) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06137d20) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06136538 removed from chain
main debug: removing module "swscale"
main debug: Filter 06137d20 removed from chain
main debug: accepted socket 3908 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (06137280) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06137528) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (06136d30) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (061377d0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06137280 removed from chain
main debug: removing module "swscale"
main debug: Filter 06137528 removed from chain
main debug: Filter 06136d30 removed from chain
main debug: removing module "swscale"
main debug: Filter 061377d0 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r377-native-2026-11-02-10h47m04s908.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (06137528) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (061367e0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06137528 removed from chain
main debug: removing module "swscale"
main debug: Filter 061367e0 removed from chain
main debug: accepted socket 4068 (from socket 1176)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
ts debug: Can't read TS packet at 203973796
main debug: EOF reached
main warning: buffer too late (-253618 us): dropped
main error: read error: No error
main warning: picture is too late to be displayed (missing 602 ms)
main warning: picture is too late to be displayed (missing 572 ms)
main warning: picture is too late to be displayed (missing 539 ms)
main warning: picture is too late to be displayed (missing 506 ms)
main warning: picture is too late to be displayed (missing 472 ms)
main debug: killing decoder fourcc `hevc'
main debug: removing module "avcodec"
main debug: saving a free vout
main debug: reusing provided vout
main debug: removing module "hevc"
main debug: killing decoder fourcc `mp4a'
main debug: removing module "faad"
mmdevice debug: state changed: 0
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp11100
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

- composition-r377-cli-error.log (1966 bytes)
- composition-r377-cli.log (1248045 bytes)
- composition-r377-monitor.log (14968 bytes)
- composition-r377-native-2026-11-02-10h42m58s961.png (672906 bytes)
- composition-r377-native-2026-11-02-10h44m39s166.png (12152 bytes)
- composition-r377-native-2026-11-02-10h45m45s947.png (351036 bytes)
- composition-r377-native-2026-11-02-10h47m04s908.png (12152 bytes)
- composition-r377-reentry-stdout.log (540 bytes)
- composition-r377-reentry.log (29521 bytes)
- composition-r377-source0-stdout.log (540 bytes)
- composition-r377-source0.log (29624 bytes)
- composition-r377-source1-stdout.log (335 bytes)
- composition-r377-source1.log (29062 bytes)
- composition-r377-source2-stdout.log (335 bytes)
- composition-r377-source2.log (28926 bytes)
- composition-r377-vlc.log (40818 bytes)

上述16项已逐项清理，本轮临时产物残留0，指定120秒源保留。未运行远程本轮测试。
