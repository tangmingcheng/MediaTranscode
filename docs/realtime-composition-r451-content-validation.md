# r451：Windows 带源时间标记的内容取证

状态：实流和离线测量结束，内容同步门禁未通过；候选偏移待输入/输出联合取证。生产源码ce71265d，Windows FFmpeg不修改。采用已审查的content-timing-measurement方法，保留未标记源r449/rk450结果；source2新增drawtext并以H264 NVENC CBR8200kbps重新编码，音频copy，其余输入与720p30 H264 VBR8–12Mbps/AAC192kbps TS/RTP输出不变。源仍指定完整120秒，不循环。dumpcap420秒仅捕获保险上限，不限制CLI或源。

## 实际命令

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i 11 -f "udp dst port 62024" -a duration:420 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-output.pcapng' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-capture.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-capture.log' -PassThru; Write-Output "CAPTURE_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CAPTURE_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r451 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r451-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```

启动PID：capture40852；source0/1/2/3=43200/27332/38764/43336；CLI37000；VLC39448。

## 重入命令

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-reentry.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-reentry.log' -PassThru; Write-Output "REENTRY_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "REENTRY_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

首轮四源exit0；VLC17h57m21s400四格且source2标记可读，17h59m03s136全黑。重入PID6816，仍完整120秒。

## 离线分析实际命令与程序

以下通过 D:/Python/python.exe -c 直接执行，未创建测试脚本文件；只分析已结束捕获，不增加实流FFmpeg监控。

```python
import subprocess,pathlib,json,struct
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
cmd=['D:/Wireshark/tshark.exe','-r',str(p/'composition-r451-output.pcapng'),'-Y','udp.dstport==62024','-T','fields','-e','udp.payload']
r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
last={}; cc={}; stats=dict(datagrams=0,rtp_gaps=0,ts_packets=0,tei=0,cc_errors=0)
with (p/'composition-r451-output.ts').open('wb') as f:
 for line in r.stdout.decode().splitlines():
  if not line: continue
  if line=='No matching RTP/H.264 payload dumped.': continue
  b=bytes.fromhex(line.replace(':','')); assert b[0]>>6==2
  seq=int.from_bytes(b[2:4],'big'); ssrc=int.from_bytes(b[8:12],'big')
  if ssrc in last and seq!=(last[ssrc]+1)%65536: stats['rtp_gaps']+=1
  last[ssrc]=seq; pos=12+4*(b[0]&15)
  if b[0]&16: pos+=4+4*int.from_bytes(b[pos+2:pos+4],'big')
  end=len(b)-(b[-1] if b[0]&32 else 0); payload=b[pos:end]
  assert len(payload)%188==0 and len(payload)>0
  for i in range(0,len(payload),188):
   t=payload[i:i+188]; assert t[0]==0x47
   pid=((t[1]&31)<<8)|t[2]; afc=(t[3]>>4)&3; counter=t[3]&15
   stats['tei']+=bool(t[1]&128); stats['ts_packets']+=1
   dis=bool(afc&2 and t[4]>0 and t[5]&128)
   if pid!=8191 and afc&1:
    if pid in cc and not dis and counter!=(cc[pid]+1)%16: stats['cc_errors']+=1
    cc[pid]=counter
  f.write(payload); stats['datagrams']+=1
stats['stderr']=r.stderr.decode(); stats['ts_bytes']=(p/'composition-r451-output.ts').stat().st_size
(p/'composition-r451-wire.json').write_text(json.dumps(stats,indent=2))
print(json.dumps(stats))

```

