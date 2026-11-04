# r233：sender reservation边界诊断与原规格复验

## 修改与静态门禁

六文件接入固定大小sender观测，复用State级master-clock采样。每包重置permit/snapshot/reservation时间，保留批级activation；仅pacer reservation失败输出generation/endpoint/global sequence及原错误。不改发送状态机、期限和准入参数；采样存在开销。两名独立审查r233_boundary_review_a/b均限定PASS，不代表超期修复。

Release全量clean-first/all-target构建会话83346，718项，configure/build退出0。ETW预检0x5及方法依据见[r232补充记录](realtime-composition-r232-validation.md)。

## 实际启动与监控命令

```powershell
$ErrorActionPreference='Stop'
$r233Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r233-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r233Vlc.Id)"
$r233Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r233Source0.Id)"
$r233Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r233Source1.Id)"
$r233Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r233Source2.Id)"
$r233Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r233 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r233Cli.Id)"
$r233Clock=[Diagnostics.Stopwatch]::StartNew()
$r233LastTime=0.0
$r233LastCpu=0.0
$r233Cores=[Environment]::ProcessorCount
while(-not $r233Cli.WaitForExit(1000)) {
$r233Cli.Refresh()
$r233Time=$r233Clock.Elapsed.TotalSeconds
$r233Cpu=$r233Cli.TotalProcessorTime.TotalSeconds
$r233Usage=100*($r233Cpu-$r233LastCpu)/(($r233Time-$r233LastTime)*$r233Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-monitor.log' -Value "seconds=$r233Time cpu=$r233Usage ws=$($r233Cli.WorkingSet64) private=$($r233Cli.PrivateMemorySize64)"
$r233LastTime=$r233Time
$r233LastCpu=$r233Cpu
}
Write-Output "CLI_EXIT=$($r233Cli.ExitCode)"
$r233Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r233Source0.ExitCode)"
$r233Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r233Source1.ExitCode)"
$r233Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r233Source2.ExitCode)"
exit $r233Cli.ExitCode
```


## 实流结果：FAIL

VLC PID32944，三源PID31000/14164/10372，CLI PID12460。CLI在10:14:04.569发生物化前超期，自行exit1，监控会话82709最终exit1；初始三源均完成120秒/3600帧并exit0。未进行失源、重入、抓包或离线解码。本轮sender reservation trace未触发，不能声称sender超期修复。

CLI产物6047744字节，SHA256 70B526936D87FC096691CC117D38D552AEB096059BE229F31B42DB4ED15C5397，修改时间2026-10-16 10:13:13.659。

VLC原生snapshot返回0，10:14:01.078图片实际检查为三格视频、右下黑，早于CLI失败。仅此画面观察，不证明持续AV同步。外部CPU30样本均值1.736746%、峰4.123379%；WS外部峰259244032字节。内部CPU均值2.017467%、峰7.066381%，WS初始204587008、最终258740224、峰259244032。drift26样本/maxAbs22677ns，不是内容同步证明。最终queued/workers/payload bytes/objects均0，reservations=releases=23956，workerErrors/errors=1，graphPayloadPressureFailures=1必须保留，不能记为0。FFmpeg保留色彩range变更警告及AAC关闭剩2帧；VLC存在picture/buffer late、flushing buffers和截断的`main error: Failed to cr`，不补全未知错误文本。

### 因果边界

失败batch attempts=1、blockedAttempts=0，无前次尝试。deadline29636722739；mux内部队列pop后的采样29646503300，已超9.780561ms；materializer开始29646524900，距mux出口仅21.6us，超9.802161ms；调用返回29646539000，耗时14.1us。因此本次失败发生于sender之前，既非materializer重试耗尽，也不能用sender优先级解释。`observeMuxDequeued`是MediaTsMuxSession::takeScheduledBatch内部pop后的时刻，不是mux输入到达或batch创建时间。

独立审查确认batch本身的deadline可来自AU prepareAccessUnit或维护包prepareMaintenance，不能仅凭一个剩余包或全局最大值判定来源。但现有AU trace提供更强关联：audio canonical_sequence=1049的dispatch同为29636722739，scheduler缺video等待91005400ns，结束29646017600已超过dispatch9294861ns，mux input ready=29646274500。late_output_commit同时记录head_arrived=29555001800、scheduled=29646021500、committed=29646034100。下一步优先复用该证据检查scheduler持有audio但缺video时的等待期限与唤醒条件，不再先扩大mux埋点。selectMediaHead在任一未EOF输入无head时返回nullopt；仍需追缺失video的实际生产链，不能把91ms称为OS Waiting或把全局canvas最大值拼接为同一帧原因。准备存储预算与线程优先级不作未经证明的归因。

