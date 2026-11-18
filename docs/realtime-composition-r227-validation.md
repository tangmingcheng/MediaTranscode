# r227：源保留数量配额与 Windows realtime 复验

## 实现与审查

固定存储事实先于账户预算生成；reference-only编译不要求占位预算，正式admission仍要求非零预算。源配额改为最终图可达媒体上界，加opened decoder内部packet以及Raw输入atomic completion，不再由输入fps乘输出延迟推算。启动缓存和acquisition不重复整块叠加；decoded startup帧容量不计入packet minimum。minimumProgress独立保留。普通节点引用仍为保守上界，硬件surface进展和非cadence入口尚未完成。

两名未参与实现的审查员均限定PASS。Release全量重建会话46999退出0，717项构建计划及必需产物检查通过；realtime CLI 6039040字节，SHA256 68692734F4C28C60D648CE82F0379A75F0DEFD57BF53A156205B110B7366A8A3。无外部依赖修改。

## 实际结果与边界

2026-10-15，三路H264 1280×720/30fps约8Mbps，第三路带AAC44100Hz双声道；输出HEVC 1280×720/30fps 8Mbps CBR/GOP60＋AAC192kbps，MPEG-TS/RTP62020。输入等待100/150/200ms，共享准备存储2GiB。初始三源及AV重入源各完整120秒、3600帧、退出0。

VLC原生快照：15:52:34三格视频＋第四格黑；15:54:06全黑；15:55:04仅左下格重入；15:56:46再次全黑。未使用系统屏幕截图。全失源时CLI继续输出，随后caller_stop退出0；不是源EOF导致CLI自动结束。排空15:57:09.081→09.336，255ms。最终queued/workers/errors/workerErrors/payload bytes/objects均0，reservations/releases各227286，payload pressure failures0。CPU平均1.735309%、峰值9.333333%；WS初始203489280、结束252481536、峰值260014080字节。准备存储retained842016719、high_water859389064，低于2147483648。

A/V telemetry：generation1共113点，最大绝对raw_phase6804ns；generation2共117点，最大15874ns。这是运行时时钟数据，不是内容级同步证明。VLC有3条playback too late、4条Failed to create video converter；FFmpeg有色彩range unknown→tv变化警告，均保留，不宣称无警告。

抓包613966包，接口drop0；输出RTP176674包、SSRC4b43a232、序号断点0；提取TS203894648字节，同步错误0。ffprobe退出0，视频9330包、音频13393包，逐流DTS严格递增；离线FFmpeg全量音视频解码退出0，诊断日志为空。首次解析因tshark末尾附加“No matching RTP/H.264 payload dumped.”非十六进制文本失败；确认仅此行后显式跳过，重新解析成功，不将该行当包。

本轮真实证据支持canonical三源播放、失源黑屏、重入、配额运行及排空。未完成内容级A/V、静音检测、HEVC重排输入、硬件surface进展和RKMPP，完整合屏仍75/FAIL，不创建成功提交。PID：VLC14232、capture20772、sources38532/32020/8164、CLI6676、reentry17404；结束后均已不在进程表。

## 实际命令

### 初始链路与后台监控

```powershell
$ErrorActionPreference='Stop'
$r227Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-wiring-r227-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r227Vlc.Id)"
$r227Capture=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i "\Device\NPF_Loopback" -f "udp and (dst portrange 61020-61041 or dst port 62020)" -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-wire.pcapng -q' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-capture-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-capture.log' -PassThru
Write-Output "CAPTURE_PID=$($r227Capture.Id)"
$r227Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r227Source0.Id)"
$r227Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r227Source1.Id)"
$r227Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r227Source2.Id)"
$r227Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-wiring-r227 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r227Cli.Id)"
$r227Clock=[Diagnostics.Stopwatch]::StartNew()
$r227LastTime=0.0
$r227LastCpu=0.0
$r227Cores=[Environment]::ProcessorCount
while(-not $r227Cli.WaitForExit(1000)) {
$r227Cli.Refresh()
$r227Time=$r227Clock.Elapsed.TotalSeconds
$r227Cpu=$r227Cli.TotalProcessorTime.TotalSeconds
$r227Usage=100*($r227Cpu-$r227LastCpu)/(($r227Time-$r227LastTime)*$r227Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-monitor.log' -Value "seconds=$r227Time cpu=$r227Usage ws=$($r227Cli.WorkingSet64) private=$($r227Cli.PrivateMemorySize64)"
$r227LastTime=$r227Time
$r227LastCpu=$r227Cpu
}
Write-Output "CLI_EXIT=$($r227Cli.ExitCode)"
$r227Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r227Source0.ExitCode)"
$r227Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r227Source1.ExitCode)"
$r227Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r227Source2.ExitCode)"
exit $r227Cli.ExitCode
```

