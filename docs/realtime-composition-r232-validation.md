# r232：协议批次重试关联诊断

## 失败驱动与方法

r231原规格三源链路在物化前超期13.810672ms；同batch七次尝试未消费，首次出队至失败51.4869ms。既有日志缺少前次返回时刻及WouldBlock原因，无法区分调用内部和调用间隔。

采用[Microsoft CPU Analysis](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis)区分Running、Ready与Waiting的分析原则：通知不代表立即运行，墙钟间隔不能直接当CPU耗时或等待时间。本轮先补应用层边界观测，不声称代替OS线程状态跟踪。

## 修改

仅修改MediaProtocolBatchMaterializationObservation.h与MediaMpegTsDatagramMaterializerNode.cpp：

- 每个batch保留前次/本次调用开始和完成时刻、最近WouldBlock起止与次数；调用范围包括generation许可和materialize。
- 最近阻塞保留原生错误码、原消息长度和最多256字节文本；显式标明截断。256仅是诊断文本存储长度，不参与媒体容量或策略。
- 原位固定内存记录，无逐包日志或热路径字符串分配；在原失败trace中输出。新batch清局部历史，跨batch lastWirePublishedAt仍明确独立。
- 时钟不可用输出unavailable；原始错误、deadline、reservation、队列与线程策略不变。成功和失败路径都记录返回边界。
- 两名独立非实现者静态审查均明确PASS；该结论不代表根因已修复。

## 证据限制

只保留最近状态，不能还原全部重试。尝试间隔仍包括工作线程返回、等待、唤醒、调度和节点入口；需要结合具体阻塞来源再选择下一处诊断。没有提高期限、源等待或任何验收参数。

## 构建与启动

Release全量clean-first/all-target构建会话4516退出0，718项构建计划。realtime CLI 6044672字节，SHA256 61936744636C4931DD33B0778A63A4C21445753E5738EDD55B0ACA99F2E4EF33。

VLC15092、源39340/37796/37896、CLI38992；本轮启动与监控命令：

```powershell
$ErrorActionPreference='Stop'
$r232Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r232-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r232Vlc.Id)"
$r232Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r232Source0.Id)"
$r232Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r232Source1.Id)"
$r232Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r232Source2.Id)"
$r232Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r232 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r232Cli.Id)"
$r232Clock=[Diagnostics.Stopwatch]::StartNew()
$r232LastTime=0.0
$r232LastCpu=0.0
$r232Cores=[Environment]::ProcessorCount
while(-not $r232Cli.WaitForExit(1000)) {
$r232Cli.Refresh()
$r232Time=$r232Clock.Elapsed.TotalSeconds
$r232Cpu=$r232Cli.TotalProcessorTime.TotalSeconds
$r232Usage=100*($r232Cpu-$r232LastCpu)/(($r232Time-$r232LastTime)*$r232Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-monitor.log' -Value "seconds=$r232Time cpu=$r232Usage ws=$($r232Cli.WorkingSet64) private=$($r232Cli.PrivateMemorySize64)"
$r232LastTime=$r232Time
$r232LastCpu=$r232Cpu
}
Write-Output "CLI_EXIT=$($r232Cli.ExitCode)"
$r232Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r232Source0.ExitCode)"
$r232Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r232Source1.ExitCode)"
$r232Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r232Source2.ExitCode)"
exit $r232Cli.ExitCode
```

## 实流结果：FAIL（2026-10-15执行，2026-10-16补齐归档）

初始三源各完成120秒/3600帧并exit0。16:55:47.791节点82 MediaScheduledDatagramSenderNode在重入期间失败；CLI自行exit1，不是caller_stop。重入PID37428随后核对身份后强制清理，exit=-1，不能记为完整120秒或自然结束。本轮没有抓包、录制或离线解码，不能提供对应通过结论。

VLC原生快照均返回0：16:53:28.817三格视频及右下黑；16:55:06.429全黑，源结束后CLI仍持续运行；16:56:18.038显示左下格重入图像，但该快照晚于CLI失败，只是VLC保留的画面，不能证明截图时链路仍在播放。未使用系统屏幕截图。

CPU外部169样本均值2.184473%、峰4.619048%；内部均值2.227107%、峰5.871560%。WS初始207925248、最终254619648、峰268226560字节。准备存储graph_bound retained845279746、high_water862697228、capacity2147483648。内部drift generation1有113样本/maxAbs4537ns，generation2有9样本/maxAbs11338ns；不代表内容AV证明。