```python
import subprocess,pathlib,re,json
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
meta={}
for name,path in [('reference',p/'test-continuous-120s.mp4'),('received',p/'composition-r451-output.ts')]:
 cmd=['D:/mabs/local64/bin-video/ffmpeg.exe','-hide_banner','-nostdin','-y','-copyts','-i',str(path),'-map','0:a:0','-vn','-af','aformat=sample_fmts=flt:sample_rates=44100:channel_layouts=mono,ashowinfo','-c:a','pcm_f32le','-f','f32le',str(p/('composition-r451-'+name+'.f32'))]
 r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 log=r.stderr.decode(errors='replace'); (p/('composition-r451-'+name+'-audio.log')).write_text(log)
 assert r.returncode==0,log[-2000:]
 blocks=[]; total=0
 for line in log.splitlines():
  if 'Parsed_ashowinfo' not in line or 'nb_samples:' not in line: continue
  pts=int(re.search(r'pts:(-?\d+)',line)[1]); rate=int(re.search(r'rate:(\d+)',line)[1]); n=int(re.search(r'nb_samples:(\d+)',line)[1])
  blocks.append(dict(index=total,pts=pts,rate=rate,n=n)); total+=n
 assert total*4==(p/('composition-r451-'+name+'.f32')).stat().st_size
 gaps=[i for i in range(1,len(blocks)) if blocks[i]['pts'] != blocks[i-1]['pts']+blocks[i-1]['n']]
 meta[name]=dict(command=cmd,blocks=blocks,samples=total,gaps=gaps)
 print(name,'samples',total,'blocks',len(blocks),'first',blocks[0],'gaps',len(gaps))
(p/'composition-r451-audio-map.json').write_text(json.dumps(meta))

```