### 重入源

```powershell
$r227Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($r227Reentry.Id)"
$r227Reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($r227Reentry.ExitCode)"
exit $r227Reentry.ExitCode
```

### VLC原生快照（四次执行）

```powershell
$r227Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r227Stream=$r227Client.GetStream(); $r227Stream.ReadTimeout=2000; $r227Bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n"); $r227Stream.Write($r227Bytes,0,$r227Bytes.Length); $r227Buffer=[byte[]]::new(4096); try {while(($r227Read=$r227Stream.Read($r227Buffer,0,4096)) -gt 0){[Text.Encoding]::UTF8.GetString($r227Buffer,0,$r227Read)}}catch [IO.IOException] {}} finally {$r227Client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-wiring-r227-native-*.png' | Select-Object -ExpandProperty FullName
```

### 调用方停止、停止抓包及关闭VLC

```powershell
$r227Target=Get-CimInstance Win32_Process -Filter "ProcessId=6676"
if(-not $r227Target -or $r227Target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $r227Target.CommandLine -notlike '*composition-wiring-r227*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R227Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R227Console]::FreeConsole() | Out-Null
if(-not [R227Console]::AttachConsole(6676)){throw 'AttachConsole failed'}
try {
[R227Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$r227Ids=[uint32[]]::new(16)
$r227Count=[R227Console]::GetConsoleProcessList($r227Ids,16)
if($r227Count -ne 2 -or $r227Ids[0..1] -notcontains 6676 -or $r227Ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R227Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R227Console]::FreeConsole() | Out-Null}
$r227Target=Get-CimInstance Win32_Process -Filter "ProcessId=20772"
if(-not $r227Target -or $r227Target.Name -ne 'dumpcap.exe' -or $r227Target.CommandLine -notlike '*composition-wiring-r227*'){throw 'Capture identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R227CaptureConsole {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R227CaptureConsole]::FreeConsole() | Out-Null
if(-not [R227CaptureConsole]::AttachConsole(20772)){throw 'AttachConsole failed'}
try {
[R227CaptureConsole]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$r227Ids=[uint32[]]::new(16)
$r227Count=[R227CaptureConsole]::GetConsoleProcessList($r227Ids,16)
if($r227Count -ne 2 -or $r227Ids[0..1] -notcontains 20772 -or $r227Ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R227CaptureConsole]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'Capture caller-stop requested'
} finally {[R227CaptureConsole]::FreeConsole() | Out-Null}
$r227Vlc=Get-CimInstance Win32_Process -Filter "ProcessId=14232"
if(-not $r227Vlc -or $r227Vlc.Name -ne 'vlc.exe' -or $r227Vlc.CommandLine -notlike '*composition-wiring-r227*'){throw 'VLC identity mismatch'}
$r227Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r227Bytes=[Text.Encoding]::ASCII.GetBytes("quit`r`n"); $r227Client.GetStream().Write($r227Bytes,0,$r227Bytes.Length)} finally {$r227Client.Dispose()}
```

### 停流后离线检查

