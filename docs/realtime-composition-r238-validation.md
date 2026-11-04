# r238：单AU等待诊断实流复验

## 清理

VLC测试结束后按身份核对强制清理，不记为自然退出。19件清单产物已删除，6个PID与本轮前缀无残留；源124427809字节保留，无远程测试。

```powershell
$proc=Get-CimInstance Win32_Process -Filter 'ProcessId=12096';if($proc){if($proc.Name -ne 'vlc.exe' -or $proc.CommandLine -notlike '*composition-r238*'){throw 'VLC identity mismatch'};Stop-Process -Id 12096 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$names=@('composition-r238-cli-error.log','composition-r238-cli.log','composition-r238-monitor.log','composition-r238-output.sdp','composition-r238-source0-stdout.log','composition-r238-source0.log','composition-r238-source1-stdout.log','composition-r238-source1.log','composition-r238-source2-stdout.log','composition-r238-source2.log','composition-r238-reentry-stdout.log','composition-r238-reentry.log','composition-r238-vlc.log','composition-r238-native-2026-10-16-11h03m22s957.png','composition-r238-native-2026-10-16-11h05m01s804.png','composition-r238-native-2026-10-16-11h05m32s043.png','composition-r238-native-2026-10-16-11h07m22s916.png','composition-r238-native-2026-10-16-11h07m55s290.png','composition-r238-native-2026-10-16-11h08m05s201.png')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped acceptance directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force;Write-Output "REMOVED $name"}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r238-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(12096,28984,26848,24124,18136,35632)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

## 结果：完整验收未通过（画面证据不完整）

Release SHA256 31E1DD8AF301A1411FB6F11570DE98312A29C6BC5E7A56A77FDA16F6E131BD95。VLC12096，初始源28984/26848/24124，CLI18136，重入源35632。初始三源及重入源均120秒3600帧exit0；重入会话57249 exit0。CLI持续输出，最终受控caller-stop后exit0（主会话72474），不能称失源自动退出。

VLC原生快照返回0并查看：11:03:22.957三格视频右下黑；11:05:01.804全黑；11:05:32.043左下重入。11:07:22.916源结束后仍左下视频，11:07:55.290 RC报告pause state4（该图未单独查看）；RC play后11:08:05.201仍左下视频。暂停来源未证明，不能把此现象直接归因生产黑底失效，也不能判再次失源黑屏通过。未使用系统截图。

本轮未复现此前超期，不能证明已修复。缺少再次失源画面与内容级音频/线速证据，不创建成功验收提交。

## 启动与监控

```powershell
$ErrorActionPreference='Stop'
$r238Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r238-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r238Vlc.Id)"
$r238Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r238Source0.Id)"
$r238Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r238Source1.Id)"
$r238Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r238Source2.Id)"
$r238Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r238 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r238Cli.Id)"
$r238Clock=[Diagnostics.Stopwatch]::StartNew()
$r238LastTime=0.0
$r238LastCpu=0.0
$r238Cores=[Environment]::ProcessorCount
while(-not $r238Cli.WaitForExit(1000)) {
$r238Cli.Refresh()
$r238Time=$r238Clock.Elapsed.TotalSeconds
$r238Cpu=$r238Cli.TotalProcessorTime.TotalSeconds
$r238Usage=100*($r238Cpu-$r238LastCpu)/(($r238Time-$r238LastTime)*$r238Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-monitor.log' -Value "seconds=$r238Time cpu=$r238Usage ws=$($r238Cli.WorkingSet64) private=$($r238Cli.PrivateMemorySize64)"
$r238LastTime=$r238Time
$r238LastCpu=$r238Cpu
}
Write-Output "CLI_EXIT=$($r238Cli.ExitCode)"
$r238Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r238Source0.ExitCode)"
$r238Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r238Source1.ExitCode)"
$r238Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r238Source2.ExitCode)"
exit $r238Cli.ExitCode
```

## 重入

```powershell
$r238Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r238-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($r238Reentry.Id)"
$r238Reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($r238Reentry.ExitCode)"
exit $r238Reentry.ExitCode
```

## VLC快照（重复调用）

```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$stream=$client.GetStream();$stream.ReadTimeout=2000;$bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n");$stream.Write($bytes,0,$bytes.Length);$buffer=[byte[]]::new(4096);try{while(($n=$stream.Read($buffer,0,4096))-gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$n)}}catch [IO.IOException]{}}finally{$client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r238-native-*.png' | Select-Object -ExpandProperty FullName
```

## VLC恢复播放

```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$bytes=[Text.Encoding]::ASCII.GetBytes("play`r`n");$client.GetStream().Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}
```

## 受控停止（仅测试结束后）

```powershell
$r238Target=Get-CimInstance Win32_Process -Filter "ProcessId=18136"
if(-not $r238Target -or $r238Target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r238Target.CommandLine -notlike '*composition-r238*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R238Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R238Console]::FreeConsole() | Out-Null
if(-not [R238Console]::AttachConsole(18136)){throw 'AttachConsole failed'}
try {
[R238Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$r238Ids=[uint32[]]::new(16)
$r238Count=[R238Console]::GetConsoleProcessList($r238Ids,16)
if($r238Count -ne 2 -or $r238Ids[0..1] -notcontains 18136 -or $r238Ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R238Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R238Console]::FreeConsole() | Out-Null}
```



## 指标与诊断

CPU样本=319，均值=1.55442297537548，峰值=4.32246318729328，WS峰值=270704640；drift样本=230，maxAbs raw_phase_ns=13606，不是内容级AV证明。

```text
[2026-10-16 11:03:03.842] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-16 11:03:03.844] [info] [graph][runtime.node] canvas_cuda_stream stage=first_source device_stream=0000000000000000 readiness=device_stream
[2026-10-16 11:03:03.888] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=845304414 high_water=862759394
[2026-10-16 11:03:04.045] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-16 11:03:04.057] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5611594568 dispatch_master_ns=5611594568 now_ns=5457959500
[2026-10-16 11:03:04.058] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5458241600 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5611594568 present_dispatch_master_ns=5611594568
[2026-10-16 11:03:04.094] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5611594568 dispatch_master_ns=5611594568 now_ns=5494432600
[2026-10-16 11:03:38.598] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=39998306100 present_generation=1 present_canonical_target_ns=34504852607 present_canonical_dispatch_ns=34504852607 present_target_master_ns=40116447175 present_dispatch_master_ns=40116447175
[2026-10-16 11:03:38.605] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=40005368500 present_generation=1 present_canonical_target_ns=34533333333 present_canonical_dispatch_ns=34533333333 present_target_master_ns=40144927901 present_dispatch_master_ns=40144927901
[2026-10-16 11:03:56.128] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=57528441400 present_generation=1 present_canonical_target_ns=52035918367 present_canonical_dispatch_ns=52035918367 present_target_master_ns=57647512935 present_dispatch_master_ns=57647512935
[2026-10-16 11:03:56.130] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=57530161900 present_generation=1 present_canonical_target_ns=52066666666 present_canonical_dispatch_ns=52066666666 present_target_master_ns=57678261234 present_dispatch_master_ns=57678261234
[2026-10-16 11:04:47.027] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=108427663700 present_generation=1 present_canonical_target_ns=102934058956 present_canonical_dispatch_ns=102934058956 present_target_master_ns=108545653524 present_dispatch_master_ns=108545653524
[2026-10-16 11:04:47.028] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=108428730900 present_generation=1 present_canonical_target_ns=102966666666 present_canonical_dispatch_ns=102966666666 present_target_master_ns=108578261234 present_dispatch_master_ns=108578261234
[2026-10-16 11:08:26.686] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-10-16 11:08:26.725] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=23576 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=6443 maximum_mux_input_after_emit_ns=8168866 maximum_mux_input_after_dispatch_ns=-91831134 worst_mux_input_ready_ns=138686430100 worst_mux_input_emit_ns=138678261234 worst_mux_input_dispatch_ns=138778261234 maximum_protocol_batch_produced_after_release_ns=8185266 worst_protocol_batch_produced_ns=138686446500 worst_protocol_batch_release_ns=138678261234 worst_protocol_batch_deadline_ns=138778261234 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-16 11:08:26.753] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=34888 committed_datagrams=178607 committed_payload_bytes=207085520 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=178607 pacing_cancelled=0 pacing_submitted=178607 pacing_maximum_submit_lateness_ns=10740666 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=73173000 last_materialized_sequence=178607 last_scheduled_sequence=178607 last_submitted_sequence=178607 last_committed_sequence=178607 shared_scope_charged_datagrams=178607 shared_scope_charged_wire_bytes=212086516 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=178529 endpoint_1_committed_payload_bytes=207080208 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=78 endpoint_2_committed_payload_bytes=5312
[2026-10-16 11:08:26.951] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-10-16 11:08:26.962] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=3435 missing=audio begin_steady_ns=8692459563200 end_steady_ns=8692497026100 observed_wait_ns=37462900 held_dispatch_master_ns=120078261234 begin_master_ns=119943577900 end_master_ns=119981040000 dispatch_remaining_at_end_ns=97221234 resumed_dispatch_master_ns=120085970985
[2026-10-16 11:08:26.963] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=54256 maximum_ns=26810600 begin_steady_ns=8612498524500 end_steady_ns=8612525335100 video_cursor=1036 audio_cursor=not_owned next_video_cursor=1036 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=54256 maximum_ns=932300 begin_steady_ns=8652529394100 end_steady_ns=8652530326400 video_cursor=2238 audio_cursor=not_owned next_video_cursor=2238 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=54251 maximum_ns=504200 begin_steady_ns=8676296164400 end_steady_ns=8676296668600 video_cursor=2950 audio_cursor=not_owned next_video_cursor=2950 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=54251 maximum_ns=12453100 begin_steady_ns=8624575014000 end_steady_ns=8624587467100 video_cursor=1399 audio_cursor=not_owned next_video_cursor=1399 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=17 maximum_ns=131500 begin_steady_ns=8577960912500 end_steady_ns=8577961044000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=9680 maximum_ns=19874900 begin_steady_ns=8612498535700 end_steady_ns=8612518410600 video_cursor=1036 audio_cursor=not_owned next_video_cursor=1036 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=9680 maximum_ns=19849100 begin_steady_ns=8612498548900 end_steady_ns=8612518398000 video_cursor=1036 audio_cursor=not_owned next_video_cursor=1036 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=9681 maximum_ns=6922900 begin_steady_ns=8612518411200 end_steady_ns=8612525334100 video_cursor=1036 audio_cursor=not_owned next_video_cursor=1036 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:08:26.964] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=54255 maximum_ns=122087000 begin_steady_ns=8577836468700 end_steady_ns=8577958555700 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 11:08:26.965] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=20799 operation=copy maximum_ns=427300 begin_steady_ns=8832161396300 end_steady_ns=8832161823600 native_result=0 thread_cpu_ns=0
[2026-10-16 11:08:26.965] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=13822 operation=copy maximum_ns=2471500 begin_steady_ns=8831695320200 end_steady_ns=8831697791700 native_result=0 thread_cpu_ns=0
[2026-10-16 11:08:26.965] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41598 operation=copy maximum_ns=2593900 begin_steady_ns=8690694878600 end_steady_ns=8690697472500 native_result=0 thread_cpu_ns=0
[2026-10-16 11:08:26.965] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=20799 operation=copy maximum_ns=10558300 begin_steady_ns=8612501725100 end_steady_ns=8612512283400 native_result=0 thread_cpu_ns=0
[2026-10-16 11:08:26.965] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=20799 operation=copy maximum_ns=376100 begin_steady_ns=8630040966200 end_steady_ns=8630041342300 native_result=0 thread_cpu_ns=0
[2026-10-16 11:08:26.965] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=8612498549700 end_steady_ns=8612518397700 wall_ns=19848000 thread_cpu_ns=0 surface_count=1 surface_total_ns=1000 surface_maximum_ns=1000 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1700 validate_maximum_ns=1100 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=3118700 black_maximum_ns=3118700 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=16718200 tile_maximum_ns=10620500 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=5100 publish_maximum_ns=5100 publish_maximum_tile=none publish_maximum_result=0 context_push_count=4 context_push_total_ns=7400 context_push_maximum_ns=4300 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=3 source_wait_total_ns=14200 source_wait_maximum_ns=5400 source_wait_maximum_tile=1 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=281800 plane_copy_maximum_ns=79500 plane_copy_maximum_tile=2 plane_copy_maximum_result=0 destination_wait_count=4 destination_wait_total_ns=19383700 destination_wait_maximum_ns=10556800 destination_wait_maximum_tile=0 destination_wait_maximum_result=0 context_pop_count=4 context_pop_total_ns=111200 context_pop_maximum_ns=94100 context_pop_maximum_tile=1 context_pop_maximum_result=0
[2026-10-16 11:08:27.023] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10340 completion_waits=10340 completed_datagrams=10462 drained_datagrams=163 would_block=44 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 11:08:27.024] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=121277 completion_waits=121277 completed_datagrams=212075 drained_datagrams=90840 would_block=44 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 11:08:27.024] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=73485 completion_waits=73485 completed_datagrams=105441 drained_datagrams=32060 would_block=104 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 11:08:27.025] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=64417 completion_waits=64417 completed_datagrams=105379 drained_datagrams=41066 would_block=104 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=916, threads=82, processThreads=131, workers=0, workerProgress=570830, workerProcessCalls=2186323, workerWaits=1615475, workerWakeups=1590805, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1043, logicalProcessors=22, averageSystemMachineCpuPercent=13.136528, averageProcessMachineCpuPercent=1.569536, peakProcessMachineCpuPercent=8.247423, averageProcessSingleCoreCpuPercent=34.529782, peakProcessSingleCoreCpuPercent=181.443299, initialWorkingSetBytes=207572992, workingSetBytes=262152192, peakWorkingSetBytes=270856192, totalPushed=508950, totalPopped=508914, droppedBuffers=0, encodedPacketsPushed=108206, encodedPacketsPopped=108206, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15571876, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=672, graphPayloadReservations=230047, graphPayloadReleases=230047, graphPayloadPressureFailures=0, lastInputReceivedAtNs=8832190419200
[in @ 000001e1c9c28640] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e1c9c28640] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38418.998211
[in @ 000001e1cf87a6c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e1cf87a6c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8229.529267
[in @ 000001e1ca018ac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e1ca018ac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 96.395956
[in @ 000001e1b5c45540] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e1b5c45540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8229.529267
[in @ 000001e1c9d12dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e1c9d12dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 96.395956
[in @ 000001e1c9d12540] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e1c9d12540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38418.998211
[aac @ 000001e1c9d6fe40] Qavg: 18758.582

```

composition-r238-source0.log
```text
[out#0/rtp @ 0000020b14f2c2c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000020b14fbb140] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
```

composition-r238-source1.log
```text
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.21    
[out#0/rtp @ 000001c21c8f9580] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

composition-r238-source2.log
```text
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.17    
[out#0/rtp @ 000002187d558080] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
```

composition-r238-reentry.log
```text
[out#0/rtp @ 000001c29f6666c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001c29f6fa040] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
```

VLC错误/暂停/迟到诊断
```text
faad warning: decoded zero sample
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main warning: playback too late (81514): up-sampling
main warning: picture is too late to be displayed (missing 30 ms)
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
main debug: toggling pause
main debug: toggling pause
main error: Failed to create video converter
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
```

后续只读核验：VLC官方3.0分支oldrc.c中snapshot触发video-snapshot callback，pause为独立Input命令；仅此不能确定本机暂停来源或证明本机二进制完全等同该分支。依据：https://raw.githubusercontent.com/videolan/vlc/3.0.x/modules/control/oldrc.c 。下轮快照前记录RC status，若暂停则原样记录，不将缓存画面当实时输出。