```text
[2026-10-16 10:14:04.569] [info] [graph][runtime.node] au_processing_trace stage=late_mux_input generation=1 stream=audio canonical_sequence=1049 dispatch_master_ns=29636722739 ready_master_ns=29646274500 scheduled_created_steady_ns=5638485314400 adapter_dequeued_steady_ns=5638485363300 adapted_created_steady_ns=5638485364200 write_entered_steady_ns=5638485563400 session_ready_steady_ns=5638485565800
[2026-10-16 10:14:04.708] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1049 missing=video begin_steady_ns=5638394303500 end_steady_ns=5638485308900 observed_wait_ns=91005400 held_dispatch_master_ns=29636722739 begin_master_ns=29555012300 end_master_ns=29646017600 dispatch_remaining_at_end_ns=-9294861 resumed_dispatch_master_ns=29668876934
```

### 原始关键日志

```text
[2026-10-16 10:13:39.882] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=843205566 high_water=863228177
[2026-10-16 10:14:04.569] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=1 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=29646524900 mux_dequeued_master_ns=29646503300 first_dequeued_master_ns=29646524500 last_wire_published_master_ns=29636253200 previous_attempt_started_master_ns=unavailable previous_attempt_completed_master_ns=unavailable last_attempt_completed_master_ns=29646539000 last_blocked_started_master_ns=unavailable last_blocked_completed_master_ns=unavailable blocked_attempts=0 remaining_front_release_master_ns=29635543600 remaining_front_deadline_master_ns=29636722739 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=29635543600 materialized_ns=29646524900 protocol_deadline_ns=29636722739 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-16 10:14:04.579] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=2640 committed_datagrams=17665 committed_payload_bytes=21424732 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=17665 pacing_cancelled=0 pacing_submitted=17665 pacing_maximum_submit_lateness_ns=18874220 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=49101800 last_materialized_sequence=17665 last_scheduled_sequence=17665 last_submitted_sequence=17665 last_committed_sequence=17665 shared_scope_charged_datagrams=17665 shared_scope_charged_wire_bytes=21919352 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=17659 endpoint_1_committed_payload_bytes=21424324 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=6 endpoint_2_committed_payload_bytes=408
[2026-10-16 10:14:04.706] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=98888 pressure_failures=0 access_units=1782 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=98820861 maximum_mux_input_after_emit_ns=109551761 maximum_mux_input_after_dispatch_ns=9551761 worst_mux_input_ready_ns=29646274500 worst_mux_input_emit_ns=29536722739 worst_mux_input_dispatch_ns=29636722739 maximum_protocol_batch_produced_after_release_ns=19617512 worst_protocol_batch_produced_ns=29555161600 worst_protocol_batch_release_ns=29535544088 worst_protocol_batch_deadline_ns=29635543601 worst_protocol_batch_datagrams=17 exit_reason=aborted
[CLI] final runtime report: queued=0, peakQueued=856, threads=82, processThreads=132, workers=0, workerProgress=68838, workerProcessCalls=281973, workerWaits=213125, workerWakeups=211180, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=78, logicalProcessors=22, averageSystemMachineCpuPercent=13.537992, averageProcessMachineCpuPercent=2.017467, peakProcessMachineCpuPercent=7.066381, averageProcessSingleCoreCpuPercent=44.384276, peakProcessSingleCoreCpuPercent=155.460385, initialWorkingSetBytes=204587008, workingSetBytes=258740224, peakWorkingSetBytes=259244032, totalPushed=61705, totalPopped=61112, droppedBuffers=0, encodedPacketsPushed=8186, encodedPacketsPopped=8183, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=14595894, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=649, graphPayloadReservations=23956, graphPayloadReleases=23956, graphPayloadPressureFailures=1, lastInputReceivedAtNs=5638484569900
```

### VLC快照命令

```powershell
$r233Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r233Stream=$r233Client.GetStream(); $r233Stream.ReadTimeout=2000; $r233Bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n"); $r233Stream.Write($r233Bytes,0,$r233Bytes.Length); $r233Buffer=[byte[]]::new(4096); try {while(($r233Read=$r233Stream.Read($r233Buffer,0,4096)) -gt 0){[Text.Encoding]::UTF8.GetString($r233Buffer,0,$r233Read)}}catch [IO.IOException] {}} finally {$r233Client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r233-native-*.png' | Select-Object -ExpandProperty FullName
```

### 结束与精确进程清理

```powershell
$r233VlcProcess=Get-CimInstance Win32_Process -Filter 'ProcessId=32944'
if($r233VlcProcess){if($r233VlcProcess.Name -ne 'vlc.exe' -or $r233VlcProcess.CommandLine -notlike '*composition-r233*'){throw 'VLC identity mismatch'};try{$r233Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$r233Bytes=[Text.Encoding]::ASCII.GetBytes("shutdown`r`n");$r233Client.GetStream().Write($r233Bytes,0,$r233Bytes.Length)}finally{$r233Client.Dispose()}}catch{Write-Output $_.Exception.Message};$r233VlcWait=Get-Process -Id 32944 -ErrorAction SilentlyContinue;if($r233VlcWait -and -not $r233VlcWait.WaitForExit(5000)){Stop-Process -Id 32944 -Force;Write-Output 'VLC_FORCE_CLEANUP=32944'}}
```

