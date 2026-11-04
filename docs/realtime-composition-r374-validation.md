# r374 Windows Release realtime 接管与重入复验

状态：部分运行行为已验证，完整验收未通过（初始图像由残留旧VLC取得，测试隔离不合格）。Release778目标全量构建，configure/build0；本次准备修复双独立局部PASS。

初始源PID30768/10496/31452，CLI30900，VLC12612；所选AV重入PID35400。协议source2/1/0在10:00:15.506至.510完成安装，decoder契约在.641/.690/.734绑定，直接证明本轮先协议接管再decoder准备。

旧r371 VLC34696占用UDP62024，r372 VLC21416也残留；提升环境CIM核对命令行后已清理，旧报告无残留断言已更正。初始三格后来查到由残留旧VLC保存，来源见末尾更正；不能计测试隔离通过。清理后原生快照10h02m37s319确认全黑；10h03m43s266确认左下重入画面、其余黑。

## 实际媒体命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r374 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r374-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-reentry.log' -PassThru
```

## 结束结果

三个初始源及重入源均3600帧/120秒、退出0；CLI经caller-stop退出0，不是全失源自动退出。两次全黑及左下恢复快照已实际查看；第二次全黑为10h05m15s994。初始三格由残留旧VLC以旧文件名前缀取得，见末尾来源核验；测试隔离不合格。

sender提交180506 datagrams，deadline/partial/ambiguous失败0，delivery_evidence仍not_proven；本轮未复现r371/r372运行期错误不证明已修复。内部A/V锁定不等于内容对齐。

## 调用方停止命令

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=30900"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r374*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R374Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R374Console]::FreeConsole() | Out-Null
if(-not [R374Console]::AttachConsole(30900)){throw 'AttachConsole failed'}
try {
[R374Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R374Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 30900 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R374Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R374Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 12612){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```

## 全部非周期CLI诊断和完整stderr

