# r337：晚提交诊断自锁修复复验

状态：正常三源链路观察通过，崩溃修复分支验证未完成。r336已捕获根因，单文件修复经双独立静态PASS；Release全量746项重建configure/build exit0，四产物齐全，realtime6481920字节/2026-10-26 11:28:30。本轮未触发带other_dispatch_master_ns的late_output_commit，不能只以无崩溃判定命中修复。

仅realtime，原120秒三源，HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz双声道，TS/RTP。使用VLC原生快照及后台CPU/内存/时钟遥测；同时覆盖完整已接受工作观察。本轮不使用调试器。

## 实际执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r337 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r337-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r337-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


实际PID：源9948、29384、6388；CLI27696；VLC40048。第一张VLC原生快照已查看，三格有画面，右下空格黑。

## 结果与边界

本轮三源正常链路观察通过，崩溃修复触发分支未覆盖。三源均3600帧/120秒自然exit0；VLC原生两快照确认三格→全黑。全部输入结束后会话继续输出黑屏，符合连续合屏行为；CLI在显式caller_stop后exit0，不记为EOF自动退出。

TS产生84258数据报/98186196字节；RTP媒体84258/99197292字节=TS字节+12×84258；RTCP37/2524字节；总提交84295/99199816字节。sender deadline/pressure/partial/ambiguous=0、backlog回零；worker/errors=0，payloadBytes/objects=0，reservation/release均143773、payloadPressureFailures=0。该对账不是NIC实际交付证明。

CPU平均2.768435%、峰8.189655%；WS初始209514496、峰270372864、结束245039104；外部监控至146.2499902秒，最后private1251663872。最后内部phase18141ns、frequency/stretch=0，仅内部时钟遥测，不代表内容A/V验收。

本轮日志中 `late_output_commit.*other_dispatch_master_ns=` 为0条，不能把未崩溃当作该修复分支已实测。下一轮需同规格TS/UDP及失源重入，并继续获取该分支真实触发证据；不得人工更改生产阈值或关闭诊断。完整周期、其他平台及整体矩阵未通过。

## 原生快照命令（两次）

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 40048){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose()
~~~~

## caller_stop与VLC退出命令

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=27696"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r337*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R337Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R337Console]::FreeConsole() | Out-Null
if(-not [R337Console]::AttachConsole(27696)){throw 'AttachConsole failed'}
try {
[R337Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R337Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 27696 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R337Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R337Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 40048){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

## 本轮清理清单

记录后逐项删除下列13项，保留复用源和正式构建。

```
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-native-2026-10-26-11h29m17s105.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-native-2026-10-26-11h30m53s148.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r337-vlc.log
```


清理核验：13项本轮产物已逐项删除，五个精确PID均不存在。