RC shutdown后5秒VLC未退出，核对身份后强制清理PID32944；不记为自然退出。五个本轮PID复核无残留；无远程测试。

### 临时产物精确清理命令

```powershell
$r233Root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$r233Names=@('composition-r233-cli-error.log','composition-r233-cli.log','composition-r233-monitor.log','composition-r233-native-2026-10-16-10h14m01s078.png','composition-r233-output.sdp','composition-r233-source0-stdout.log','composition-r233-source0.log','composition-r233-source1-stdout.log','composition-r233-source1.log','composition-r233-source2-stdout.log','composition-r233-source2.log','composition-r233-vlc.log')
foreach($r233Name in $r233Names){$r233Path=[IO.Path]::GetFullPath((Join-Path $r233Root $r233Name));if(-not $r233Path.StartsWith($r233Root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup path escaped acceptance directory'};if(Test-Path -LiteralPath $r233Path){Remove-Item -LiteralPath $r233Path -Force;Write-Output "REMOVED $r233Name"}}
Get-ChildItem -LiteralPath $r233Root -Filter 'composition-r233-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(32944,31000,14164,10372,12460)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```


清理复核：12件清单产物全部删除，本轮前缀文件及五个PID均无残留；指定源124427809字节保留。完整目标未完成。

## r234：缺头等待与成熟实现适用性核验（2026-10-16）

本轮只读核对生产链，未修改调度行为、构建或新增实流结论。r233同AU证据仍是当前失败依据。

- MediaAvOutputSchedulerNode::onProcess在selectMediaHead无候选时processWaiting；selectMediaHead要求两个未EOF输入都有head；logMissingMediaWait明确deadline_ns=none。因而持有audio的dispatch不会自动转成缺video等待的截止时间。
- MediaTsMuxSession::writeAccessUnit对所有流共享m_lastAccessUnitEmission，拒绝emitOnMaster回退。不能只加计时器后发audio：例如先发时间10的audio，随后时间9的video即违反当前全局顺序。
- MediaCodecLineageRegistry的submit/resolve/finishGeneration追踪单个媒体身份；VideoEncodeNode接收实际packet后才resolve并创建canonical AU。现有lineage、单帧duration、帧率、重排深度和submission-order队列都没有证明aggregate/filter/encoder/边队列内全部未发布视频的最小dispatch。

权威实现对照：

| 实现 | 实际机制 | 本项目适用判定 |
| --- | --- | --- |
| [FFmpeg max_interleave_delta](https://www.ffmpeg.org/doxygen/trunk/structAVFormatContext.html)（页内max_interleave_delta条目） | 缓存包首尾时间戳跨度超过阈值后，即使未齐流也输出；不是墙钟超时 | 直接等价替换FAIL：当前每路单head，不是同样缓存模型；且现有TS全局顺序仍须成立 |
| [GStreamer GstAggregator源码](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gstreamer/libs/gst/base/gstaggregator.c) | get_next_time由子类提供；到时调用aggregate(self, timeout)，由子类决定语义 | 直接超时放行FAIL：基类并不保证缺失流以后不会出现较早AU；不能把raw mixer的缺流填补照搬为编码包排序 |

GStreamer文档页面本轮抓取失败，实际核对的是官方仓库源码gst_aggregator_wait_and_check与aggregate调用，不能声称页面正文读取成功。FFmpeg为官方trunk资料，未修改或重建外部依赖。

独立审查A明确限定FAIL“现有产品授权缺video提前放audio”：缺少generation内覆盖完整视频生产链、单调推进的dispatch前沿，以及发布/积压/EOF/purge同步语义。该FAIL针对候选捷径，不表示整个任务无路可走。暂不新增该复杂状态机。下一具体证据应关联等待开始时最早未完成视频的canonical identity/dispatch与实际持有阶段，区分确有较早视频未完成和较早视频已排空但scheduler不知道。现有恢复后dispatch只能描述恢复的head，不能倒推等待开始时的安全下界；同理不把全局最慢compose当作该帧原因。
复用观测的具体接点：MediaAvContinuousAggregateVideo::buildVideo已有ExecutionStage::Canvas的video_cursor=m_nextVideoFrame，输出lineage的sequence=m_nextVideoFrame+1；MediaCanvasComposeObservation保留同次最慢compose的steady起止及各嵌套阶段。下一轮先归档aggregate_execution、canvas_compose_correlated、scheduler关联等待及编码packet阶段；必须按身份/区间验证关联，不能直接相加各自最大值。本轮r233日志已按要求清理，已归档内容不包含完整aggregate/canvas阶段原文，无法追溯补齐，需真实复现采集，不能补造旧证据。
独立审查B最终同样限定FAIL超时放行等价替换：共同transportLead使dispatch排序对应emit排序，现有调度计划只包含transport/preparation lead，没有未来dispatch下界。两者均建议优先修复真实缺头来源；本轮没有声称代码修复或验收通过。