```text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 10:00:15.502] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:00:15.506] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-02 10:00:15.507] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:00:15.507] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:00:15.510] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-02 10:00:15.510] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-02 10:00:15.640] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:00:15.640] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:00:15.641] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r374.source.0 decoder=h264
[2026-11-02 10:00:15.690] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:00:15.690] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:00:15.690] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r374.source.1 decoder=h264
[2026-11-02 10:00:15.734] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:00:15.734] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:00:15.734] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r374.source.2 decoder=h264
[2026-11-02 10:00:15.801] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:00:15.838] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:00:15.838] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-02 10:00:15.846] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:00:15.846] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:00:15.846] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:00:15.851] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:00:15.851] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:00:15.851] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:00:15.856] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:00:15.857] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:00:15.857] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:00:15.879] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=816944170 high_water=834373061
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-02 10:00:15.881] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.882] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.883] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.884] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.885] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.886] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.887] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.888] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-02 10:00:15.889] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:00:15.890] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:00:15.890] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:00:15.890] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:00:15.890] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-02 10:00:15.890] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-02 10:00:15.890] [info] [graph][runtime.node] register node=1 name=composition-r374.output.video.codec_resolver kind=CodecResolver
[2026-11-02 10:00:15.890] [info] [graph][runtime.node] register node=2 name=composition-r374.output.video.encode kind=VideoEncode
[2026-11-02 10:00:15.890] [info] [graph][runtime.node] register node=3 name=composition-r374.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:00:15.890] [info] [graph][runtime.node] register node=4 name=composition-r374.output.audio.encode kind=AudioEncode
[2026-11-02 10:00:15.890] [info] [graph][runtime.node] register node=5 name=composition-r374.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=8 name=composition-r374.source_0.input kind=RawRtpInput
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=9 name=composition-r374.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=10 name=composition-r374.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=11 name=composition-r374.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=12 name=composition-r374.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=13 name=composition-r374.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=14 name=composition-r374.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=15 name=composition-r374.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=16 name=composition-r374.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=17 name=composition-r374.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=18 name=composition-r374.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=20 name=composition-r374.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=21 name=composition-r374.source_0.video.codec_resolver kind=CodecResolver
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=22 name=composition-r374.source_0.video.decode kind=VideoDecode
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=23 name=composition-r374.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=24 name=composition-r374.source_0.video.framerate kind=VideoFrameRate
[2026-11-02 10:00:15.891] [info] [graph][runtime.node] register node=25 name=composition-r374.source_0.video.preparation kind=VideoPreparation
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=26 name=composition-r374.source_0.video.filter kind=VideoFilter
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=27 name=composition-r374.source_1.input kind=RawRtpInput
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=28 name=composition-r374.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=29 name=composition-r374.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=30 name=composition-r374.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=31 name=composition-r374.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=32 name=composition-r374.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=33 name=composition-r374.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=34 name=composition-r374.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=35 name=composition-r374.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=36 name=composition-r374.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=37 name=composition-r374.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=39 name=composition-r374.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=40 name=composition-r374.source_1.video.codec_resolver kind=CodecResolver
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=41 name=composition-r374.source_1.video.decode kind=VideoDecode
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=42 name=composition-r374.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=43 name=composition-r374.source_1.video.framerate kind=VideoFrameRate
[2026-11-02 10:00:15.892] [info] [graph][runtime.node] register node=44 name=composition-r374.source_1.video.preparation kind=VideoPreparation
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=45 name=composition-r374.source_1.video.filter kind=VideoFilter
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=46 name=composition-r374.source_2.video.input kind=RawRtpInput
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=47 name=composition-r374.source_2.audio.input kind=RawRtpInput
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=48 name=composition-r374.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=49 name=composition-r374.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=50 name=composition-r374.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=51 name=composition-r374.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=52 name=composition-r374.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=53 name=composition-r374.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=54 name=composition-r374.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=55 name=composition-r374.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=56 name=composition-r374.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=57 name=composition-r374.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=58 name=composition-r374.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=59 name=composition-r374.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=60 name=composition-r374.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:00:15.893] [info] [graph][runtime.node] register node=62 name=composition-r374.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=63 name=composition-r374.source_2.video.codec_resolver kind=CodecResolver
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=64 name=composition-r374.source_2.video.decode kind=VideoDecode
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=65 name=composition-r374.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=66 name=composition-r374.source_2.video.framerate kind=VideoFrameRate
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=67 name=composition-r374.source_2.video.preparation kind=VideoPreparation
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=68 name=composition-r374.source_2.video.filter kind=VideoFilter
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=69 name=composition-r374.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=70 name=composition-r374.source_2.audio.encode.decode kind=AudioDecode
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=71 name=composition-r374.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=72 name=composition-r374.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=73 name=composition-r374.source_2.audio.encode.resample kind=AudioResample
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=74 name=composition-r374.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=75 name=composition-r374.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=76 name=composition-r374.output.mpegts.mux kind=FileMux
[2026-11-02 10:00:15.894] [info] [graph][runtime.node] register node=77 name=composition-r374.output.mpegts.plan.source kind=Unknown
[2026-11-02 10:00:15.895] [info] [graph][runtime.node] register node=78 name=composition-r374.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-02 10:00:15.895] [info] [graph][runtime.node] register node=79 name=composition-r374.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-02 10:00:15.895] [info] [graph][runtime.node] register node=80 name=composition-r374.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-02 10:00:15.895] [info] [graph][runtime.node] register node=81 name=composition-r374.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-02 10:00:15.895] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-02 10:00:15.895] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-02 10:00:15.896] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-02 10:00:15.896] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:00:15.897] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-02 10:00:15.897] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:00:15.897] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:15.897] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-02 10:00:15.897] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:00:15.897] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-02 10:00:15.897] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:15.899] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-02 10:00:15.899] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:00:15.899] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:00:15.900] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-11-02 10:00:15.900] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274818350800 group=composition-r374.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=509351300 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.900] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274818498800 group=composition-r374.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.900] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:15.901] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:00:15.901] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-11-02 10:00:15.901] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=180472488 dts=180472488 size=33389 generation=1 sequence=1
[2026-11-02 10:00:15.901] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274819209800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.901] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274819446700 group=composition-r374.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=510434700 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.901] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274819613500 group=composition-r374.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.901] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3737440111 dts=3737440111 size=33389 generation=1 sequence=1
[2026-11-02 10:00:15.902] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274819876300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.902] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-02 10:00:15.903] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-02 10:00:15.903] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-02 10:00:15.903] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:00:15.904] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:00:15.904] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-02 10:00:15.904] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-11-02 10:00:15.904] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:00:15.905] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274822746700 group=composition-r374.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6009070 epoch_master_release_ns=513624300 video_count=16 min_presentation_ns=-11000000 max_end_ns=522333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.905] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274822882800 group=composition-r374.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.905] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1116889386 dts=1116889386 size=33389 generation=1 sequence=1
[2026-11-02 10:00:15.906] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274823751000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.930] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:00:15.930] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274848380800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.930] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=180472488 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274848987900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274849058800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1116889386 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274849376900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000275F6569230 stream=Video payload=Frame pts=180472488 dts=180472488 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274849612700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.931] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274849686100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.932] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274850057100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.932] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000275F6569230 stream=Video payload=Frame pts=180472488 dts=180472488 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.932] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274850206000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.932] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000275F656A5E0 stream=Video payload=Frame pts=1116889386 dts=1116889386 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.932] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3737440111 format=cuda size=1280x720 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-02 10:00:15.932] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000275F6569230 stream=Video payload=Frame pts=180472488 dts=180472488 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.932] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274850763400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000275F656A5E0 stream=Video payload=Frame pts=1116889386 dts=1116889386 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274850925100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274851124500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000275F6568E40 stream=Video payload=Frame pts=180472488 dts=180472488 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274851387500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.934] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274852190300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.934] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000275F6569A10 stream=Video payload=Frame pts=1116889386 dts=1116889386 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000275F656A5E0 stream=Video payload=Frame pts=1116889386 dts=1116889386 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000275F6568A50 stream=Video payload=Frame pts=3737440111 dts=3737440111 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.934] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000275F6569A10 stream=Video payload=Frame pts=1116889386 dts=1116889386 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.933] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000275F6568E40 stream=Video payload=Frame pts=180472488 dts=180472488 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.935] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274853047400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.935] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000275F6568A50 stream=Video payload=Frame pts=3737440111 dts=3737440111 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.935] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:00:15.935] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000275F6568A50 stream=Video payload=Frame pts=3737440111 dts=3737440111 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.935] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:00:15.936] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274853924400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.936] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000275802462A0 stream=Video payload=Frame pts=3737440111 dts=3737440111 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.936] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000275802462A0 stream=Video payload=Frame pts=3737440111 dts=3737440111 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:00:15.936] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:00:15.981] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=45
[2026-11-02 10:00:15.993] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274910838800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.993] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=56
[2026-11-02 10:00:15.993] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274911299100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.993] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=59
[2026-11-02 10:00:15.993] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274911566600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.994] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274911909100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.994] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=372296 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274912246900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.994] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274912300600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.994] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=372296 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274912395100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.994] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae90000 drm_prime=0 software_frame=0
[2026-11-02 10:00:15.994] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=372296 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274912415300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4274912673900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=1245813 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274912749800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=372296 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274912904200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=1245813 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274913162000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=1245813 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274913143800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=34 first_video_sequence=1
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=1245813 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274913386600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=60157 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274913392100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.995] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0af20000 drm_prime=0 software_frame=0
[2026-11-02 10:00:15.996] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274913690300 group=composition-r374.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6009070 epoch_master_release_ns=671342168 video_count=16 min_presentation_ns=-11000000 max_end_ns=522333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.996] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=372296 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274913692600 group=composition-r374.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.996] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-11-02 10:00:15.996] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=60157 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274913908200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.996] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=60157 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274914001800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=60157 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274915280700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r374.source.2.input.video generation=1 steady_ns=4274914773100 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274914834400 group=composition-r374.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6009070 epoch_master_release_ns=513624300 video_count=16 min_presentation_ns=-11000000 max_end_ns=522333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=18 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.998] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=34
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274915045800 group=composition-r374.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=672703468 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=1245813 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274915108900 group=composition-r374.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.996] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r374.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable pts=1116889386 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274914309500 group=composition-r374.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6009070 epoch_master_release_ns=671342168 video_count=16 min_presentation_ns=-11000000 max_end_ns=522333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-11-02 10:00:15.997] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-02 10:00:15.998] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274916009800 group=composition-r374.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=672703468 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.998] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r374.source.0.input.video generation=1 steady_ns=4274916316500 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:00:15.998] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r374.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=3737440111 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274916383900 group=composition-r374.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=510434700 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=18 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.999] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:00:15.999] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=60157 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4274916871000 group=composition-r374.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:15.999] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-02 10:00:15.999] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274916754800 group=composition-r374.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=674445068 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:16.000] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=694562122 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:00:16.000] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274918074100 group=composition-r374.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=509351300 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=18 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:16.000] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r374.source.1.input.video generation=1 steady_ns=4274918081400 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:00:16.000] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r374.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable pts=180472488 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4274918074000 group=composition-r374.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-5000000 epoch_master_release_ns=674445068 video_count=16 min_presentation_ns=-5000000 max_end_ns=528333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:00:16.000] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2 source_master_ns=717782077 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:00:16.000] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:00:16.001] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-02 10:00:16.001] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=281300 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:00:16.001] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=32300 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-02 10:00:16.001] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=599900 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:00:16.002] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=64000 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-02 10:00:16.002] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=193800 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-02 10:00:16.002] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-02 10:00:16.002] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r374.source.0.input.video generation=1 steady_ns=4274920436100 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:00:16.002] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r374.source.1.input.video generation=1 steady_ns=4274920526100 source_sequence=1 presentation_ns=-5000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:00:16.002] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-02 10:00:16.003] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-02 10:00:16.003] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r374.source.2.input.video generation=1 steady_ns=4274920682400 source_sequence=1 presentation_ns=-11000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:00:16.003] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-02 10:00:16.008] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-02 10:00:16.008] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=259 side_data_elems=1 received_footprint=267 side_data[0]=Quality stats:8
[2026-11-02 10:00:16.008] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=267 wrapped_size=259 wrapped_side_data_elems=1
[2026-11-02 10:00:16.008] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=676251168 dispatch_master_ns=676251168 now_ns=517721200 node=74 identity_kind=output identity="composition-r374.output" canonical_sequence=1 canonical_created_steady_ns=4274926637700 head_observed_steady_ns=4274926665800 codec_receive_begin_steady_ns=4274926471200 codec_receive_end_steady_ns=4274926471800
[2026-11-02 10:00:16.009] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=517835800 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=676251168 present_dispatch_master_ns=676251168
[2026-11-02 10:00:16.048] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-02 10:00:16.048] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=676251168 dispatch_master_ns=676251168 now_ns=557465100 node=74 identity_kind=output identity="composition-r374.output" canonical_sequence=1 canonical_created_steady_ns=4274966388900 head_observed_steady_ns=4274966409700 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 10:00:16.212] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-02 10:00:16.605] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=47 source_master_ns=1762680036 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:00:17.626] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=91 source_master_ns=2784358040 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:00:18.648] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=135 source_master_ns=3806036045 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:00:19.693] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=4850934004 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:00:20.509] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573620 ntp_fraction=2181843386 arrival_ns=4279426786100
[2026-11-02 10:00:20.509] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:20.509] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:20.510] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573620 ntp_fraction=2190433320 arrival_ns=4279428098000
[2026-11-02 10:00:20.511] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:20.511] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:20.511] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573620 ntp_fraction=2190433320 arrival_ns=4279428653300
[2026-11-02 10:00:20.511] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:20.511] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:20.511] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573620 ntp_fraction=2194728288 arrival_ns=4279429475700
[2026-11-02 10:00:20.512] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:20.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:20.715] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=224 source_master_ns=5872612008 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:00:21.736] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=268 source_master_ns=6894290013 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:00:22.758] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=312 source_master_ns=7915968018 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:00:23.803] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=357 source_master_ns=8960865977 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:00:24.824] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=401 source_master_ns=9982543981 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:00:25.511] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573625 ntp_fraction=2194728288 arrival_ns=4284429373100
[2026-11-02 10:00:25.511] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:25.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:25.511] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573625 ntp_fraction=2194728288 arrival_ns=4284429486900
[2026-11-02 10:00:25.512] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:25.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:25.526] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573625 ntp_fraction=2259152797 arrival_ns=4284443889600
[2026-11-02 10:00:25.526] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:25.526] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:25.540] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573625 ntp_fraction=2319282339 arrival_ns=4284458450100
[2026-11-02 10:00:25.541] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:25.541] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:25.846] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=445 source_master_ns=11004221986 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:00:26.891] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=12049119945 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:00:27.913] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=534 source_master_ns=13070797950 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:00:28.934] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=578 source_master_ns=14092475954 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:00:29.956] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=622 source_master_ns=15114153959 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:00:30.514] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573630 ntp_fraction=2207613190 arrival_ns=4289432247500
[2026-11-02 10:00:30.515] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:30.515] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:30.514] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573630 ntp_fraction=2207613190 arrival_ns=4289432267900
[2026-11-02 10:00:30.515] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:30.515] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:30.545] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573630 ntp_fraction=2336462209 arrival_ns=4289462336200
[2026-11-02 10:00:30.545] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:30.545] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:30.573] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573630 ntp_fraction=2461016260 arrival_ns=4289491248200
[2026-11-02 10:00:30.574] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:30.574] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:31.001] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=667 source_master_ns=16159051918 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:00:32.023] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=711 source_master_ns=17180729922 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:00:33.044] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=755 source_master_ns=18202407927 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:00:34.089] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=19247305886 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:00:35.111] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=844 source_master_ns=20268983891 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:00:35.543] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573635 ntp_fraction=2332167241 arrival_ns=4294461535800
[2026-11-02 10:00:35.544] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:35.544] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:35.544] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573635 ntp_fraction=2336462209 arrival_ns=4294461930600
[2026-11-02 10:00:35.544] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:35.545] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:35.562] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573635 ntp_fraction=2413771620 arrival_ns=4294480025700
[2026-11-02 10:00:35.562] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:35.563] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:35.577] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573635 ntp_fraction=2478196129 arrival_ns=4294494863300
[2026-11-02 10:00:35.577] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:35.578] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:36.132] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=888 source_master_ns=21290661895 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:00:37.154] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=932 source_master_ns=22312339900 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:00:38.199] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=977 source_master_ns=23357237859 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:00:39.221] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1021 source_master_ns=24378915863 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:00:40.242] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1065 source_master_ns=25400593868 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:00:40.552] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573640 ntp_fraction=2370821947 arrival_ns=4299470052000
[2026-11-02 10:00:40.552] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:40.552] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:40.568] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573640 ntp_fraction=2439541424 arrival_ns=4299485979000
[2026-11-02 10:00:40.568] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:40.568] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:40.583] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573640 ntp_fraction=2503965933 arrival_ns=4299500807100
[2026-11-02 10:00:40.583] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:40.583] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:40.599] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573640 ntp_fraction=2572685410 arrival_ns=4299517081600
[2026-11-02 10:00:40.599] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:40.600] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:41.288] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=26445491827 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:00:42.309] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1154 source_master_ns=27467169832 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:00:43.331] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1198 source_master_ns=28488847836 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:00:44.353] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1242 source_master_ns=29510525841 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:00:45.397] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1287 source_master_ns=30555423800 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:00:45.578] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573645 ntp_fraction=2482491097 arrival_ns=4304495842900
[2026-11-02 10:00:45.578] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:45.578] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573645 ntp_fraction=2482491097 arrival_ns=4304495804000
[2026-11-02 10:00:45.578] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:45.578] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:45.579] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:45.608] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573645 ntp_fraction=2611340115 arrival_ns=4304526169300
[2026-11-02 10:00:45.608] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573645 ntp_fraction=2611340115 arrival_ns=4304526433100
[2026-11-02 10:00:45.609] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:45.609] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:45.609] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:45.610] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:46.419] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1331 source_master_ns=31577101804 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:00:47.441] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1375 source_master_ns=32598779809 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:00:48.486] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=33643677768 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:00:49.507] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1464 source_master_ns=34665355773 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:00:50.529] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1508 source_master_ns=35687033777 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:00:50.605] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573650 ntp_fraction=2598455214 arrival_ns=4309523224900
[2026-11-02 10:00:50.605] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:50.606] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:50.605] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573650 ntp_fraction=2598455214 arrival_ns=4309523352500
[2026-11-02 10:00:50.606] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:50.607] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:50.634] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573650 ntp_fraction=2723009265 arrival_ns=4309551958900
[2026-11-02 10:00:50.634] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:50.635] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:50.649] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573650 ntp_fraction=2783138807 arrival_ns=4309566972400
[2026-11-02 10:00:50.649] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:50.649] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:51.551] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1552 source_master_ns=36708711782 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:00:52.595] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1597 source_master_ns=37753609741 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:00:53.617] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1641 source_master_ns=38775287745 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:00:54.639] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1685 source_master_ns=39796965750 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:00:55.620] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573655 ntp_fraction=2662879723 arrival_ns=4314538409700
[2026-11-02 10:00:55.621] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:00:55.621] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:55.636] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573655 ntp_fraction=2731599200 arrival_ns=4314553954400
[2026-11-02 10:00:55.636] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:00:55.637] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:55.651] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573655 ntp_fraction=2796023709 arrival_ns=4314569112000
[2026-11-02 10:00:55.652] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:00:55.652] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:55.652] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573655 ntp_fraction=2796023709 arrival_ns=4314568943500
[2026-11-02 10:00:55.652] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:00:55.652] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:00:55.684] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=40841863709 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 10:00:56.706] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1774 source_master_ns=41863541714 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:00:57.727] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1818 source_master_ns=42885219718 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:00:58.749] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1862 source_master_ns=43906897723 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:00:59.794] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1907 source_master_ns=44951795682 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:01:00.624] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573660 ntp_fraction=2680059592 arrival_ns=4319542563500
[2026-11-02 10:01:00.625] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:00.625] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:00.638] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573660 ntp_fraction=2740189134 arrival_ns=4319556391300
[2026-11-02 10:01:00.639] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:00.639] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:00.683] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573660 ntp_fraction=2929167695 arrival_ns=4319600722000
[2026-11-02 10:01:00.683] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:00.683] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573660 ntp_fraction=2933462663 arrival_ns=4319600867300
[2026-11-02 10:01:00.683] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:00.683] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:00.683] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:00.815] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1951 source_master_ns=45973473686 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:01:01.837] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1995 source_master_ns=46995151691 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:01:02.882] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=48040049650 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:01:03.904] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2084 source_master_ns=49061727655 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:01:04.925] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2128 source_master_ns=50083405659 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:01:05.628] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573665 ntp_fraction=2697239461 arrival_ns=4324546568000
[2026-11-02 10:01:05.629] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:05.629] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:05.676] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573665 ntp_fraction=2899102924 arrival_ns=4324593774500
[2026-11-02 10:01:05.676] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:05.676] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:05.704] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573665 ntp_fraction=3023656976 arrival_ns=4324622156100
[2026-11-02 10:01:05.704] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573665 ntp_fraction=3023656976 arrival_ns=4324622238700
[2026-11-02 10:01:05.705] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:05.705] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:05.705] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:05.706] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:05.947] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2172 source_master_ns=51105083664 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:01:06.992] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2217 source_master_ns=52149981623 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:01:08.014] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2261 source_master_ns=53171659628 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:01:09.036] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2305 source_master_ns=54193337632 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:01:10.080] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=55238235591 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:01:10.644] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573670 ntp_fraction=2761663971 arrival_ns=4329561772000
[2026-11-02 10:01:10.644] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:10.644] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:10.703] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573670 ntp_fraction=3019362009 arrival_ns=4329621253500
[2026-11-02 10:01:10.704] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:10.704] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:10.718] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573670 ntp_fraction=3083786518 arrival_ns=4329636071400
[2026-11-02 10:01:10.718] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:10.718] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573670 ntp_fraction=3083786518 arrival_ns=4329636160600
[2026-11-02 10:01:10.719] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:10.719] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:10.719] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:11.102] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2394 source_master_ns=56259913596 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:01:12.124] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2438 source_master_ns=57281591600 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:01:13.146] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2482 source_master_ns=58303269605 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:01:14.190] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=59348167564 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:01:15.212] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2571 source_master_ns=60369845569 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:01:15.652] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573675 ntp_fraction=2800318676 arrival_ns=4334570431000
[2026-11-02 10:01:15.653] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:15.653] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:15.723] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573675 ntp_fraction=3105261355 arrival_ns=4334641329700
[2026-11-02 10:01:15.724] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:15.723] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573675 ntp_fraction=3105261355 arrival_ns=4334641203200
[2026-11-02 10:01:15.724] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:15.724] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:15.724] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:15.738] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573675 ntp_fraction=3169685864 arrival_ns=4334656010400
[2026-11-02 10:01:15.738] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:15.738] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:16.235] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2615 source_master_ns=61391523573 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:01:17.258] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2659 source_master_ns=62413201578 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:01:18.300] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2704 source_master_ns=63458099537 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:01:19.322] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2748 source_master_ns=64479777541 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:01:20.344] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2792 source_master_ns=65501455546 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:01:20.675] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573680 ntp_fraction=2899102924 arrival_ns=4339593186600
[2026-11-02 10:01:20.675] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:20.675] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:20.748] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573680 ntp_fraction=3212635537 arrival_ns=4339666468800
[2026-11-02 10:01:20.749] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:20.749] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:20.749] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573680 ntp_fraction=3216930504 arrival_ns=4339667095300
[2026-11-02 10:01:20.750] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:20.750] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:20.764] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573680 ntp_fraction=3281355014 arrival_ns=4339682473500
[2026-11-02 10:01:20.765] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:20.765] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:21.389] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=66546353505 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:01:22.410] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2881 source_master_ns=67568031510 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:01:23.431] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2925 source_master_ns=68589709514 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:01:24.453] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2969 source_master_ns=69611387519 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:01:25.499] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3014 source_master_ns=70656285478 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:01:25.683] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573685 ntp_fraction=2933462663 arrival_ns=4344601510800
[2026-11-02 10:01:25.684] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:25.684] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:25.790] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573685 ntp_fraction=3388729196 arrival_ns=4344707771900
[2026-11-02 10:01:25.790] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:25.791] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573685 ntp_fraction=3393024163 arrival_ns=4344708508700
[2026-11-02 10:01:25.791] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:25.791] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:25.791] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573685 ntp_fraction=3397319131 arrival_ns=4344709160500
[2026-11-02 10:01:25.791] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:25.792] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:25.792] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:26.520] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3058 source_master_ns=71677963482 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:01:27.541] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3102 source_master_ns=72699641487 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:01:28.587] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=73744539446 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:01:29.608] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3191 source_master_ns=74766217451 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:01:30.630] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3235 source_master_ns=75787895455 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:01:30.704] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573690 ntp_fraction=3019362009 arrival_ns=4349621627800
[2026-11-02 10:01:30.704] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:30.704] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:30.818] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573690 ntp_fraction=3513283248 arrival_ns=4349735965900
[2026-11-02 10:01:30.818] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573690 ntp_fraction=3513283248 arrival_ns=4349736005900
[2026-11-02 10:01:30.818] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:30.818] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573690 ntp_fraction=3513283248 arrival_ns=4349736445700
[2026-11-02 10:01:30.820] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:30.820] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:30.819] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:30.820] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:30.819] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:31.651] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3279 source_master_ns=76809573460 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:01:32.696] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3324 source_master_ns=77854471419 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:01:33.719] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3368 source_master_ns=78876149423 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:01:34.740] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3412 source_master_ns=79897827428 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:01:35.724] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573695 ntp_fraction=3109556322 arrival_ns=4354641994500
[2026-11-02 10:01:35.724] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:35.725] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:35.785] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=80942725387 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:01:35.833] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573695 ntp_fraction=3577707757 arrival_ns=4354750961200
[2026-11-02 10:01:35.833] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:35.833] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:35.847] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573695 ntp_fraction=3637837299 arrival_ns=4354765065700
[2026-11-02 10:01:35.847] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573695 ntp_fraction=3637837299 arrival_ns=4354765064200
[2026-11-02 10:01:35.847] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:35.847] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:35.848] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:35.848] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:36.808] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3501 source_master_ns=81964403392 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:01:37.828] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3545 source_master_ns=82986081396 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:01:38.850] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3589 source_master_ns=84007759401 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:01:39.895] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3634 source_master_ns=85052657360 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:01:40.729] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573700 ntp_fraction=3126736191 arrival_ns=4359646734700
[2026-11-02 10:01:40.729] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:40.730] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:40.835] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573700 ntp_fraction=3586297692 arrival_ns=4359752881600
[2026-11-02 10:01:40.835] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:40.835] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:40.849] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573700 ntp_fraction=3646427234 arrival_ns=4359767466400
[2026-11-02 10:01:40.850] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:40.849] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573700 ntp_fraction=3646427234 arrival_ns=4359767467000
[2026-11-02 10:01:40.850] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:40.850] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:40.850] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:40.916] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3678 source_master_ns=86074335364 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:01:41.938] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3722 source_master_ns=87096013369 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:01:42.983] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=88140911328 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:01:44.005] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3811 source_master_ns=89162589333 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:01:45.026] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3855 source_master_ns=90184267337 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:01:45.745] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573705 ntp_fraction=3199750635 arrival_ns=4364663114800
[2026-11-02 10:01:45.745] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:45.746] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:45.875] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573705 ntp_fraction=3758096384 arrival_ns=4364792953500
[2026-11-02 10:01:45.875] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573705 ntp_fraction=3758096384 arrival_ns=4364793312800
[2026-11-02 10:01:45.876] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:45.876] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:45.875] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573705 ntp_fraction=3758096384 arrival_ns=4364793384600
[2026-11-02 10:01:45.876] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:45.875] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:45.877] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:45.877] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:46.048] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3899 source_master_ns=91205945342 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:01:47.093] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3944 source_master_ns=92250843301 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:01:48.120] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3988 source_master_ns=93272521306 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:01:49.137] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4032 source_master_ns=94294199310 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:01:50.181] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=95339097269 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:01:50.756] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573710 ntp_fraction=3246995275 arrival_ns=4369673964800
[2026-11-02 10:01:50.756] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:50.756] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:50.901] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573710 ntp_fraction=3869765533 arrival_ns=4369819307900
[2026-11-02 10:01:50.902] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:50.902] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:50.917] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573710 ntp_fraction=3934190043 arrival_ns=4369834649800
[2026-11-02 10:01:50.917] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:50.917] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573710 ntp_fraction=3934190043 arrival_ns=4369834584300
[2026-11-02 10:01:50.917] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:50.917] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:50.918] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:51.203] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4121 source_master_ns=96360775274 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 10:01:52.225] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4165 source_master_ns=97382453278 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 10:01:53.246] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4209 source_master_ns=98404131283 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 10:01:54.291] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4254 source_master_ns=99449029242 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 10:01:55.312] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4298 source_master_ns=100470707247 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 10:01:55.774] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573715 ntp_fraction=3324304687 arrival_ns=4374691943700
[2026-11-02 10:01:55.774] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:01:55.774] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:55.907] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573715 ntp_fraction=3891240370 arrival_ns=4374824693000
[2026-11-02 10:01:55.907] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:01:55.908] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:55.943] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573715 ntp_fraction=4050154160 arrival_ns=4374861224100
[2026-11-02 10:01:55.943] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:01:55.943] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:55.952] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573715 ntp_fraction=4088808865 arrival_ns=4374870240200
[2026-11-02 10:01:55.952] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:01:55.952] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:01:56.334] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4342 source_master_ns=101492385251 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 10:01:57.379] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=102537283210 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 10:01:58.401] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4431 source_master_ns=103558961215 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 10:01:59.423] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4475 source_master_ns=104580639219 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 10:02:00.444] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4519 source_master_ns=105602317224 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 10:02:00.780] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573720 ntp_fraction=3345779523 arrival_ns=4379697485200
[2026-11-02 10:02:00.780] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:02:00.780] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:00.913] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573720 ntp_fraction=3921305141 arrival_ns=4379831036000
[2026-11-02 10:02:00.913] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:02:00.914] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:00.946] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573720 ntp_fraction=4063039062 arrival_ns=4379864293800
[2026-11-02 10:02:00.946] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:02:00.947] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:00.987] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573720 ntp_fraction=4239132721 arrival_ns=4379904795400
[2026-11-02 10:02:00.987] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:02:00.987] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:01.489] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4564 source_master_ns=106647215183 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 10:02:02.511] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4608 source_master_ns=107668893188 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 10:02:03.532] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4652 source_master_ns=108690571192 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 10:02:04.579] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=109735469151 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 10:02:05.599] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4741 source_master_ns=110757147156 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 10:02:05.792] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573725 ntp_fraction=3397319131 arrival_ns=4384709578200
[2026-11-02 10:02:05.792] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:02:05.792] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:05.934] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573725 ntp_fraction=4011499454 arrival_ns=4384852255400
[2026-11-02 10:02:05.935] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:02:05.935] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:05.949] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573725 ntp_fraction=4075923963 arrival_ns=4384867367900
[2026-11-02 10:02:05.950] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:02:05.950] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:06.011] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573726 ntp_fraction=47244640 arrival_ns=4384928899900
[2026-11-02 10:02:06.011] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:02:06.011] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:06.621] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4785 source_master_ns=111778825160 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 10:02:07.642] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4829 source_master_ns=112800503165 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 10:02:08.687] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4874 source_master_ns=113845401124 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 10:02:09.709] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4918 source_master_ns=114867079129 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 10:02:10.731] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4962 source_master_ns=115888757133 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-02 10:02:10.806] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3632087500 ntp_seconds=4002573730 ntp_fraction=3461743640 arrival_ns=4389724011900
[2026-11-02 10:02:10.806] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3632087500
[2026-11-02 10:02:10.807] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:10.941] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=773014159 ntp_seconds=4002573730 ntp_fraction=4037269258 arrival_ns=4389858770600
[2026-11-02 10:02:10.942] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=773014159
[2026-11-02 10:02:10.942] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:10.972] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=622833289 ntp_seconds=4002573730 ntp_fraction=4170413244 arrival_ns=4389889546400
[2026-11-02 10:02:10.972] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=622833289
[2026-11-02 10:02:10.972] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:11.016] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=766346614 ntp_seconds=4002573731 ntp_fraction=64424509 arrival_ns=4389933669100
[2026-11-02 10:02:11.016] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=766346614
[2026-11-02 10:02:11.017] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:02:11.775] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5007 source_master_ns=116933655092 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-02 10:02:12.797] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5051 source_master_ns=117955333097 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-02 10:02:13.819] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5095 source_master_ns=118977011101 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-11-02 10:02:14.840] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5139 source_master_ns=119998689106 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5305482
[2026-11-02 10:02:14.973] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=4393890984000
[2026-11-02 10:02:14.973] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:02:14.973] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=4393891200700
[2026-11-02 10:02:14.974] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:02:14.973] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.0 phase=purging old=1 next=2 transition=1
[2026-11-02 10:02:14.973] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r374.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=24 video_raw_generation=0 video_ssrc=766346614
[2026-11-02 10:02:14.974] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r374.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=24 video_raw_generation=0 video_ssrc=622833289
[2026-11-02 10:02:14.974] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.1 phase=purging old=1 next=2 transition=1
[2026-11-02 10:02:14.978] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-02 10:02:14.979] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3625
[2026-11-02 10:02:14.980] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3625
[2026-11-02 10:02:14.980] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-02 10:02:14.981] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:02:14.982] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:02:14.989] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=4393906813000
[2026-11-02 10:02:14.989] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:02:14.989] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r374.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=48 video_raw_generation=0 video_ssrc=773014159 audio_raw_generation=0 audio_ssrc=3632087500
[2026-11-02 10:02:14.989] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r374.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=48 video_raw_generation=0 video_ssrc=773014159 audio_raw_generation=0 audio_ssrc=3632087500
[2026-11-02 10:02:14.989] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.2 phase=purging old=1 next=2 transition=1
[2026-11-02 10:02:14.990] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-02 10:02:14.990] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3625
[2026-11-02 10:02:14.990] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-02 10:02:14.990] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5193
[2026-11-02 10:02:14.991] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-02 10:02:14.990] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=4393907894200
[2026-11-02 10:02:14.991] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:02:24.988] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=129497136000 present_generation=1 present_canonical_target_ns=128940408163 present_canonical_dispatch_ns=128940408163 present_target_master_ns=129616659331 present_dispatch_master_ns=129616659331
[2026-11-02 10:02:24.989] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=129498580800 present_generation=1 present_canonical_target_ns=128966666666 present_canonical_dispatch_ns=128966666666 present_target_master_ns=129642917834 present_dispatch_master_ns=129642917834
[2026-11-02 10:02:50.182] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=154691072700 present_generation=1 present_canonical_target_ns=154134058956 present_canonical_dispatch_ns=154134058956 present_target_master_ns=154810310124 present_dispatch_master_ns=154810310124
[2026-11-02 10:02:50.186] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=154694975100 present_generation=1 present_canonical_target_ns=154166666666 present_canonical_dispatch_ns=154166666666 present_target_master_ns=154842917834 present_dispatch_master_ns=154842917834
[2026-11-02 10:02:50.252] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=154760764400 present_generation=1 present_canonical_target_ns=154203718820 present_canonical_dispatch_ns=154203718820 present_target_master_ns=154879969988 present_dispatch_master_ns=154879969988
[2026-11-02 10:02:50.252] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=154761426500 present_generation=1 present_canonical_target_ns=154233333333 present_canonical_dispatch_ns=154233333333 present_target_master_ns=154909584501 present_dispatch_master_ns=154909584501
[2026-11-02 10:03:04.884] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573784 ntp_fraction=3792456122 arrival_ns=4443801807400
[2026-11-02 10:03:04.884] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:04.885] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:03:04.884] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r374.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=2785627171 raw_generation=1 raw_sequence=5195 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-02 10:03:04.885] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573784 ntp_fraction=3792456122 arrival_ns=4443801789000
[2026-11-02 10:03:04.885] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:04.886] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:05.424] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-02 10:03:05.927] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=4444845400800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3584 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.928] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4444845848000 group=composition-r374.source.2 release_identity=15887 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=170536029600 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.928] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4444846196600 group=composition-r374.source.2 release_identity=15887 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=170536029600 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.928] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=4444846233100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=3601 release_committed_count=3584 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.929] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4444847113300 group=composition-r374.source.2 release_identity=15887 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=170536029600 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.929] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4444847492900 group=composition-r374.source.2 release_identity=15887 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=170536029600 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.929] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=4444847539000 input_count=3599 accepted_count=3599 would_block_count=0 resolved_count=3599 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.930] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=170537496720 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:03:05.930] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4444848141800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.930] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=170583936629 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:03:05.936] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=4444854297000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.936] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4444854615800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.937] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4444855066000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.937] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=4444855108100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.937] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:03:05.937] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4444855574800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.938] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=4444855795000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3597 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.938] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=4444855863900 input_count=3591 accepted_count=3591 would_block_count=0 resolved_count=3591 output_attempt_count=0 published_count=3590 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.938] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4444856304200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:03:05.938] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4444856489000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.939] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=4138362325 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=4444856895800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.939] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1379454 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4444857358200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.939] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1379454 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4444857655000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.939] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=4444857678400 input_count=3584 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3584 published_count=3583 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.940] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1379454 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4444858063900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.940] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1379454 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4444858294000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.940] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r374.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1379454 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=4444858544900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:03:05.945] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r374.source.2.input.video generation=2 steady_ns=4444863440300 source_sequence=4 presentation_ns=1093000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:03:05.968] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r374.source.2.input.video generation=2 steady_ns=4444886083700 source_sequence=4 presentation_ns=1093000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:03:06.482] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=171628834588 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:03:07.503] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=172650512593 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:03:08.525] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=173672190597 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:03:09.570] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=174717088556 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:03:09.887] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573789 ntp_fraction=3809635991 arrival_ns=4448804866600
[2026-11-02 10:03:09.887] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:09.887] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:09.887] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573789 ntp_fraction=3809635991 arrival_ns=4448805039400
[2026-11-02 10:03:09.888] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:09.888] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:10.592] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=175738766561 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:03:11.613] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=176760444565 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:03:12.635] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=177782122570 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:03:13.680] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=178827020529 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:03:14.701] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=179848698534 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:03:14.890] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573794 ntp_fraction=3818225926 arrival_ns=4453807779500
[2026-11-02 10:03:14.890] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:14.890] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:14.891] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573794 ntp_fraction=3822520893 arrival_ns=4453807904700
[2026-11-02 10:03:14.891] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:14.891] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:15.723] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=180870376538 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:03:16.768] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=181915274497 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:03:17.790] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=182936952502 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:03:18.812] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=183958630507 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:03:19.833] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=184980308511 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:03:19.914] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573799 ntp_fraction=3925600108 arrival_ns=4458832506900
[2026-11-02 10:03:19.915] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:19.915] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:19.929] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573799 ntp_fraction=3990024617 arrival_ns=4458846976400
[2026-11-02 10:03:19.929] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:19.929] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:20.878] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=186025206470 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:03:21.900] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=187046884475 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:03:22.922] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=188068562479 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:03:23.966] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=189113460439 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:03:24.915] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573804 ntp_fraction=3929895075 arrival_ns=4463833119400
[2026-11-02 10:03:24.916] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:24.916] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:24.940] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573804 ntp_fraction=4037269258 arrival_ns=4463857883400
[2026-11-02 10:03:24.940] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:24.941] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:24.988] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=190135138443 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:03:26.010] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=191156816448 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:03:27.032] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=192178494452 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:03:28.077] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=193223392411 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:03:29.098] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=194245070416 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:03:29.939] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573809 ntp_fraction=4032974290 arrival_ns=4468857121900
[2026-11-02 10:03:29.940] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:29.940] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:29.955] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573809 ntp_fraction=4097398800 arrival_ns=4468872581700
[2026-11-02 10:03:29.955] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:29.955] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:30.120] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=195266748420 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:03:31.165] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=196311646380 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:03:32.186] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=197333324384 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:03:33.208] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=198355002389 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:03:34.229] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=199376680393 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:03:34.953] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573814 ntp_fraction=4093103833 arrival_ns=4473871397600
[2026-11-02 10:03:34.954] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:34.954] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:34.983] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573814 ntp_fraction=4221952851 arrival_ns=4473900961700
[2026-11-02 10:03:34.983] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:34.983] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:35.274] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=200421578352 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:03:36.296] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=201443256357 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:03:37.318] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=202464934361 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:03:38.340] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1420 source_master_ns=203486612366 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:03:39.384] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=204531510325 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:03:39.957] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573819 ntp_fraction=4110283702 arrival_ns=4478875118900
[2026-11-02 10:03:39.957] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:39.958] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:39.987] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573819 ntp_fraction=4239132721 arrival_ns=4478905409600
[2026-11-02 10:03:39.988] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:39.988] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:40.406] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=205553188330 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:03:41.428] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=206574866334 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:03:42.473] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=207619764293 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:03:43.494] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=208641442298 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:03:44.516] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=209663120302 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:03:44.978] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573824 ntp_fraction=4200478015 arrival_ns=4483896216500
[2026-11-02 10:03:44.978] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:44.979] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:44.993] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573824 ntp_fraction=4264902524 arrival_ns=4483911151000
[2026-11-02 10:03:44.994] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:44.994] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:45.538] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1730 source_master_ns=210684798307 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 10:03:46.582] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=211729696266 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:03:47.604] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=212751374271 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:03:48.626] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=213773052275 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:03:49.671] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=214817950234 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:03:49.994] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573829 ntp_fraction=4269197492 arrival_ns=4488911810200
[2026-11-02 10:03:49.994] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:49.995] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:50.037] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573830 ntp_fraction=158913789 arrival_ns=4488955491400
[2026-11-02 10:03:50.038] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:50.038] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:50.692] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=215839628239 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:03:51.714] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=216861306243 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:03:52.736] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2040 source_master_ns=217882984248 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:03:53.781] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=218927882207 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:03:54.802] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=219949560212 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:03:55.012] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573835 ntp_fraction=51539607 arrival_ns=4493930500700
[2026-11-02 10:03:55.013] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:03:55.013] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:55.067] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573835 ntp_fraction=287762808 arrival_ns=4493985142100
[2026-11-02 10:03:55.068] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:03:55.068] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:03:55.824] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=220971238216 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:03:56.869] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=222016136175 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:03:57.891] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=223037814180 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:03:58.912] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=224059492185 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:03:59.934] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=225081170189 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:04:00.019] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573840 ntp_fraction=81604378 arrival_ns=4498937269700
[2026-11-02 10:04:00.020] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:00.020] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:00.094] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573840 ntp_fraction=399431958 arrival_ns=4499011640800
[2026-11-02 10:04:00.094] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:00.095] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:00.979] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=226126068148 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:04:02.001] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2439 source_master_ns=227147746153 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:04:03.022] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=228169424157 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:04:04.067] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=229214322117 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:04:05.044] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573845 ntp_fraction=188978561 arrival_ns=4503961856700
[2026-11-02 10:04:05.044] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:05.044] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:05.089] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=230236000121 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:04:05.119] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573845 ntp_fraction=511101108 arrival_ns=4504037242100
[2026-11-02 10:04:05.120] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:05.120] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:06.110] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=231257678126 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:04:07.132] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=232279356130 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:04:08.177] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=233324254089 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:04:09.199] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=234345932094 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:04:10.054] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573850 ntp_fraction=231928233 arrival_ns=4508972312900
[2026-11-02 10:04:10.055] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:10.055] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:10.125] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573850 ntp_fraction=536870912 arrival_ns=4509043347500
[2026-11-02 10:04:10.126] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:10.126] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:10.220] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=235367610098 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:04:11.266] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=236412508058 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:04:12.287] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=237434186062 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:04:13.309] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=238455864067 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:04:14.330] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=239477542071 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:04:15.056] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573855 ntp_fraction=236223201 arrival_ns=4513973857000
[2026-11-02 10:04:15.056] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:15.056] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:15.129] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573855 ntp_fraction=554050781 arrival_ns=4514047209600
[2026-11-02 10:04:15.130] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:15.130] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:15.376] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=240522440030 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:04:16.397] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=241544118035 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:04:17.419] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=242565796039 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:04:18.464] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=243610693999 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:04:19.485] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=244632372003 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:04:20.069] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573860 ntp_fraction=296352743 arrival_ns=4518987340500
[2026-11-02 10:04:20.070] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:20.070] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:20.158] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573860 ntp_fraction=674309865 arrival_ns=4519075664100
[2026-11-02 10:04:20.158] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:20.158] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:20.508] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=245654050008 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:04:21.529] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=246675728012 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:04:22.573] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=247720625971 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:04:23.595] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=248742303976 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:04:24.617] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=249763981980 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:04:25.082] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573865 ntp_fraction=347892350 arrival_ns=4523999829800
[2026-11-02 10:04:25.082] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:25.082] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:25.195] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573865 ntp_fraction=837518622 arrival_ns=4524113267300
[2026-11-02 10:04:25.196] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:25.196] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:25.662] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3458 source_master_ns=250808879940 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:04:26.683] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=251830557944 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:04:27.705] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=252852235949 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:04:28.727] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=253873913953 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:04:29.772] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=254918811912 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:04:30.088] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573870 ntp_fraction=377957122 arrival_ns=4529006000800
[2026-11-02 10:04:30.088] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:30.089] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:30.198] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573870 ntp_fraction=850403524 arrival_ns=4529116412800
[2026-11-02 10:04:30.199] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:30.199] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:30.794] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=255940489917 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:04:31.815] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=256962167921 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:04:32.860] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3768 source_master_ns=258007065881 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:04:33.882] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=259028743885 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:04:34.903] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=260050421890 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:04:35.114] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573875 ntp_fraction=485331304 arrival_ns=4534031725400
[2026-11-02 10:04:35.114] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:35.114] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:35.229] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573875 ntp_fraction=983547510 arrival_ns=4534147043100
[2026-11-02 10:04:35.229] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:35.230] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:35.925] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=261072099894 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:04:36.970] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=262116997853 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:04:37.991] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=263138675858 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:04:39.013] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=264160353863 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:04:40.035] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4077 source_master_ns=265182031867 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:04:40.125] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573880 ntp_fraction=536870912 arrival_ns=4539043414500
[2026-11-02 10:04:40.126] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:40.126] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:40.231] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573880 ntp_fraction=987842478 arrival_ns=4539148593600
[2026-11-02 10:04:40.231] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:40.232] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:41.080] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4122 source_master_ns=266226929826 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 10:04:42.102] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4166 source_master_ns=267248607831 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 10:04:43.123] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4210 source_master_ns=268270285835 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 10:04:44.169] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4255 source_master_ns=269315183795 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 10:04:45.140] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573885 ntp_fraction=601295421 arrival_ns=4544057780200
[2026-11-02 10:04:45.140] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:45.140] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:45.190] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4299 source_master_ns=270336861799 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 10:04:45.273] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573885 ntp_fraction=1168231104 arrival_ns=4544190747500
[2026-11-02 10:04:45.273] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:45.273] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:46.212] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4343 source_master_ns=271358539804 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 10:04:47.233] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4387 source_master_ns=272380217808 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 10:04:48.278] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4432 source_master_ns=273425115767 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 10:04:49.300] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4476 source_master_ns=274446793772 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 10:04:50.150] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573890 ntp_fraction=644245094 arrival_ns=4549068201100
[2026-11-02 10:04:50.151] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:50.151] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:50.284] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573890 ntp_fraction=1215475744 arrival_ns=4549201666300
[2026-11-02 10:04:50.284] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:50.284] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:50.322] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4520 source_master_ns=275468471776 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 10:04:51.366] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4565 source_master_ns=276513369736 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 10:04:52.388] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4609 source_master_ns=277535047740 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 10:04:53.410] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4653 source_master_ns=278556725745 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 10:04:54.431] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4697 source_master_ns=279578403749 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 10:04:55.153] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573895 ntp_fraction=657129996 arrival_ns=4554071310100
[2026-11-02 10:04:55.154] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:04:55.154] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:55.304] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573895 ntp_fraction=1301375090 arrival_ns=4554221783400
[2026-11-02 10:04:55.304] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:04:55.304] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:04:55.476] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4742 source_master_ns=280623301708 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 10:04:56.498] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4786 source_master_ns=281644979713 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 10:04:57.520] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4830 source_master_ns=282666657717 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 10:04:58.565] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4875 source_master_ns=283711555677 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 10:04:59.586] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4919 source_master_ns=284733233681 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 10:05:00.168] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2785627171 ntp_seconds=4002573900 ntp_fraction=717259538 arrival_ns=4559085706400
[2026-11-02 10:05:00.168] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2785627171
[2026-11-02 10:05:00.168] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:05:00.317] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2099019385 ntp_seconds=4002573900 ntp_fraction=1361504632 arrival_ns=4559234942700
[2026-11-02 10:05:00.317] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2099019385
[2026-11-02 10:05:00.317] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:05:00.608] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=285754911686 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-02 10:05:01.630] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=286776589690 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-02 10:05:02.675] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=287821487649 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-02 10:05:03.696] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=288843165654 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-11-02 10:05:04.375] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=4563293024900
[2026-11-02 10:05:04.375] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r374.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=96 video_raw_generation=1 video_ssrc=2099019385 audio_raw_generation=1 audio_ssrc=2785627171
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r374.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=96 video_raw_generation=1 video_ssrc=2099019385 audio_raw_generation=1 audio_ssrc=2785627171
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.2 phase=purging old=2 next=3 transition=2
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=2 cutoff=0
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=2 cutoff=0
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=4563294246500
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=2 cutoff=7220
[2026-11-02 10:05:04.376] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=2 cutoff=10386
[2026-11-02 10:05:04.378] [info] [graph][runtime.node] av_reacquisition group=composition-r374.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-02 10:05:04.377] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-02 10:05:31.669] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-11-02 10:05:31.669] [info] [graph][runtime.lifecycle] worker.finished node=80
[2026-11-02 10:05:31.669] [info] [graph][runtime.lifecycle] worker.finished node=4
[2026-11-02 10:05:31.669] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=7031250000 process_calls=42594
[2026-11-02 10:05:31.669] [info] [graph][runtime.lifecycle] worker.finished node=77
[2026-11-02 10:05:31.669] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=7
[2026-11-02 10:05:31.669] [info] [graph][runtime.lifecycle] worker.finished node=5
[2026-11-02 10:05:31.669] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=8
[2026-11-02 10:05:31.670] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=421875000 process_calls=40792
[2026-11-02 10:05:31.672] [info] [graph][runtime.lifecycle] worker.finished node=2
[2026-11-02 10:05:31.673] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=4312500000 process_calls=28415
[2026-11-02 10:05:31.706] [info] [graph][runtime.lifecycle] worker.finished node=74
[2026-11-02 10:05:31.706] [info] [graph][runtime.lifecycle] worker.finished node=75
[2026-11-02 10:05:31.706] [info] [graph][runtime.lifecycle] worker.finished node=78
[2026-11-02 10:05:31.706] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=890625000 process_calls=69282
[2026-11-02 10:05:31.706] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=343750000 process_calls=68859
[2026-11-02 10:05:31.706] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=609375000 process_calls=68043
[2026-11-02 10:05:31.734] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=180506 produced_ts_payload_bytes=208533360 pending_bytes=0 peak_pending_bytes=92872 access_units=23066 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=33335175 maximum_mux_input_after_emit_ns=60760072 maximum_mux_input_after_dispatch_ns=-39239928 worst_mux_input_ready_ns=51140412600 worst_mux_input_emit_ns=51079652528 worst_mux_input_dispatch_ns=51179652528 maximum_protocol_batch_produced_after_release_ns=44035192 worst_protocol_batch_produced_ns=22620286800 worst_protocol_batch_release_ns=22576251608 worst_protocol_batch_deadline_ns=22642918276 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-11-02 10:05:31.734] [info] [graph][runtime.lifecycle] worker.finished node=76
[2026-11-02 10:05:31.734] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=3218750000 process_calls=139653
[2026-11-02 10:05:31.734] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=180506 last_sequence=180506 maximum_submit_interval_ns=90200 maximum_submit_interval_sequence=180506 maximum_start_lateness_ns=253666 maximum_start_lateness_sequence=180506 first_release_master_ns=316242917834 first_deadline_master_ns=316309556256 last_release_master_ns=316242917834 last_deadline_master_ns=316309556256 first_submitted_master_ns=316243261700 last_submitted_master_ns=316243261700 worst_wait_started_master_ns=316229188100 worst_wait_completed_master_ns=316243169200 worst_submit_observed_master_ns=316243171500 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=13977700 wait_call_max_ns=13977700 wait_before_call_count=1 wait_before_call_total_ns=100 wait_before_call_max_ns=100 wait_arm_count=1 wait_arm_total_ns=3100 wait_arm_max_ns=3100 wait_api_wall_count=1 wait_api_wall_total_ns=13972200 wait_api_wall_max_ns=13972200 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=3400 wait_return_to_next_clock_max_ns=3400 wait_target_lateness_count=1 wait_target_lateness_total_ns=251366 wait_target_lateness_max_ns=251366 worst_wait_metric=call_wall worst_wait_sequence=180506 worst_wait_target_master_ns=316242917834 worst_wait_sampled_master_ns=316229188100 worst_wait_sampled_steady_ns=4590638132600 worst_wait_timeout_ns=13729734 worst_wait_entered_steady_ns=4590638132700 worst_wait_returned_steady_ns=4590652110400 worst_wait_arm_before_steady_ns=4590638133100 worst_wait_arm_after_steady_ns=4590638136200 worst_wait_api_before_steady_ns=4590638136300 worst_wait_api_returned_steady_ns=4590652108500 worst_wait_windows_due_100ns=-137298 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=30900 worst_wait_windows_tid=34412 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=316243169200 worst_wait_next_steady_ns=4590652113800 worst_wait_outcome=deadline
[2026-11-02 10:05:31.734] [info] [graph][runtime.lifecycle] worker.finished node=79
[2026-11-02 10:05:31.735] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=34122 committed_datagrams=180506 committed_payload_bytes=208533360 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=180506 pacing_cancelled=0 pacing_submitted=180506 pacing_maximum_submit_lateness_ns=32527788 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=80070800 last_materialized_sequence=180506 last_scheduled_sequence=180506 last_submitted_sequence=180506 last_committed_sequence=180506 submitted_service_generation=1 submitted_service_sequence=180506 submitted_service_completed_ns=316243261700 submitted_service_duration_ns=34560 submitted_service_available_ns=316243296260 submitted_scope_available_ns=316243296260 shared_scope_charged_datagrams=180506 shared_scope_charged_wire_bytes=213587528 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=180506 endpoint_1_committed_payload_bytes=208533360
[2026-11-02 10:05:31.735] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=2875000000 process_calls=110976
[2026-11-02 10:05:31.735] [info] [graph][runtime.lifecycle] worker.finished node=81
[2026-11-02 10:05:31.735] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=12281250000 process_calls=68136
[2026-11-02 10:05:31.931] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-11-02 10:05:31.932] [info] [graph][runtime.lifecycle] stop.begin
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=78125000 process_calls=302
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=203125000 process_calls=10804
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=921875000 process_calls=229961
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=1046875000 process_calls=236645
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=110
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=62500000 process_calls=10805
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=108
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=84
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=82
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=156250000 process_calls=10810
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=265625000 process_calls=21604
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=494
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=84
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=489
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=0 process_calls=10851
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=310
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=83
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=328125000 process_calls=21780
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=187500000 process_calls=10819
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=375000000 process_calls=30958
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=609375000 process_calls=31400
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=250000000 process_calls=10898
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=156250000 process_calls=10890
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=218750000 process_calls=10836
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=609375000 process_calls=52066
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=15625000 process_calls=82
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=359375000 process_calls=31072
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=318
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=546875000 process_calls=52938
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=171875000 process_calls=10807
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=78125000 process_calls=10760
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=390625000 process_calls=32473
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=312500000 process_calls=21653
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=109375000 process_calls=10766
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=546875000 process_calls=52106
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=187500000 process_calls=10853
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=796875000 process_calls=52487
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=968750000 process_calls=11205
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=93750000 process_calls=10796
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=281250000 process_calls=21527
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=111
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=968750000 process_calls=14326
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=468750000 process_calls=31926
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1203125000 process_calls=14336
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=890625000 process_calls=51228
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=187500000 process_calls=10806
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1687500000 process_calls=26073
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=93750000 process_calls=10843
[2026-11-02 10:05:31.932] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=234375000 process_calls=102809
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=250000000 process_calls=11061
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1500000000 process_calls=53618
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=108
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=265625000 process_calls=90774
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=109375000 process_calls=10856
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=82
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=296875000 process_calls=21795
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=3375000000 process_calls=32267
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=78125000 process_calls=10815
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=1015625000 process_calls=43065
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=171875000 process_calls=11143
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=390625000 process_calls=21750
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=4093750000 process_calls=53210
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1515625000 process_calls=11245
[2026-11-02 10:05:31.933] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=156250000 process_calls=10893
[2026-11-02 10:05:31.948] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=44144392 worst_materialized_at_ns=22620396000 worst_release_ns=22576251608 worst_deadline_ns=22642918276 worst_global_sequence=15759
[2026-11-02 10:05:31.948] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590587154300 maximum_inter_event_gap_ns=44932500 gap_begin_generation=1 gap_begin_begin=5594112 gap_begin_end=5595136 gap_begin_rate=44100 gap_begin_steady_ns=4401824142400 gap_end_generation=1 gap_end_begin=5595136 gap_end_end=5596160 gap_end_rate=44100 gap_end_steady_ns=4401869074900
[2026-11-02 10:05:31.948] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=3807 missing=audio begin_steady_ns=4401824225500 end_steady_ns=4401869077200 observed_wait_ns=44851700 held_dispatch_master_ns=127542917834 begin_master_ns=127415280800 end_master_ns=127460132100 dispatch_remaining_at_end_ns=82785734 resumed_dispatch_master_ns=127550083367
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590587113600 maximum_inter_event_gap_ns=45000800 gap_begin_generation=1 gap_begin_begin=5594112 gap_begin_end=5595136 gap_begin_rate=44100 gap_begin_steady_ns=4401823972500 gap_end_generation=1 gap_end_begin=5595136 gap_end_end=5596160 gap_end_rate=44100 gap_end_steady_ns=4401868973300
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590587116000 maximum_inter_event_gap_ns=44984000 gap_begin_generation=1 gap_begin_begin=5594112 gap_begin_end=5595136 gap_begin_rate=44100 gap_begin_steady_ns=4401824000600 gap_end_generation=1 gap_end_begin=5595136 gap_end_end=5596160 gap_end_rate=44100 gap_end_steady_ns=4401868984600
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590570823200 maximum_inter_event_gap_ns=44857500 gap_begin_generation=1 gap_begin_begin=5596160 gap_begin_end=5597184 gap_begin_rate=44100 gap_begin_steady_ns=4401823843200 gap_end_generation=1 gap_end_begin=5597184 gap_end_end=5598208 gap_end_rate=44100 gap_end_steady_ns=4401868700700
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590570920600 maximum_inter_event_gap_ns=44879800 gap_begin_generation=1 gap_begin_begin=5596160 gap_begin_end=5597184 gap_begin_rate=44100 gap_begin_steady_ns=4401823930300 gap_end_generation=1 gap_end_begin=5597184 gap_end_end=5598208 gap_end_rate=44100 gap_end_steady_ns=4401868810100
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590587098400 maximum_inter_event_gap_ns=44881900 gap_begin_generation=1 gap_begin_begin=5594112 gap_begin_end=5595136 gap_begin_rate=44100 gap_begin_steady_ns=4401823937800 gap_end_generation=1 gap_end_begin=5595136 gap_end_end=5596160 gap_end_rate=44100 gap_end_steady_ns=4401868819700
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590587100100 maximum_inter_event_gap_ns=44882400 gap_begin_generation=1 gap_begin_begin=5594112 gap_begin_end=5595136 gap_begin_rate=44100 gap_begin_steady_ns=4401823945400 gap_end_generation=1 gap_end_begin=5595136 gap_end_end=5596160 gap_end_rate=44100 gap_end_steady_ns=4401868827800
[2026-11-02 10:05:31.949] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590570777800 maximum_inter_event_gap_ns=45031300 gap_begin_generation=1 gap_begin_begin=5596160 gap_begin_end=5597184 gap_begin_rate=44100 gap_begin_steady_ns=4401823509600 gap_end_generation=1 gap_end_begin=5597184 gap_end_end=5598208 gap_end_rate=44100 gap_end_steady_ns=4401868540900
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=13595 last_generation=1 last_begin=13920256 last_end=13921280 last_rate=44100 last_steady_ns=4590570797200 maximum_inter_event_gap_ns=44747800 gap_begin_generation=1 gap_begin_begin=5596160 gap_begin_end=5597184 gap_begin_rate=44100 gap_begin_steady_ns=4401823806000 gap_end_generation=1 gap_end_begin=5597184 gap_end_end=5598208 gap_end_rate=44100 gap_end_steady_ns=4401868553800
[2026-11-02 10:05:31.949] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r374.output audio_commits=13595 real_samples=10471424 silence_samples=3449856
[2026-11-02 10:05:31.949] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:05:31.949] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=9471 maximum_call_ns=18774800 maximum_begin_steady_ns=4429119183400 maximum_end_steady_ns=4429137958200 maximum_encoder_pts=4626 maximum_native_result=0 maximum_between_submissions_ns=48226500 before_gap_begin_steady_ns=4403852786700 before_gap_end_steady_ns=4403855914800 before_gap_encoder_pts=3868 before_gap_native_result=0 after_gap_begin_steady_ns=4403904141300 after_gap_end_steady_ns=4403907445600 after_gap_encoder_pts=3869 after_gap_native_result=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=53210 maximum_ns=8044400 begin_steady_ns=4534252389800 end_steady_ns=4534260434200 video_cursor=7780 audio_cursor=not_owned next_video_cursor=7780 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=53210 maximum_ns=818000 begin_steady_ns=4393908307100 end_steady_ns=4393909125100 video_cursor=3570 audio_cursor=not_owned next_video_cursor=3570 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=53204 maximum_ns=352100 begin_steady_ns=4335453800200 end_steady_ns=4335454152300 video_cursor=1817 audio_cursor=not_owned next_video_cursor=1817 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=53204 maximum_ns=7273000 begin_steady_ns=4379856585000 end_steady_ns=4379863858000 video_cursor=3149 audio_cursor=not_owned next_video_cursor=3149 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=3 maximum_ns=337900 begin_steady_ns=4274918528700 end_steady_ns=4274918866600 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=9471 maximum_ns=7997300 begin_steady_ns=4534252413500 end_steady_ns=4534260410800 video_cursor=7780 audio_cursor=not_owned next_video_cursor=7780 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=9471 maximum_ns=7962400 begin_steady_ns=4534252426300 end_steady_ns=4534260388700 video_cursor=7780 audio_cursor=not_owned next_video_cursor=7780 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=9472 maximum_ns=6963500 begin_steady_ns=4435318987900 end_steady_ns=4435325951400 video_cursor=4812 audio_cursor=not_owned next_video_cursor=4812 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=53209 maximum_ns=93566900 begin_steady_ns=4274820130300 end_steady_ns=4274913697200 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:05:31.950] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r374.output output_generation=1 video_commits=9471 source[0].group=composition-r374.source.0 source[0].identity=composition-r374.source.0.input.video source[0].real_frames=3570 source[0].black_frames=5901 source[0].contribution_generation=0 source[1].group=composition-r374.source.1 source[1].identity=composition-r374.source.1.input.video source[1].real_frames=3570 source[1].black_frames=5901 source[1].contribution_generation=0 source[2].group=composition-r374.source.2 source[2].identity=composition-r374.source.2.input.video source[2].real_frames=7123 source[2].black_frames=2348 source[2].contribution_generation=0  source[2].first_interval_miss_first_candidate_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_candidate_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_candidate_final_source_sequence=4  source[2].first_interval_miss_first_candidate_final_presentation_ns=1093000000  source[2].first_interval_miss_first_candidate_final_duration_ns=33333333  source[2].first_interval_miss_first_candidate_final_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_final_source_sequence=4  source[2].first_interval_miss_first_real_final_presentation_ns=1093000000  source[2].first_interval_miss_first_real_final_duration_ns=33333333  source[2].first_interval_miss_first_real_final_decoded_sequence=unavailable source[2].first_interval_miss_cursor=5098 source[2].first_interval_miss_target_ns=170609584501 source[2].first_interval_miss_steady_ns=4444852155200 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable source[2].first_interval_miss_epoch_source_start_ns=993000000 source[2].first_interval_miss_epoch_master_release_ns=170536029600 source[2].first_interval_miss_prepare_expired=0 source[2].first_interval_miss_prepare_old_generation=0 source[2].first_interval_miss_prepare_incomplete=0 source[2].first_interval_miss_first_real_cursor_at_miss=unavailable source[2].first_interval_miss_first_real_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_begin_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_end_ns_at_miss=unavailable source[2].first_interval_miss_prepare_last_begin_ns=unavailable source[2].first_interval_miss_prepare_last_end_ns=unavailable source[2].first_interval_miss_first_candidate_steady_ns_final=4444863440300 source[2].first_interval_miss_first_candidate_begin_ns_final=170636029600 source[2].first_interval_miss_first_candidate_end_ns_final=170669362933 source[2].first_interval_miss_first_real_cursor=5099 source[2].first_interval_miss_first_real_steady_ns=4444886083700 source[2].first_interval_miss_cursor_committed_generation=0 source[2].first_interval_miss_cursor_committed_steady_ns=4444852268300
[2026-11-02 10:05:31.951] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=7127 operation=copy maximum_ns=250500 begin_steady_ns=4477619305800 end_steady_ns=4477619556300 native_result=0 thread_cpu_ns=0
[2026-11-02 10:05:31.951] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=21392 operation=copy maximum_ns=296500 begin_steady_ns=4482719070800 end_steady_ns=4482719367300 native_result=0 thread_cpu_ns=0
[2026-11-02 10:05:31.951] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=42786 operation=copy maximum_ns=1595800 begin_steady_ns=4312820639100 end_steady_ns=4312822234900 native_result=0 thread_cpu_ns=0
[2026-11-02 10:05:31.951] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=91100 begin_steady_ns=4274757519600 end_steady_ns=4274757610700 native_result=0 thread_cpu_ns=0
[2026-11-02 10:05:31.952] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=7127 operation=copy maximum_ns=598100 begin_steady_ns=4350456102800 end_steady_ns=4350456700900 native_result=0 thread_cpu_ns=0
[2026-11-02 10:05:31.952] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=7125 operation=copy maximum_ns=7700300 begin_steady_ns=4534252670100 end_steady_ns=4534260370400 native_result=0 thread_cpu_ns=0
[2026-11-02 10:05:31.952] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=4534252427300 end_steady_ns=4534260387900 wall_ns=7960600 thread_cpu_ns=0 surface_count=1 surface_total_ns=1300 surface_maximum_ns=1300 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=1100 validate_maximum_ns=1100 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=185000 black_maximum_ns=185000 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=44100 tile_maximum_ns=44100 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=9200 publish_maximum_ns=9200 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=4300 context_push_maximum_ns=4300 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=2 source_wait_total_ns=3700 source_wait_maximum_ns=3700 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=203500 plane_copy_maximum_ns=125500 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=5500 context_pop_maximum_ns=5500 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=7698300 batch_completion_maximum_ns=7698300 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=1 source_query_ready_maximum_ns=10800 source_query_ready_begin_steady_ns=4534252625400 source_query_ready_end_steady_ns=4534252636200 source_query_ready_tile=2 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-02 10:05:31.952] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=4590870239200 input_count=3569 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3569 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:05:31.952] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=4590870368800 input_count=3592 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3592 published_count=3583 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=4 incomplete=0
[2026-11-02 10:05:31.952] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=4590870490600 input_count=3599 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3599 published_count=3583 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.952] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-02 10:05:31.953] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:05:31.953] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=4590871063700 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:05:31.957] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:05:31.957] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=4590875372800 input_count=3597 accepted_count=3597 would_block_count=0 resolved_count=3597 output_attempt_count=0 published_count=3596 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=1 incomplete=0
[2026-11-02 10:05:31.963] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-02 10:05:31.964] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=4590881788600 input_count=3599 accepted_count=3599 would_block_count=0 resolved_count=3599 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=4590888868900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=4590889007600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=4590889086100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] hardware_transfer.summary forwarded=7168 download=0 upload=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=4590889221800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3597 download=0 upload=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=4590889356200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3599 download=0 upload=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=4590889485500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.971] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-02 10:05:31.972] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:05:31.972] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=4590889935200 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.972] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-02 10:05:31.983] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:05:31.984] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=4590901795800 input_count=3599 accepted_count=3599 would_block_count=0 resolved_count=3599 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.984] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-02 10:05:31.997] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-02 10:05:31.997] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=4590915105500 input_count=3599 accepted_count=3599 would_block_count=0 resolved_count=3599 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:31.997] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-02 10:05:32.008] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=4590925994700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:32.008] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=4590926175300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=3601 release_committed_count=3584 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:32.008] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=4590926253300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=3601 release_committed_count=3584 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:32.008] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=4590926328900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:32.008] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=4590926409200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3584 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:32.008] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=4590926479200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3584 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:05:32.008] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:05:32.009] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:05:32.009] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:05:32.009] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10345 completion_waits=10345 completed_datagrams=10484 drained_datagrams=173 would_block=37 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:05:32.009] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=121132 completion_waits=121132 completed_datagrams=212807 drained_datagrams=91711 would_block=37 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:05:32.009] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=75661 completion_waits=75661 completed_datagrams=106280 drained_datagrams=30718 would_block=98 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:05:32.010] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=74180 completion_waits=74180 completed_datagrams=106235 drained_datagrams=32152 would_block=98 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:05:32.010] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-02 10:05:32.011] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-02 10:05:32.072] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=391, threads=81, processThreads=130, workers=0, workerProgress=582640, workerProcessCalls=2325027, workerWaits=1742370, workerWakeups=1718353, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=925, logicalProcessors=22, averageSystemMachineCpuPercent=11.500086, averageProcessMachineCpuPercent=2.027988, peakProcessMachineCpuPercent=6.349206, averageProcessSingleCoreCpuPercent=44.615726, peakProcessSingleCoreCpuPercent=139.682540, initialWorkingSetBytes=190763008, workingSetBytes=248569856, peakWorkingSetBytes=250298368, totalPushed=508881, totalPopped=508845, droppedBuffers=0, encodedPacketsPushed=105865, encodedPacketsPopped=105865, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4366023, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=207, graphPayloadReservations=233083, graphPayloadReleases=233083, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4563292127900
[CLI] realtime video validation stopped successfully
[in @ 00000275f67a0d00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000275f67a0d00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41527.112344
[in @ 00000275f6d27580] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000275f6d27580] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 2005.249867
[in @ 00000275800c3a80] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000275800c3a80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 12409.882067
[in @ 00000275f6901d40] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000275f69016c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000275f6901d40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 12409.882067
[in @ 00000275f68ff5c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000275f69016c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41527.112344
[in @ 00000275f68ff5c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 2005.249867
[aac @ 00000275809c5580] Qavg: 16726.664
```

