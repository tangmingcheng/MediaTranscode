# 接收端内容同步：可识别源时间补充测量

## 问题与适用边界

r437原未标记源存在248帧重复周期；视频匹配有多个候选，不能按音频偏移挑最近峰再声称音画同步，也不能把15.692ms候选或内部phase当完整结论。原规格Windows r449、RKMPP rk450功能回归保留。

补充测量继续使用指定120秒源、四路、1280×720/30fps和既有码率/协议，仅选中source2发送端给视频烧录原帧号/PTS，音频copy不变；视频需要重新编码并明确记录，因此这是带标记补充，不是未标记输入逐字节复验。不修改生产核心或Windows FFmpeg。

## 方法依据与结果判据

采用[FFmpeg drawtext](https://ffmpeg.org/ffmpeg-filters.html#drawtext)可见源帧号/PTS，按[FFmpeg ffprobe](https://ffmpeg.org/ffprobe.html)提取接收原TS帧时间戳，音频继续使用[Scipy correlate](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.correlate.html)归一化相关并保留其他峰。[ITU-R BT.1359](https://www.itu.int/rec/R-REC-BT.1359)作为音画相对时序背景，不自动将其广播阈值引入项目验收。

真实链路保持FFmpeg→realtime CLI→VLC，dumpcap仅旁路捕获。先核抓包丢失、RTP序号、TS连续计数、解码与PTS完整性；保留原输出时间轴，不用VLC重封装后时间戳。视频位移=输出视频PTS−画面标记源PTS；音频位移=输出音频窗口PTS−独立相关得到的源时间；两者之差是接收内容相对偏移。对起始、稳态、失源前、重入分别取证，并列出30fps量化/窗口匹配不确定度。不以稀疏点声称p95/p99，不以120秒声称小时级漂移，也不将TS内容时序等同显示器/扬声器物理同步。原计划未明确的阈值不得自行增加。

## 已执行工具能力检查（非合屏验收）

以下仅单帧验证标记字体及转义，不降低真实链路规格，也不作为功能通过。exit0，查看输出确认frame=0、pts=0.000000可读。临时PNG已在记录后删除。

```powershell
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -y -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -vf "drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='frame=%{frame_num} pts=%{pts}':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -frames:v 1 -update 1 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r451-marker-probe.png'
```

本机dumpcap提升权限枚举确认Loopback接口11，物理以太网接口7；每轮启动前仍须重新核对接口。普通权限只枚举Loopback，不能据此认为不存在物理接口。

状态：测量方法及工具能力已核；真实带标记接收测量尚未执行，内容同步门禁未通过。

## 历史数值门禁定位

数值有真实来源：docs/av_sync_system_design.md 的 Separate RTP to Separate RTP 段、docs/superpowers/specs/2026-07-15-av-sync-production-wiring-design.md 均列出 flash/beep startup≤40ms、steady P95≤20ms/P99≤40ms及1ms/hour等要求。当前未找到原合屏计划明确继承这些数值的语句；本次源也不是flash/beep。应保留历史目标与当前取证的差别，不自动新增或宣称通过该统计门禁。若后续整体审查发现明确继承证据，按该证据恢复相应要求。