最终queued/workers/payload bytes/objects均0，reservations=releases=124203，pressureFailures=0。workerErrors/errors=2，日志包含同一sender错误的secondary failure，不应改写成只有一条失败。FFmpeg保留unknown→tv range警告及AAC关闭剩2帧；VLC有零sample、D3D纹理尺寸/切片、playback too late、picture late及两条converter失败。

### 同一数据报的关联证据

原始错误：

```text
IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=92093 now_ns=174817113300 release_ns=174745050884 deadline_ns=174811717552 not_before_ns=174809535120 not_after_ns=174811681072 duration_ns=36480 rate=6250000 queue_wire_bytes=228 average_queue_residence_ns=7667000
```

同序号92093的materializer aborted记录：materialized_at=174809446300、release=174745050884、deadline=174811717552。此materialized_at是调用开始采样，非物化完成；采样距release64.395416ms，距deadline2.271252ms，至pacer检查7.667ms，检查时超完成deadline5.395748ms（超过最晚可开始时间5.432228ms）。此处两个日志有相同global sequence，可以关联；不再用无关全会话峰值归因。

本次失败发生在pacer reservation创建阶段，未触发materializer的failure trace，故新增重试原因输出尚无本次失败样本，不能声称r231物化前超期已修复。约7.667ms涵盖物化调用开始至pacer检查的路径，尚不能拆分为计算、队列、锁或OS调度耗时。保持原deadline与测试规格，不把拒绝过期任务改成继续发送。

### 重入命令

```powershell
$r232Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r232-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($r232Reentry.Id)"
$r232Reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($r232Reentry.ExitCode)"
exit $r232Reentry.ExitCode
```

### VLC原生快照（三次执行）

```powershell
$r232Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r232Stream=$r232Client.GetStream(); $r232Stream.ReadTimeout=2000; $r232Bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n"); $r232Stream.Write($r232Bytes,0,$r232Bytes.Length); $r232Buffer=[byte[]]::new(4096); try {while(($r232Read=$r232Stream.Read($r232Buffer,0,4096)) -gt 0){[Text.Encoding]::UTF8.GetString($r232Buffer,0,$r232Read)}}catch [IO.IOException] {}} finally {$r232Client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r232-native-*.png' | Select-Object -ExpandProperty FullName
```

### 失败后终止剩余测试进程（2026-10-15执行）

```powershell
$r232Process=Get-CimInstance Win32_Process -Filter 'ProcessId=37428'
if($r232Process){if($r232Process.Name -ne 'ffmpeg.exe' -or $r232Process.CommandLine -notlike '*61020*' -or $r232Process.CommandLine -notlike '*test-continuous-120s.mp4*'){throw 'Reentry identity mismatch'};Stop-Process -Id 37428 -Force;Write-Output 'REENTRY_CLEANUP_PID=37428'}
$r232Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$r232Bytes=[Text.Encoding]::ASCII.GetBytes("shutdown`r`n");$r232Client.GetStream().Write($r232Bytes,0,$r232Bytes.Length)}finally{$r232Client.Dispose()}
```

2026-10-16续轮核对，本机没有CLI/FFmpeg/VLC进程。会话中断前尚未归档及删除的16件r232临时文件仍存在，本轮补齐记录后按精确清单删除；不能宣称此前已即时完成清理。无远程测试或产物。完整目标仍75/FAIL。

双独立源码复审均限定PASS诊断安全性，实流仍FAIL。reserve在beginSubmitGroup中同步失败，未到该包waitUntil；pacingQueueSnapshot在获取ledger mutex后采样，因此不能排除锁前时间。pacing_reserved/submitted均92092、last_scheduled_sequence92093；deadline_misses=0的计数范围不覆盖本次reservation拒绝。下一步以同sequence关联物化调用返回、wire发布、sender出队/激活及snapshot锁前/锁后边界。

### 精确清理命令