## CPU与内存完整采样

```text
1.0144187,4.27080688046895,239894528,1156268032
2.0467709,2.40789476163799,240144384,1156268032
3.0749335,2.83217053234753,240263168,1156268032
4.1072014,2.9585122938796,240541696,1156276224
5.1336082,2.56023333934548,240672768,1156276224
6.1610115,2.35036497086658,240812032,1156276224
7.2008666,2.11731860088444,240873472,1156280320
8.2225503,2.36352280776597,241860608,1157115904
9.2465613,2.28879377272315,244445184,1159569408
10.2703359,2.77493609521968,244445184,1159569408
11.2998506,2.06959824680679,244445184,1159569408
12.3399249,2.1168723671516,244633600,1159577600
13.3632533,2.15151318526344,244772864,1159577600
14.394686,2.06574972674593,244830208,1159581696
15.424298,2.69022346635078,244838400,1159581696
16.4546916,2.55032728181824,244887552,1159577600
17.4796582,3.0488798366698,245428224,1159950336
18.5083105,3.03795558518656,245489664,1159950336
19.5284067,2.78494233280066,246304768,1160769536
20.5540697,2.63133566908784,245493760,1159950336
21.5840207,2.20663630864699,246525952,1160769536
22.6239877,2.185384029231,245616640,1159950336
23.6524277,2.34799572874716,245747712,1159962624
24.6995662,1.8991149343056,245747712,1159958528
25.7290912,2.2075493773607,245813248,1159962624
26.7602412,2.41070208460986,245805056,1159958528
27.7842993,2.49675109431601,245977088,1159958528
28.8228869,1.77798281925464,245977088,1159958528
29.8456172,1.87499445001643,245977088,1159958528
30.873443,2.21119889452791,245977088,1159958528
31.896064,2.29190482104319,245977088,1159958528
32.9230744,2.28210931456975,246034432,1159958528
33.9463597,3.33151556959813,246095872,1159958528
34.9791031,2.81960825717387,246104064,1159966720
36.0055923,2.49083787907187,246104064,1159966720
37.0415346,2.26243295596677,246218752,1159966720
38.0757847,1.92278092468771,246284288,1159970816
39.1133919,2.25880275310348,247201792,1160781824
40.143538,2.61988434103051,246407168,1159974912
41.1799968,2.26130551450767,246407168,1159974912
42.2124853,3.23303182729704,247250944,1160663040
43.2421826,2.6210262339851,247316480,1160663040
44.2653843,2.84590205252964,247308288,1160658944
45.2951157,3.24168825173069,247308288,1160658944
46.3238436,2.07118113369125,247308288,1160658944
47.3526583,2.69230830744968,247308288,1160658944
48.386442,2.33585877517001,247369728,1160658944
49.4131268,2.9746006493203,247373824,1160658944
50.441399,1.79581915089304,247529472,1160933376
51.4676046,2.28389905492622,247529472,1160933376
52.4994962,1.30772633305845,247529472,1160933376
53.5275681,2.34883642600554,247586816,1160933376
54.5649058,3.56025026197528,247586816,1160933376
55.593441,2.83114454243454,247586816,1160933376
56.6261261,2.88854225305908,247586816,1160937472
57.6542263,2.2106087254212,247705600,1160933376
58.676361,1.94557171734445,247705600,1160933376
59.7021636,2.21556006265462,247730176,1160966144
60.7196226,2.16392458610572,247783424,1160863744
61.7482077,2.14051763481169,247844864,1160863744
62.783734,2.74344465312864,247844864,1160863744
63.8132642,2.96637949302243,247848960,1160871936
64.85141,1.91556558205635,247971840,1160867840
65.8807939,1.31091210789465,247971840,1160867840
66.9033327,2.08371733002386,248999936,1161797632
67.9411079,2.32687457483348,248999936,1161797632
68.9709348,1.79310805446132,248999936,1161797632
69.9905268,2.08973963917117,249085952,1161801728
71.0163058,2.14637319096467,249143296,1161801728
72.0420777,3.2542012330599,249200640,1161801728
73.069271,3.04227062228694,249200640,1161801728
74.0980199,3.45189809061898,249208832,1161805824
75.1263931,2.41721143116667,249208832,1161809920
76.159711,3.02423871685567,249270272,1161814016
77.1933596,2.40487478485964,249499648,1161818112
78.2255933,1.99534184062105,250302464,1162625024
79.2567314,2.4107299056697,249499648,1161814016
80.2958247,2.59732560720357,249499648,1161814016
81.3263564,2.13647435149695,249499648,1161814016
82.3658756,2.80122947049157,249499648,1161814016
83.3993933,2.19902114180266,250634240,1162997760
84.4255864,2.90681602269061,249823232,1162178560
85.4539276,2.48635198299765,249823232,1162178560
86.4815704,2.34981720036645,249823232,1162178560
87.513789,2.33940051775148,249827328,1162182656
88.5464225,3.1637996002894,249827328,1162182656
89.5747594,3.59141232623454,249831424,1162186752
90.6116328,3.35635347224036,250056704,1162186752
91.6429097,2.54814289846974,250056704,1162186752
92.6695474,2.69801738591557,250064896,1162190848
93.6908057,3.40767231596907,250056704,1162186752
94.7284248,2.32722463115099,250056704,1162186752
95.7531692,2.7030021960953,250056704,1162186752
96.7749838,3.61433651288778,250056704,1162186752
97.8087945,2.67929744162674,250056704,1162186752
98.832913,2.08050320171134,250056704,1162186752
99.859016,2.56099135183401,250056704,1162186752
100.8891672,2.34409543693461,250056704,1162186752
101.9166202,2.07375112845243,250056704,1162186752
102.9490167,1.99502719246828,250060800,1162190848
103.971691,1.94454516324147,250122240,1164292096
105.0067269,2.26441421017378,250122240,1164292096
106.0301417,1.80434258825543,250122240,1164292096
107.0512231,2.43447334810472,250974208,1165094912
108.0892248,2.46321194061454,251031552,1165094912
109.1152992,2.90715229368801,250241024,1164296192
110.1483979,2.40615485678709,250241024,1164296192
111.1796985,2.47921719605147,250245120,1164296192
112.2116515,2.34000262344576,250245120,1164296192
113.2335835,2.43244702636327,250236928,1164292096
114.2582757,2.42589477556816,250236928,1164296192
115.2840734,2.7694633073452,250298368,1164296192
116.3094008,3.11707531396576,250298368,1164296192
117.3410212,2.34075705295547,250298368,1164300288
118.3663093,2.14740085782187,250298368,1164300288
119.4035751,2.12260400897682,250298368,1164300288
120.435967,1.99503608165569,242315264,1157541888
121.4676567,1.44566459539847,242311168,1157541888
122.4908784,1.31880688044617,242311168,1157537792
123.5194215,1.10482840861373,242311168,1157541888
124.5410128,1.25138995497426,242315264,1157541888
125.5651751,1.24824853532403,242311168,1157537792
126.5910092,1.17698014097637,243097600,1158332416
127.6243355,1.16844636939596,243097600,1158332416
128.6563352,1.23876885905014,242311168,1157541888
129.6895468,1.16857608222399,242311168,1157537792
130.7207234,1.23975766217841,242319360,1157541888
131.7478028,1.31385345493428,242315264,1157541888
132.7704464,1.3195524014249,242315264,1157541888
133.7986811,1.51959469953696,242319360,1157541888
134.832916,1.44210688763962,243077120,1158619136
135.8604247,1.17506193732118,243077120,1158619136
136.8933708,1.30639131914222,242290688,1157824512
137.9268708,1.5118529269473,243077120,1158619136
138.9585759,1.44564301633021,243085312,1158627328
139.9809898,1.31984885786648,242298880,1157828608
141.0100664,0.966223682297494,243085312,1158627328
142.0359731,1.73073066178257,242290688,1157828608
143.0538245,1.1862108394569,242290688,1157828608
144.0854686,1.44572849563841,242290688,1157828608
145.1263826,1.29639126592767,242290688,1157824512
146.1625586,1.4394053449677,242298880,1157828608
147.1968153,1.30473587280782,242290688,1157824512
148.2235957,1.31423605104052,242290688,1157828608
149.2478222,1.45619867551491,242311168,1157857280
150.2749126,1.45213826623955,242311168,1157857280
151.3058782,1.10223235029727,242311168,1157857280
152.3408462,1.16659294165266,242311168,1157857280
153.3709648,1.03419247947848,242311168,1157857280
154.4069584,1.16543805254815,242311168,1157857280
155.4409244,1.30510269987778,242311168,1157857280
156.4809231,1.57069689344103,242311168,1157857280
157.5164162,1.71470788344045,242311168,1157857280
158.5534125,1.30128894209345,242311168,1157857280
159.589801,1.1649939801883,242311168,1157857280
160.6159771,1.38422103716363,242311168,1157857280
161.6442337,1.17420725880716,242311168,1157857280
162.6665179,1.45896539604864,242311168,1157857280
163.7033343,1.23301395590299,242311168,1157857280
164.7356822,0.963161916460685,242376704,1157926912
165.7729385,1.23249103515601,242319360,1157861376
166.7983144,1.31603621479871,242315264,1157861376
167.8367099,1.3679321081944,242315264,1157861376
168.8608203,1.38701310469509,243253248,1158651904
169.8881544,1.45179379592996,245362688,1160429568
170.914103,1.93833917570171,246951936,1162317824
171.9471219,2.33758813829323,247066624,1162375168
172.9698904,2.29157429076085,247279616,1162645504
174.005665,1.57710251561752,247283712,1162645504
175.0365218,1.44683264710216,247283712,1162645504
176.0658449,1.93198458641059,247287808,1162645504
177.0879575,2.0845861974325,248156160,1163448320
178.1110354,1.9437780482174,247750656,1162645504
179.1423844,1.79046172448987,247762944,1162645504
180.1636026,2.71233548681015,247775232,1162645504
181.1905393,1.72899476843913,247775232,1162645504
182.2186854,1.79603940441046,247832576,1162645504
183.2532361,1.71626985687426,248627200,1163448320
184.2737157,1.73993500881174,247840768,1162645504
185.2980253,2.56547523238178,247840768,1162649600
186.328061,1.86169628524882,247840768,1162649600
187.3513724,2.15154892777948,247861248,1162661888
188.3864396,2.0584961229395,247853056,1162661888
189.4070254,2.29647522040768,247853056,1162657792
190.4344939,2.76495979283949,247853056,1162657792
191.4662423,2.47814116485975,247853056,1162657792
192.498097,2.13373505538576,247853056,1162657792
193.5252866,1.52114079036628,247853056,1162657792
194.5497346,2.14916183686681,247853056,1162657792
195.5873789,1.91649138691972,247910400,1162657792
196.6157503,1.79564592042419,247967744,1162657792
197.64278,1.52137761936195,247967744,1162657792
198.6711167,1.79570651236204,247967744,1162657792
199.7003156,2.4842799402702,247967744,1162657792
200.7209885,1.6004370521376,247967744,1162657792
201.7534759,1.99485155064272,247967744,1162657792
202.7905367,2.05453896066833,247967744,1162657792
203.816617,3.18400563244943,248029184,1162657792
204.8483888,2.06507080168484,248033280,1162661888
205.8724025,1.66457290028979,248033280,1162661888
206.9049483,2.54501147463956,248033280,1162661888
207.9373018,2.61428244914522,248033280,1162661888
208.9666831,2.27685309612675,248033280,1162661888
209.9992561,1.71955705002766,248033280,1162661888
211.0267069,2.07375556881344,248152064,1162661888
212.0541387,1.58990867060247,248152064,1162661888
213.0911084,2.12321010484161,248152064,1162665984
214.1094952,1.60402975301008,248164352,1162678272
215.1385978,2.07042700910656,248172544,1162678272
216.1641146,1.86989977771564,248168448,1162678272
217.1924074,2.34833184407468,248160256,1162674176
218.2180068,2.4929989056333,248164352,1162678272
219.2507738,2.06308084803432,248164352,1162678272
220.2808741,2.34421126493478,248164352,1162674176
221.3052674,2.01061359041407,248164352,1162674176
222.3265273,1.80814982463414,248164352,1162674176
223.3507552,1.9415955800817,248164352,1162674176
224.3776278,1.86743091242639,248164352,1162674176
225.4137722,1.98781086005876,248164352,1162674176
226.438103,2.4267506693594,248164352,1162674176
227.4616488,1.73472274696276,248164352,1162674176
228.492581,2.06675261300581,248164352,1162674176
229.5216697,2.00143980874449,248168448,1162678272
230.5541875,2.06357877625139,248164352,1162674176
231.5882921,1.64832982518934,248164352,1162678272
232.6119725,1.80387444078335,248164352,1162678272
233.6489874,1.71219158164283,248172544,1162678272
234.6779212,2.20881778082062,248164352,1162674176
235.6996073,1.66836512168017,248164352,1162674176
236.7308205,2.68604626437712,248164352,1162678272
237.7565443,2.07724712849775,248164352,1162674176
238.7835446,1.79804320319177,248160256,1162674176
239.8106546,2.07444365080842,248168448,1162678272
240.8338791,2.08232095515874,248160256,1162678272
241.8631104,1.8631513017177,248160256,1162674176
242.8980952,2.26452601043032,248160256,1162674176
243.9210767,1.31911654138597,248160256,1162674176
244.9584366,1.98548169339211,248160256,1162674176
245.98111,2.01399497719319,248160256,1162674176
247.0013479,1.87957498575932,248160256,1162674176
248.02553,1.66429920474637,248160256,1162674176
249.0474669,2.43243536322594,248160256,1162674176
250.087114,1.98111367877527,248160256,1162674176
251.112895,1.80018045673581,248160256,1162674176
252.1542203,1.6369000681588,248217600,1162674176
253.1808124,2.00630717001335,248217600,1162674176
254.2105003,2.13822513157098,249012224,1163476992
255.2350583,2.3568921693772,248217600,1162678272
256.2581367,1.80493587694829,248217600,1162674176
257.2793401,1.73870179223669,248217600,1162674176
258.3006467,2.92072385065811,248221696,1162678272
259.3185066,2.16307229065083,248221696,1162678272
260.3410582,2.01423487177469,248217600,1162674176
261.3710143,1.86184016616208,248217600,1162674176
262.3881301,1.81551688518742,248217600,1162674176
263.4069787,2.02155559806341,248217600,1162674176
264.4389507,2.06467018308813,248217600,1162674176
265.4732651,2.33466026120568,248279040,1162674176
266.5042844,2.47989361772192,248279040,1162674176
267.5295578,1.80107170349967,248279040,1162674176
268.55967,1.93050462234739,248279040,1162674176
269.5886183,2.00171290521501,248279040,1162674176
270.6199426,2.47916022323744,248279040,1162674176
271.6435479,2.70601018149906,248279040,1162674176
272.6738564,1.79226989692012,248279040,1162674176
273.7079123,2.26656025075627,248279040,1162674176
274.7464295,1.84649193712308,248279040,1162674176
275.7759765,1.65562665380541,248279040,1162674176
276.8037307,2.21135294093399,248279040,1162674176
277.8308768,2.48924489108047,248340480,1162674176
278.8504951,2.64693526623018,248340480,1162674176
279.8860642,2.26324829506783,248401920,1162674176
280.9155322,1.37979475365394,248401920,1162674176
281.9544598,2.1192088317362,248401920,1162674176
282.9809836,1.66050261527832,248401920,1162674176
284.0047681,1.87306375156459,248401920,1162674176
285.0338631,2.13945704279436,249196544,1163476992
286.0664953,2.26968517929237,248401920,1162678272
287.0993952,3.02546258354749,248401920,1162674176
288.1222464,2.29138901142221,248397824,1162674176
289.147655,1.93935994259886,248401920,1162674176
290.1863201,1.23081933811879,249188352,1163468800
291.2196492,1.44337101580442,248401920,1162678272
292.2461653,1.59132694292156,248401920,1162678272
293.2664554,1.46181686240733,248406016,1162678272
294.2955521,1.1732486982383,248401920,1162678272
295.3210916,1.52358831619848,248401920,1162678272
296.3544001,1.23719982068187,249196544,1163476992
297.3756161,1.39094427178439,248401920,1162678272
298.4043668,1.24268113830601,248410112,1162678272
299.4282132,1.17926513550893,248401920,1162674176
300.4657443,1.43752536451902,248569856,1162711040
301.5026994,1.23284903165922,248569856,1162711040
302.5347382,1.30753981166387,248569856,1162711040
303.5584307,1.04068449176968,248569856,1162711040
304.586306,1.10554620425613,248569856,1162711040
305.6055275,1.39366619076867,248569856,1162711040
306.6290786,1.31838246100443,248569856,1162711040
307.6639682,1.166681319086,248569856,1162711040
308.6954717,1.44592555694411,248569856,1162711040
309.7246604,1.38016920070587,248569856,1162711040
310.7455884,1.04350248900113,248569856,1162711040
311.7713729,1.24627452540864,248569856,1162711040
312.7973785,1.31522851160059,248569856,1162711040
313.8322725,1.44118844319057,248569856,1162711040
314.8522616,1.46224824630697,248569856,1162711040
315.8879677,1.23433577431776,248569856,1162711040
```