```python
import pathlib,json,numpy as np
from scipy.signal import correlate,find_peaks
p=pathlib.Path('D:/Code/MyCode/MediaTranscode/out/acceptance')
m=json.loads((p/'composition-r451-audio-map.json').read_text())
assert not m['reference']['gaps'] and not m['received']['gaps']
r=np.fromfile(p/'composition-r451-reference.f32',dtype='<f4').astype(np.float64)
o=np.fromfile(p/'composition-r451-received.f32',dtype='<f4').astype(np.float64)
fs=44100;n=44100;energy=np.r_[0,np.cumsum(r*r)]; norm=energy[n:]-energy[:-n]
times=[1,10,30,60,90,105,150,180,210,240]; markers=[104,374,974,1874,2774,3224,484,1384,2284,3184]
rows=[]
for t,frame in zip(times,markers):
 k=round((t-m['received']['blocks'][0]['pts']/fs)*fs); w=o[k:k+n]; we=np.dot(w,w)
 assert len(w)==n and we>0
 c=correlate(r,w,mode='valid',method='fft')/np.sqrt(np.maximum(norm*we,np.finfo(float).tiny))
 j=int(np.argmax(c)); peaks,_=find_peaks(c,distance=fs)
 best=sorted(peaks,key=lambda x:c[x],reverse=True)[:5]
 sr=m['reference']['blocks'][0]['pts']/fs+j/fs
 da=t-sr; dv=t-frame/30
 row=dict(output_pts=t,video_frame=frame,source_video_pts=frame/30,source_audio_pts=sr,audio_offset=da,video_offset=dv,video_minus_audio_ms=(dv-da)*1000,correlation=float(c[j]),other_peaks=[dict(source_time=float(q/fs),correlation=float(c[q])) for q in best])
 rows.append(row)
print(json.dumps(rows,indent=2))
(p/'composition-r451-content-result.json').write_text(json.dumps(rows,indent=2))

```

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -y -copyts -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-output.ts' -map 0:v:0 -an -vf "select='eq(n,30)+eq(n,300)+eq(n,900)+eq(n,1800)+eq(n,2700)+eq(n,3150)+eq(n,4500)+eq(n,5400)+eq(n,6300)+eq(n,7200)',crop=640:70:0:360,showinfo" -fps_mode passthrough 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-marker-%02d.png' 2> 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-video-analysis.log'
```

## 结果与根因边界

本轮内容同步门禁未通过：候选偏移首代231.678004535ms、重入代237.414965986ms，正号表示视频较迟。10点不是首次有效帧、全程分位数或长稳证明。独立音频全局相关仍有周期高峰，完整候选保留，不按视频选择音频峰。新增视频编码而音频copy可能影响发送端RTP/RTCP映射；本轮仅捕获输出，不能区分源端与引擎，不能据此修改核心或Windows FFmpeg。下一次最小诊断需同捕source2视频/音频RTP及SR和输出，关联输入标记、音频样本及共同NTP，再计算引擎增量。

五次源均3600帧/完整120秒、exit0；VLC四张原生图确认四格、全黑、仅选中格恢复、再次全黑。CLI经精确身份/控制台成员验证发送CtrlC，caller_stop/exit0；VLC quit0，dumpcap CtrlC0。193157包，捕获drop0；RTP缺口/TS CC错误/TEI均0，1218905个TS包、229154140字节。此为接收完整性，不是NIC发送服务曲线证明。音频参考5292000样本、接收12365824样本，均首PTS0、逐块连续、日志样本总数=PCM长度；没有人工扣除AAC priming。视频选帧showinfo与marker序号逐项核对。

最终errors/workerErrors/dropped/pressure0；ledger323596预留=释放，0字节/0对象；CPU整机均值2.739781%、单核60.275186%；WS235520000→296669184，峰302465024字节。原输入告警与sender delivery未证明原样记录，不以功能正常收尾升级内容门禁。

### 相关完整候选
[
  {
    "output_pts": 1,
    "video_frame": 104,
    "source_video_pts": 3.466666666666667,
    "source_audio_pts": 3.698344671201814,
    "audio_offset": -2.698344671201814,
    "video_offset": -2.466666666666667,
    "video_minus_audio_ms": 231.6780045351474,
    "correlation": 0.9984868226918422,
    "other_peaks": [
      {
        "source_time": 3.698344671201814,
        "correlation": 0.9984868226918422
      },
      {
        "source_time": 36.76501133786848,
        "correlation": 0.9974385492195834
      },
      {
        "source_time": 20.231678004535148,
        "correlation": 0.9974382323267957
      },
      {
        "source_time": 86.36501133786848,
        "correlation": 0.9974093570916942
      },
      {
        "source_time": 61.565011337868484,
        "correlation": 0.9973844788357809
      }
    ]
  },
  {
    "output_pts": 10,
    "video_frame": 374,
    "source_video_pts": 12.466666666666667,
    "source_audio_pts": 12.698344671201815,
    "audio_offset": -2.6983446712018146,
    "video_offset": -2.466666666666667,
    "video_minus_audio_ms": 231.67800453514786,
    "correlation": 0.9993951778978192,
    "other_peaks": [
      {
        "source_time": 12.698344671201815,
        "correlation": 0.9993951778978192
      },
      {
        "source_time": 78.83167800453515,
        "correlation": 0.9982360868663306
      },
      {
        "source_time": 45.76501133786848,
        "correlation": 0.9982003111905856
      },
      {
        "source_time": 95.36501133786848,
        "correlation": 0.9981939688870883
      },
      {
        "source_time": 62.29834467120181,
        "correlation": 0.9981818655845559
      }
    ]
  },
  {
    "output_pts": 30,
    "video_frame": 974,
    "source_video_pts": 32.46666666666667,
    "source_audio_pts": 32.698344671201816,
    "audio_offset": -2.6983446712018164,
    "video_offset": -2.4666666666666686,
    "video_minus_audio_ms": 231.67800453514786,
    "correlation": 0.9986917912116694,
    "other_peaks": [
      {
        "source_time": 32.698344671201816,
        "correlation": 0.9986917912116694
      },
      {
        "source_time": 65.76501133786849,
        "correlation": 0.9977219033438169
      },
      {
        "source_time": 98.83167800453515,
        "correlation": 0.9977179858060885
      },
      {
        "source_time": 82.29834467120182,
        "correlation": 0.9976868090404565
      },
      {
        "source_time": 16.16501133786848,
        "correlation": 0.997678991543839
      }
    ]
  },
  {
    "output_pts": 60,
    "video_frame": 1874,
    "source_video_pts": 62.46666666666667,
    "source_audio_pts": 62.698344671201816,
    "audio_offset": -2.6983446712018164,
    "video_offset": -2.4666666666666686,
    "video_minus_audio_ms": 231.67800453514786,
    "correlation": 0.9989762796576397,
    "other_peaks": [
      {
        "source_time": 62.698344671201816,
        "correlation": 0.9989762796576397
      },
      {
        "source_time": 112.29834467120182,
        "correlation": 0.9981297736046137
      },
      {
        "source_time": 46.16501133786848,
        "correlation": 0.9979263025873011
      },
      {
        "source_time": 79.23167800453514,
        "correlation": 0.9976940570339352
      },
      {
        "source_time": 95.76501133786849,
        "correlation": 0.9976865550605193
      }
    ]
  },
  {
    "output_pts": 90,
    "video_frame": 2774,
    "source_video_pts": 92.46666666666667,
    "source_audio_pts": 92.69834467120181,
    "audio_offset": -2.6983446712018093,
    "video_offset": -2.4666666666666686,
    "video_minus_audio_ms": 231.67800453514076,
    "correlation": 0.9992403857973394,
    "other_peaks": [
      {
        "source_time": 92.69834467120181,
        "correlation": 0.9992403857973394
      },
      {
        "source_time": 76.16501133786848,
        "correlation": 0.9939787917346975
      },
      {
        "source_time": 10.031678004535147,
        "correlation": 0.9937981252781432
      },
      {
        "source_time": 59.631678004535146,
        "correlation": 0.9936522073112773
      },
      {
        "source_time": 26.56501133786848,
        "correlation": 0.9911646339463642
      }
    ]
  },
  {
    "output_pts": 105,
    "video_frame": 3224,
    "source_video_pts": 107.46666666666667,
    "source_audio_pts": 107.69834467120181,
    "audio_offset": -2.6983446712018093,
    "video_offset": -2.4666666666666686,
    "video_minus_audio_ms": 231.67800453514076,
    "correlation": 0.9984566713936469,
    "other_peaks": [
      {
        "source_time": 107.69834467120181,
        "correlation": 0.9984566713936469
      },
      {
        "source_time": 91.16501133786848,
        "correlation": 0.9983217187657746
      },
      {
        "source_time": 74.63167800453515,
        "correlation": 0.9980445869453768
      },
      {
        "source_time": 25.03167800453515,
        "correlation": 0.9977812064834123
      },
      {
        "source_time": 58.098344671201815,
        "correlation": 0.9977588982954985
      }
    ]
  },
  {
    "output_pts": 150,
    "video_frame": 484,
    "source_video_pts": 16.133333333333333,
    "source_audio_pts": 16.37074829931973,
    "audio_offset": 133.62925170068027,
    "video_offset": 133.86666666666667,
    "video_minus_audio_ms": 237.4149659864031,
    "correlation": 0.9990722398842461,
    "other_peaks": [
      {
        "source_time": 16.37074829931973,
        "correlation": 0.9990722398842461
      },
      {
        "source_time": 65.97074829931972,
        "correlation": 0.9981461147567534
      },
      {
        "source_time": 32.90408163265306,
        "correlation": 0.9981451511505056
      },
      {
        "source_time": 49.43741496598639,
        "correlation": 0.9981358482858077
      },
      {
        "source_time": 82.50408163265305,
        "correlation": 0.9981214466365738
      }
    ]
  },
  {
    "output_pts": 180,
    "video_frame": 1384,
    "source_video_pts": 46.13333333333333,
    "source_audio_pts": 46.37074829931973,
    "audio_offset": 133.62925170068027,
    "video_offset": 133.86666666666667,
    "video_minus_audio_ms": 237.4149659864031,
    "correlation": 0.9986650387767193,
    "other_peaks": [
      {
        "source_time": 46.37074829931973,
        "correlation": 0.9986650387767193
      },
      {
        "source_time": 79.43741496598639,
        "correlation": 0.9975556552691106
      },
      {
        "source_time": 62.90408163265306,
        "correlation": 0.9969608617040288
      },
      {
        "source_time": 95.97074829931972,
        "correlation": 0.9965428259771477
      },
      {
        "source_time": 13.30408163265306,
        "correlation": 0.9964041238195186
      }
    ]
  },
  {
    "output_pts": 210,
    "video_frame": 2284,
    "source_video_pts": 76.13333333333334,
    "source_audio_pts": 76.37074829931973,
    "audio_offset": 133.62925170068027,
    "video_offset": 133.86666666666667,
    "video_minus_audio_ms": 237.4149659864031,
    "correlation": 0.9933363517230118,
    "other_peaks": [
      {
        "source_time": 76.37074829931973,
        "correlation": 0.9933363517230118
      },
      {
        "source_time": 109.43741496598639,
        "correlation": 0.9849676558344929
      },
      {
        "source_time": 10.237414965986394,
        "correlation": 0.983436384439504
      },
      {
        "source_time": 92.90408163265306,
        "correlation": 0.9800438299109843
      },
      {
        "source_time": 26.770748299319727,
        "correlation": 0.9769875942775937
      }
    ]
  },
  {
    "output_pts": 240,
    "video_frame": 3184,
    "source_video_pts": 106.13333333333334,
    "source_audio_pts": 106.37074829931973,
    "audio_offset": 133.62925170068027,
    "video_offset": 133.86666666666667,
    "video_minus_audio_ms": 237.4149659864031,
    "correlation": 0.9986215001330787,
    "other_peaks": [
      {
        "source_time": 106.37074829931973,
        "correlation": 0.9986215001330787
      },
      {
        "source_time": 23.70408163265306,
        "correlation": 0.9973439459880562
      },
      {
        "source_time": 40.237414965986396,
        "correlation": 0.9973268888679171
      },
      {
        "source_time": 89.8374149659864,
        "correlation": 0.9973195308904754
      },
      {
        "source_time": 73.30408163265307,
        "correlation": 0.9973135107220236
      }
    ]
  }
]
### 原始运行诊断
[in @ 0000020c0039c000] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c0039c000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3656.001878
[in @ 0000020c55c42480] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c55c42480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 25425.356233
[in @ 0000020c0b202000] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c0b202000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28277.232144
[in @ 0000020c0dde7740] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c0dde7740] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 17702.491011
[in @ 0000020c55c23700] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c55c23700] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 17702.491011
[in @ 0000020c55c22a40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c55c22a40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 25425.356233
[in @ 0000020c00975440] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c00975440] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28277.665478
[in @ 0000020c00977640] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020c00977640] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3656.001878
[aac @ 0000020c0afa2f80] Qavg: 12775.353

[2026-11-16 17:57:03.812] [info] [graph][runtime.node] av_scheduler_video_progress_release node=94 generation=1 audio_sequence=12 audio_canonical_dispatch_ns=255419501 video_canonical_lower_bound_ns=266666666 observed_after_output_commit=1
[2026-11-16 18:01:43.959] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=30306 committed_datagrams=193227 committed_payload_bytes=231476792 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=46 ordered_queue_maximum_wire_bytes=62000 pacing_reserved=193227 pacing_cancelled=0 pacing_submitted=193227 pacing_maximum_submit_lateness_ns=6450260 pacing_worst_late_global_sequence=6 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=46 backlog_high_water_wire_bytes=62000 backlog_max_residence_ns=43098700 last_materialized_sequence=193227 last_scheduled_sequence=193227 last_submitted_sequence=193227 last_committed_sequence=193227 submitted_service_generation=1 submitted_service_sequence=193227 submitted_service_completed_ns=286363366900 submitted_service_duration_ns=16640 submitted_service_available_ns=286363383540 submitted_scope_available_ns=286363383540 shared_scope_charged_datagrams=193227 shared_scope_charged_wire_bytes=236887148 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554646 endpoint_1_target_effective_socket_bytes=277323 endpoint_1_api_requested_socket_bytes=277323 endpoint_1_effective_socket_bytes=277323 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=193157 endpoint_1_committed_payload_bytes=231472024 endpoint_2_target_effective_socket_bytes=277323 endpoint_2_api_requested_socket_bytes=277323 endpoint_2_effective_socket_bytes=277323 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=70 endpoint_2_committed_payload_bytes=4768
[CLI] final runtime report: queued=0, peakQueued=2040, threads=102, processThreads=170, workers=0, workerProgress=573092, workerProcessCalls=4310147, workerWaits=3737036, workerWakeups=3715831, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=836, logicalProcessors=22, averageSystemMachineCpuPercent=22.994252, averageProcessMachineCpuPercent=2.739781, peakProcessMachineCpuPercent=11.946050, averageProcessSingleCoreCpuPercent=60.275186, peakProcessSingleCoreCpuPercent=262.813102, initialWorkingSetBytes=235520000, workingSetBytes=296669184, peakWorkingSetBytes=302465024, totalPushed=510308, totalPopped=509983, droppedBuffers=0, encodedPacketsPushed=94034, encodedPacketsPopped=94034, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=20770272, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1638, graphPayloadReservations=323596, graphPayloadReleases=323596, graphPayloadPressureFailures=0, lastInputReceivedAtNs=33543877342500
### 视频实际PTS证据
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   0 pts:  90000 pts_time:1       duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:0 type:P checksum:EFAA7F80 plane_checksum:[5467EFDA 1CCF5F1B DC56307C] mean:[60 137 124] stdev:[34.2 4.2 3.4]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   1 pts: 900000 pts_time:10      duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:F71EFB95 plane_checksum:[9B334A45 5A6D6A76 33A546DA] mean:[57 137 124] stdev:[33.2 4.7 2.8]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   2 pts:2700000 pts_time:30      duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:013418C4 plane_checksum:[01078BA4 6549ED4D F7839FB5] mean:[64 140 121] stdev:[34.2 6.0 6.0]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   3 pts:5400000 pts_time:60      duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:8DADF035 plane_checksum:[13824361 357C640F B2D348C5] mean:[57 137 125] stdev:[34.0 4.6 3.0]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   4 pts:8100000 pts_time:90      duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:8E5569DF plane_checksum:[9270C11B DF497AAA BBBA2E0B] mean:[58 137 124] stdev:[33.5 4.4 2.8]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   5 pts:9450000 pts_time:105     duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:0 type:P checksum:0D8B87C5 plane_checksum:[B9B5C1E1 C94AE794 63ECDE32] mean:[23 128 128] stdev:[34.9 0.7 0.4]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   6 pts:13500000 pts_time:150     duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:F419DA3C plane_checksum:[C17B56FF AB71F213 748D911B] mean:[63 140 120] stdev:[34.6 6.6 6.3]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   7 pts:16200000 pts_time:180     duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:94FAC16D plane_checksum:[5C8724E9 A7216365 AEDF391F] mean:[57 137 124] stdev:[34.3 4.6 3.2]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   8 pts:18900000 pts_time:210     duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:D51761F1 plane_checksum:[E7B1B605 C9257B33 961C30AA] mean:[58 137 124] stdev:[33.8 4.7 3.3]
[Parsed_showinfo_2 @ 0000027c41b2f7c0] n:   9 pts:21600000 pts_time:240     duration:   3000 duration_time:0.0333333 fmt:yuv420p cl:left sar:1/1 s:640x70 i:P iskey:1 type:I checksum:61A710B6 plane_checksum:[87BA9E80 1C37E4F1 56198D27] mean:[64 140 120] stdev:[35.8 6.8 6.3]
### 本轮清理清单
composition-r451-audio-map.json
composition-r451-capture.log
composition-r451-capture.out
composition-r451-cli-error.log
composition-r451-cli.log
composition-r451-content-result.json
composition-r451-marker-01.png
composition-r451-marker-02.png
composition-r451-marker-03.png
composition-r451-marker-04.png
composition-r451-marker-05.png
composition-r451-marker-06.png
composition-r451-marker-07.png
composition-r451-marker-08.png
composition-r451-marker-09.png
composition-r451-marker-10.png
composition-r451-native-2026-11-16-17h57m21s400.png
composition-r451-native-2026-11-16-17h59m03s136.png
composition-r451-native-2026-11-16-18h00m03s781.png
composition-r451-native-2026-11-16-18h01m36s197.png
composition-r451-output.pcapng
composition-r451-output.sdp
composition-r451-output.ts
composition-r451-received-audio.log
composition-r451-received.f32
composition-r451-reentry.log
composition-r451-reentry.out
composition-r451-reference-audio.log
composition-r451-reference.f32
composition-r451-source0.log
composition-r451-source0.out
composition-r451-source1.log
composition-r451-source1.out
composition-r451-source2.log
composition-r451-source2.out
composition-r451-source3.log
composition-r451-source3.out
composition-r451-video-analysis.log
composition-r451-vlc.log
composition-r451-wire.json

独立公式复核PASS（非内容门禁）：尤其105秒最高/次峰相关差约0.000135，不能假定唯一。下一轮同捕61020/61021/61022/61023和62024/62025，覆盖SR/SDES/BYE并分SSRC代次；依据RFC3550 6.4.1，将AU的RTP时戳用各自SR映射共同NTP，比较输入内容相对偏差B与输出候选偏差Δ，才可评估引擎新增量Δ−B。不得以包到达时间代替媒体时间或添加补偿常数。

清理完成：40项本轮产物按清单逐项删除，8个精确PID提升权限核查均无残留。指定120秒源、正式构建和无关文件保留。归档边界独立复审PASS；本轮不是内容验收成功提交。