```powershell
& 'D:/Wireshark/tshark.exe' -n -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-wire.pcapng' -Y 'udp.dstport == 62020' -T fields -e udp.payload 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-tshark.log' | Set-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-output-payloads.txt'
exit $LASTEXITCODE
& 'D:/Python/python.exe' -c 'from pathlib import Path
import json, re
root=Path(''D:/Code/MyCode/MediaTranscode/out/acceptance'')
prefix=''composition-wiring-r227''
count=0; gaps=0; sync=0; total=0; prior={}; pts={}
with (root/(prefix+''-wire.ts'')).open(''wb'') as out:
 for line in (root/(prefix+''-output-payloads.txt'')).open(encoding=''utf-8-sig''):
  if line.strip()==''No matching RTP/H.264 payload dumped.'': continue
  data=bytes.fromhex(line.strip().replace('':'',''''))
  if not data: continue
  if len(data)<12 or data[0]>>6!=2: raise ValueError(''Invalid RTP'')
  seq=int.from_bytes(data[2:4],''big''); ssrc=data[8:12].hex()
  if ssrc in prior and seq!=((prior[ssrc]+1)&65535): gaps+=1
  prior[ssrc]=seq
  offset=12+4*(data[0]&15)
  if data[0]&16: offset+=4+4*int.from_bytes(data[offset+2:offset+4],''big'')
  end=len(data)-(data[-1] if data[0]&32 else 0)
  payload=data[offset:end]
  if len(payload)%188: raise ValueError(''TS alignment'')
  sync+=sum(payload[i]!=0x47 for i in range(0,len(payload),188))
  out.write(payload); total+=len(payload); count+=1
text=(root/(prefix+''-cli.log'')).read_text(encoding=''utf-8-sig'')
drifts={}
for g,v in re.findall(r''av_drift_trace generation=(\d+).*?raw_phase_ns=(-?\d+)'',text):
 stats=drifts.setdefault(g,{''samples'':0,''max_abs_raw_ns'':0}); stats[''samples'']+=1; stats[''max_abs_raw_ns'']=max(stats[''max_abs_raw_ns''],abs(int(v)))
print(json.dumps({''rtp_packets'':count,''ssrc'':list(prior),''sequence_discontinuities'':gaps,''ts_bytes'':total,''ts_sync_errors'':sync,''drift'':drifts}))
'
& 'D:/mabs/local64/bin-video/ffprobe.exe' -v error -show_packets -show_entries packet=stream_index,pts,dts -of csv=p=0 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-wire.ts' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-packets.csv'
exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -v warning -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-wire.ts' -map 0:v:0 -map 0:a:0 -f null - 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-wire-decode.log'
exit $LASTEXITCODE
& 'D:/Python/python.exe' -c 'from pathlib import Path
import csv,json
p=Path("D:/Code/MyCode/MediaTranscode/out/acceptance/composition-wiring-r227-packets.csv")
counts={}; last={}; bad={}
for row in csv.reader(p.open(encoding="utf-8-sig")):
 if len(row)<3 or not row[0].isdigit(): continue
 k=row[0]; d=int(row[2]); counts[k]=counts.get(k,0)+1
 if k in last and d<=last[k]: bad[k]=bad.get(k,0)+1
 last[k]=d
print(json.dumps({"packets":counts,"nonincreasing_dts":bad}))'
```


## 临时产物清理清单

composition-wiring-r227-capture-stdout.log
composition-wiring-r227-capture.log
composition-wiring-r227-cli-error.log
composition-wiring-r227-cli.log
composition-wiring-r227-monitor.log
composition-wiring-r227-native-2026-10-15-15h52m34s038.png
composition-wiring-r227-native-2026-10-15-15h54m06s729.png
composition-wiring-r227-native-2026-10-15-15h55m04s670.png
composition-wiring-r227-native-2026-10-15-15h56m46s104.png
composition-wiring-r227-output-payloads.txt
composition-wiring-r227-packets.csv
composition-wiring-r227-reentry-stdout.log
composition-wiring-r227-reentry.log
composition-wiring-r227-source0-stdout.log
composition-wiring-r227-source0.log
composition-wiring-r227-source1-stdout.log
composition-wiring-r227-source1.log
composition-wiring-r227-source2-stdout.log
composition-wiring-r227-source2.log
composition-wiring-r227-tshark.log
composition-wiring-r227-vlc.log
composition-wiring-r227-wire-decode.log
composition-wiring-r227-wire.pcapng
composition-wiring-r227-wire.ts
composition-wiring-r227.sdp

清理复核：以上25件本轮产物已逐项删除，r227匹配残留0；指定复用源保留。未执行远程测试，未产生远程产物。