## 源结尾与VLC记录

```text
source0
frame= 3576 fps= 30 q=-1.0 size=  118130KiB time=00:01:59.20 bitrate=8118.5kbits/s speed=   1x elapsed=0:01:58.66    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.17    
[out#0/rtp @ 000001c5c2b58b80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001c5c4825c40] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
source1
frame= 3559 fps= 30 q=-1.0 size=  117543KiB time=00:01:58.63 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:58.13    
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.65    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.17    
[out#0/rtp @ 0000012d1cb6d100] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
source2
frame= 3558 fps= 30 q=-1.0 size=  117493KiB time=00:01:58.60 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.09    
frame= 3573 fps= 30 q=-1.0 size=  118036KiB time=00:01:59.10 bitrate=8118.8kbits/s speed=   1x elapsed=0:01:58.59    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.12    
[out#0/rtp @ 00000259c9945400] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
reentry
frame= 3573 fps= 30 q=-1.0 size=  118036KiB time=00:01:59.11 bitrate=8117.6kbits/s speed=   1x elapsed=0:01:58.60    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.11    
[out#0/rtp @ 00000289bb319580] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 00000289bd121c00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r374-vlc.log'
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
cache_read debug: pre-buffering done 300 bytes in 0s - 292968 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 031bbc38
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
qt debug: IM: Setting an input
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: no access_demux modules matched
main debug: looking for meta fetcher module matching "any": 1 candidates
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
ts debug: pid[258] unknown
ts debug: pid[257] unknown
ts debug: first packet for pid=257 cc=0x6
ts debug: first packet for pid=0 cc=0x6
ts debug: PATCallBack called
ts debug: new PAT ts_id=1 version=0 current_next=1
ts debug:   * number=1 pid=256
ts debug: temporary receiving program 1
ts debug: first packet for pid=256 cc=0x6
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
ts debug: first packet for pid=257 cc=0x7
main debug: Buffering 0%
ts debug: first packet for pid=258 cc=0x2
mpeg4audio: AAC channels: 2 samplerate: 44100
main debug: restarting module due to input format change
main debug: removing module "faad"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
main debug: Buffering 6%
faad warning: decoded zero sample
main debug: reusing audio output
main debug: looking for aout stream module matching "any": 2 candidates
main debug: Buffering 13%
main debug: Buffering 20%
main debug: Buffering 26%
main debug: Buffering 33%
main debug: Buffering 40%
wasapi debug: modified format
main debug: Buffering 46%
main debug: Buffering 53%
main debug: Buffering 60%
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 80%
main debug: Buffering 86%
main debug: Buffering 93%
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
main debug: Buffering 100%
main debug: Stream buffering done (1066 ms in 656 ms)
main error: buffer deadlock prevented
main debug: Decoder wait done in 0 ms
main debug: inserting 6568 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (93712): up-sampling
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
main debug: resized to 72401408x52041800
main debug: resized to 72401408x52041800
main debug: resized to 72401152x52041800
main debug: resized to 72401152x52041800
main debug: resized to 72400896x52041800
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
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x065e8058
main debug: original format sz 1280x736, of (0,0), vsz 1280x720, 4cc DX11, sar 1:1, msk r0x0 g0x0 b0x0
main debug: removing module "freetype"
main debug: looking for text renderer module matching "any": 3 candidates
main debug: VoutDisplayEvent 'resize' 1280x720
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
main debug: auto hiding mouse cursor
main debug: resampling stopped (drift: -188 us)
main debug: accepted socket 3464 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (1ab23910) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04815868) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1ad5e8e0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04815b00) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1ab23910 removed from chain
main debug: removing module "swscale"
main debug: Filter 04815868 removed from chain
main debug: Filter 1ad5e8e0 removed from chain
main debug: removing module "swscale"
main debug: Filter 04815b00 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r374-native-2026-11-02-10h02m37s319.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1ad5e8e0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b691300) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1ad5e8e0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b691300 removed from chain
main debug: accepted socket 3940 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (04c8ba88) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1aa9e6e0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1bac9c40) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1bbbc808) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04c8ba88 removed from chain
main debug: removing module "swscale"
main debug: Filter 1aa9e6e0 removed from chain
main debug: Filter 1bac9c40 removed from chain
main debug: removing module "swscale"
main debug: Filter 1bbbc808 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r374-native-2026-11-02-10h03m43s266.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1bac9c40) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1aa9e6e0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1bac9c40 removed from chain
main debug: removing module "swscale"
main debug: Filter 1aa9e6e0 removed from chain
main debug: accepted socket 4024 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (1bac9c40) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1abbfe80) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1aa9e6e0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1badf378) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1bac9c40 removed from chain
main debug: removing module "swscale"
main debug: Filter 1abbfe80 removed from chain
main debug: Filter 1aa9e6e0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1badf378 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r374-native-2026-11-02-10h05m15s994.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1bc520e0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1aa9e6e0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1bc520e0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1aa9e6e0 removed from chain
main debug: accepted socket 3076 (from socket 1176)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
main debug: incoming request - stopping current input
ts debug: Can't read TS packet at 106746588
main debug: EOF reached
main warning: picture is too late to be displayed (missing 262 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 228 ms)
main warning: picture is too late to be displayed (missing 195 ms)
main warning: picture is too late to be displayed (missing 162 ms)
main warning: picture is too late to be displayed (missing 128 ms)
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp12612
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --
```

