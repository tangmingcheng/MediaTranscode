# r335：完整已接受工作观察复验

状态：FAIL。Release 构建及双静态审查通过，但本轮真实 CLI 发生 0xc0000409 异常退出。

范围：仅 realtime，三路原 120 秒连续源；HEVC CBR 1280×720/30 8000kbps、AAC CBR 192kbps 44100Hz 双声道，TS/RTP。核验一致工作观察、维护/媒体消费及原期限；不代表完整周期或整体合屏通过。

## 实际执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r335 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r335-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r335-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
$source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($source1.ExitCode)"
$source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($source2.ExitCode)"
exit $cli.ExitCode
~~~~

## 执行记录

Release 全量 746 项重建成功，configure/build exit0；四项产物齐全。realtime 6481920 字节，2026-10-26 11:14:42；仅运行 realtime。

源 PID：20728、30356、33992；CLI 39976；VLC 9948。第一张 VLC 原生快照已实际查看，左上、右上、左下三格有画面，右下空格为黑；最终失败结果见下文。

VLC 原生快照命令：

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 9948){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose()
~~~~

## 失败结果及必要诊断

判定：FAIL。CLI PID 39976 退出 -1073740791（0xc0000409），不是 caller_stop 或自然 EOF。约 60 秒时失败，三个源仍在发送；三源随后各完成 3600 帧/120 秒并自然 exit0。监控会话最终退出同异常码。VLC PID 9948 已通过下列 RC 命令正常退出。

Windows Application 1000：2026-10-26 11:16:02，ucrtbase.dll 10.0.26100.9711+0xa527e，exception 0xc0000409；1001 BEX64 参数9=7，Report ID 4e84196a-1b7c-4bbf-9984-6cfbe2631310。与 r326 具有相同表面签名，但无栈不能证明同一根因。WER 临时 mdmp 已不存在，ReportArchive 仅有 Report.wer；未修改 WER 注册表。

末条生产记录：av_scheduler_trace stage=late_video_head，generation=1，canonical_sequence=1640，dispatch_master_ns=60318277901，now_ns=60433304500。紧前 report 的 graphPayloadPressureFailures 从0变1；该时间关联不是因果证明。workerErrors/errors=0是崩溃前快照，不能据此声称无错误。payloadCurrentBytes=13087906、objects=747、reservations/releases=66957/66210，未取得终态释放或发送对账。

崩溃前 runtime CPU平均2.777578%、峰值7.448790%；WS初始208506880、最后266182656、峰值267161600。外部监控到64.8563956秒，异常报告期间WS升至369524736、private1268117504。最后 av_drift_trace filtered_phase=4536ns、frequency/stretch=0，仅内部时钟遥测，不是内容 A/V 证明。源后黑屏、重入及正常退出门禁未完成。

第一张原生快照已查看三格有画面，失败后不把冻结画面作为失源黑屏证据。下一步保持同构建/同规格，用现有 CDB 捕获异常栈。

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 9948){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("quit`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose()
~~~~

## 本轮清理清单

以下12项在记录完成后逐项清除；保留复用120秒源及正式构建产物。

```
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-native-2026-10-26-11h15m34s181.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r335-vlc.log
```




清理核验：12 项清单文件已逐项删除，本轮五个 PID 均不存在。late_video_head 是正常诊断而非失败分支；压力计数也不是重复释放证据。仅保留源码候选，等待真实异常栈。