```powershell
$r232Root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$r232Names=@('composition-r232-cli-error.log','composition-r232-cli.log','composition-r232-monitor.log','composition-r232-native-2026-10-15-16h53m28s817.png','composition-r232-native-2026-10-15-16h55m06s429.png','composition-r232-native-2026-10-15-16h56m18s038.png','composition-r232-output.sdp','composition-r232-reentry-stdout.log','composition-r232-reentry.log','composition-r232-source0-stdout.log','composition-r232-source0.log','composition-r232-source1-stdout.log','composition-r232-source1.log','composition-r232-source2-stdout.log','composition-r232-source2.log','composition-r232-vlc.log')
foreach($r232Name in $r232Names){$r232Path=[IO.Path]::GetFullPath((Join-Path $r232Root $r232Name));if(-not $r232Path.StartsWith($r232Root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup path escaped acceptance directory'};if(Test-Path -LiteralPath $r232Path){Remove-Item -LiteralPath $r232Path -Force;Write-Output "REMOVED $r232Name"}}
Get-ChildItem -LiteralPath $r232Root -Filter 'composition-r232-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(15092,39340,37796,37896,38992,37428)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

清理复核：16件清单文件均删除；r232前缀残留0、六个本轮PID残留0，指定源124427809字节保留。

## 后续执行契约缺口（只读核验，非超期根因结论）

独立审查确认MediaThreadingPolicy.priority的High仅被保存/校验，没有worker系统优先级消费者。合屏Preparer设置High，VideoRunController传runtime，RuntimeLifecycleExecutor传ThreadedExecutor；后者构造worker时不传priority。动态MediaRuntimeBranch同样不传。MediaGraphWorker::start直接std::thread，run仅有CPU计时、Linux命名、节点执行和等待；src/tools/include未找到SetThreadPriority、AvSetMmThread、pthread_setschedparam、pthread_setschedprio、sched_setscheduler、SetPriorityClass或native_handle调用。

线程mode确实参与执行，不应把整个threading policy称为无效。外部启动器的进程优先级、操作系统实际调度及FFmpeg自有线程不属于该源码结论。后续需调查成熟平台调度adapter及可用权限，明确High/Realtime语义、失败传播和主图/动态branch共用方式后实现；不能临时提高所有线程优先级或放宽期限当作已证实修复。

## 2026-10-16 调度证据预检（r233，无实流结论）

按 Microsoft [CPU Analysis](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis) 的 Running / Ready / Waiting 分析方式，先验证本机调度采集能力，不根据全局CPU均值推断线程延迟。WPR报告未录制；MMCSS服务运行，Playback注册表事实为Priority=3、Scheduling Category=Medium、Background Only=False。这些事实不证明本项目worker注册了MMCSS，也不是提高优先级即可修复的证据。

实际执行：

```powershell
& 'C:/Windows/System32/wpr.exe' -status
& 'C:/Program Files (x86)/Windows Kits/10/Windows Performance Toolkit/xperf.exe' -loggers
& 'C:/Program Files (x86)/Windows Kits/10/Windows Performance Toolkit/xperf.exe' -on PROC_THREAD+LOADER+CSWITCH+DISPATCHER -stackwalk CSwitch+ReadyThread -BufferSize 64 -MinBuffers 64 -MaxBuffers 128 -MaxFile 64 -FileMode Circular -f 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r233-trace-probe.etl'
```

采集启动失败：NT Kernel Logger，0x5 Access denied，进程exit=-2147024891。复查logger列表无NT Kernel Logger，r233前缀文件0；未停止或修改其他已有系统采集会话。未启动CLI/FFmpeg/VLC，无媒体验收、截图或待清理产物。工具存在不等于当前令牌具有内核采集权限。

再次核对MediaWireGlobalSequenceState::pacingQueueSnapshot：先取得m_mutex，再clock.now；sender将sampledAt直接传reserve。MediaSteadyMasterClock使用私有steady anchor，不能直接将日志running-time当作ETW时间戳。后续进程内诊断应复用同一master clock，以global sequence关联物化返回、发布、sender激活及snapshot调用前后；调用前后之差仍包含锁、计算和调度，不能仅凭该差值命名为锁等待。当前ETW权限仅限制调度细分，不阻断上述已授权源码工作。未改变priority、deadline或验收规格。
### r233 sender边界诊断设计

采用官方CPU关键路径分析的边界观测方法，先覆盖现有sender owner线程内activation开始/完成、每包permit开始/完成、queue snapshot调用开始/完成与既有锁后sampledAt、pacer调用返回。按generation/endpoint/global sequence输出，仅reservation失败时格式化，正常路径固定大小可选时间，不新增队列、线程、公开参数或发送策略。抽取已有State级master-clock采样helper供物化与sender共用，采样失败表示unavailable，不能替代生产错误。activation在成功接管pending batch时保存；每包尝试重置局部观测，避免前一包时间污染。所有平台共用同一DAG sender。该切片不提供跨节点物化返回/发布时刻，不宣称可单独区分锁、调度和计算，也不修复超期；依据实流失败分段结果继续定位。
双独立非实现者r233_boundary_review_a/b均明确限定PASS：六文件静态诊断安全性通过，不代表超期修复。activation为批级，permit/snapshot/reservation为本次包级；State采样有开销，不宣称零时序影响。下一轮按原规格实流采样验证。