## 逐项清理清单

- composition-r374-cli-error.log (1966 bytes)
- composition-r374-cli.log (1220320 bytes)
- composition-r374-monitor.log (15477 bytes)
- composition-r374-native-2026-11-02-10h02m37s319.png (12152 bytes)
- composition-r374-native-2026-11-02-10h03m43s266.png (326986 bytes)
- composition-r374-native-2026-11-02-10h05m15s994.png (12152 bytes)
- composition-r374-reentry-stdout.log (540 bytes)
- composition-r374-reentry.log (29521 bytes)
- composition-r374-source0-stdout.log (540 bytes)
- composition-r374-source0.log (29521 bytes)
- composition-r374-source1-stdout.log (335 bytes)
- composition-r374-source1.log (28926 bytes)
- composition-r374-source2-stdout.log (335 bytes)
- composition-r374-source2.log (28925 bytes)
- composition-r374-vlc.log (37861 bytes)

## 最终提升环境核验与补充画面

CIM核验本轮全部精确PID及旧VLC PID均不存在，UDP62024无监听。此前新播放器无初始截图，但发现旧VLC按其r371前缀写出了本轮10h00m49s651快照；已实际查看为三格游戏画面/第四格黑。其生成时间属r374、对应当时接收62024的旧VLC34696，作为注明播放器来源的补充观察，不能掩盖测试隔离失误；仍不判完整验收通过。

额外遗留截图清单：composition-r371-native-2026-11-02-09h31m37s007.png (856131 bytes)、composition-r371-native-2026-11-02-10h00m49s651.png (785498 bytes)、composition-r371-native-2026-11-02-10h01m39s447.png (760425 bytes)。第一张属r372时间段，未用作r374证据；另两张为本轮旧VLC前缀写入，均记录后删除。

最终errors/workerErrors/stalledIntervals/pressure均0；payload预约释放233083匹配并归零。平均整机进程CPU2.027988%，峰6.349206%；WS峰250298368 bytes。全部本轮日志、快照等及上述3张遗留按清单清理，指定120秒源保留。
