# CLI与库的profile使用

使用同包CLI、头文件和库：`/home/tang/packages/media-transcode-beta-rkmpp-475eb8e2-20260910/`。旧f41a6554包只有库支持profile，CLI不能接收--profile。新包的初始CLI、动态add、Beta初始输出及动态输出均进入同一planner/open contract。

## 环境和输入

RKMPP终端先执行：

```bash
source /opt/mt-tools/mtenv.sh
mtenv on
ffenv on
```

运行库检查（必须在运行CLI的同一终端执行）：

```bash
ldd /home/tang/packages/media-transcode-beta-rkmpp-475eb8e2-20260910/bin/media_transcode_realtime_video_cli | grep -E 'libav(codec|filter|util)'
```

三项应解析到 `/home/tang/ffmpeg-ab1e61a/lib/`。目标机的 `ffenv on` 已按用户授权统一选择该修复版本，调用方无需额外export。已有终端先执行 `source /etc/profile.d/ffenv.sh` 重新加载函数，再执行 `ffenv on`；新登录终端直接启用即可。构建禁用RPATH/RUNPATH，实际依赖以CLI的 `ldd` 为准。

先在Windows的四个终端分别执行以下命令，使用默认硬件解码。然后启动RKMPP CLI，在30秒打开超时内启动源流。动态命令应在120秒源流结束前执行。

```powershell
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --network-caching=1000 rtp://@:6200
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --network-caching=1000 rtp://@:6202
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --network-caching=1000 rtp://@:6204
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --network-caching=1000 rtp://@:6206
```

## CLI初始输出

H264 Baseline、1920×1080@25、CBR6Mbps、GOP50，RTP输入→MPEG-TS/RTP输出：

```bash
/home/tang/packages/media-transcode-beta-rkmpp-475eb8e2-20260910/bin/media_transcode_realtime_video_cli \
  --media-id ffenv-profile-rk-03 \
  --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 \
  --input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 \
  --analyze-duration-us 5000000 --probe-size 5000000 \
  --video-rtp-url rtp://192.168.130.229:61894 \
  --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 \
  --output-layout mpegts --output-transport rtp \
  --rtp-host 192.168.96.122 --rtp-port 6200 --sdp /home/tang/ffenv-profile-rk-03-initial.sdp \
  --video-codec h264 --profile baseline \
  --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
```

另一个RKMPP终端发送固定连续120秒H264源：

```bash
ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://192.168.130.229:61894?rtcpport=61895&pkt_size=1200'
```

## CLI动态增加及删除

在正在运行的CLI终端键入完整一行。同配置H264 Baseline输出可以复用编码组：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6202 --sdp /home/tang/ffenv-profile-rk-03-add.sdp --video-codec h264 --profile baseline --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

独立HEVC Main输出：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6204 --sdp /home/tang/ffenv-profile-rk-03-third.sdp --video-codec hevc --profile main --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
list
```

等待每个output_id进入running。按实际返回ID删除，例如本例前三路：

```text
remove 1
remove 2
remove 3
```

等待retired后，增加H264 High输出：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6206 --sdp /home/tang/ffenv-profile-rk-03-readd.sdp --video-codec h264 --profile high --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

可选的错误组合验证（应进入failed，且不影响已有输出）：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6208 --sdp /home/tang/ffenv-profile-rk-03-invalid.sdp --video-codec hevc --profile baseline --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

add_admitted只代表受理；以running/failed判定准备结果。完整转码配置由每条add明确传入；RTCP占用RTP端口+1。无约束时可不传--profile，所选编码器的最终profile需以实际码流判断。

## 库入口

沿用同包examples/beta_minimal.c的完整输入/输出配置，初始与动态字段分别为：

```c
config.output.destination_address = "192.168.96.122";
config.output.destination_port = 6200;
config.output.codec = MT_BETA_VIDEO_CODEC_HEVC;
config.output.width = 1920;
config.output.height = 1080;
config.output.frame_rate_num = 25;
config.output.profile = "main"; /* 其余字段按完整示例填写，完成后再调用start。 */

mt_beta_video_output another = {0};
another.protocol = MT_BETA_OUTPUT_MPEGTS_RTP;
another.destination_address = "192.168.96.122";
another.destination_port = 6204;
another.codec = MT_BETA_VIDEO_CODEC_H264;
another.profile = "baseline";
another.width = 1920;
another.height = 1080;
another.frame_rate_num = 25;
another.frame_rate_den = 1;
another.gop_frames = 50;
another.rate_control_mode = MT_BETA_RATE_CONTROL_CBR;
another.rate_control.cbr.bitrate_bps = UINT64_C(6000000);
uint64_t output_id = 0;
mt_beta_status status = mt_beta_realtime_add_output(session, &another, &output_id);
/* status为OK后保存ID，等待RUNNING；之后按ID调用remove_output。 */
```

库字段NULL/空串表示无调用方约束；字符串在start/add返回前由库复制。使用同包头文件重编，不能与旧结构二进制混用。

目标h264_rkmpp公开baseline/main/high，hevc_rkmpp公开main。错误组合（如HEVC baseline）在准备阶段拒绝；不以自动转换掩盖不支持。来源：[目标RKMPP AVOption](https://raw.githubusercontent.com/nyanmisaka/ffmpeg-rockchip/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf/libavcodec/rkmppenc.h)。

让120秒源自然结束，裸RTP停源后CLI按现有NoProgress语义可能退出1；不要把它伪装成文件EOS。测试结束删除本例SDP、日志、抓包和截图。本包对应的2026-09-10完整120秒实测（ffenv-profile-rk-03）通过本轮三个门禁：发送无超约定突发、动态增删、VLC正常解码。初始H264 Baseline 6200，增加同配置Baseline 6202和HEVC Main 6204；拒绝HEVC Baseline 6208后，删除前三路至零，再恢复H264 High 6206。所有输出均为1920×1080、25fps、CBR6Mbps、GOP50；输入H264 RTP为1280×720、30fps、约8.013Mbps，固定120秒源SHA256为6e0de760672a6b8386b59b22f824de442e2a5ef7594d7b872c6851b64cabca9b。实际CLI/FFmpeg/VLC命令如上；本轮使用61894/61895以避开192.168.130.231向61884发送的外部流，未降低媒体参数。脚本PID456803、CLI456808、源456828、发送抓包456807；Windows VLC PID22840/29092/34260/36456、dumpcap17984。实际控制过程包含等待接收端RTP/RTCP端口及抓包就绪，再启动源流。

发送与接收均为89141包、113422640 IP字节，RTP序号错误0、TS错误0。50Mbps服务曲线最大超额：发送1356字节（门限1356，通过），接收10399.875字节；接收聚集不等同于发送突发。发送器would_block、deadline_misses、pressure_failures、partial/ambiguous失败均为0，最终队列为0。该轮通过不证明此前另一轮启动聚集的根因已修复。

实际参数集：6200/6202为H264 profile_idc=66、constraint_set0/1=1、CAVLC（Constrained Baseline）；6204为HEVC Main、8bit；6206为H264 profile_idc=100、CABAC（High）。四路VLC使用D3D11VA，解码帧1092/992/698/4272、显示534/483/336/2123，丢帧/损坏/不连续均0，四张1920×1080画面已检查正常。

497次CPU采样，进程单核口径均值16.620377%、峰值47.761194%；工作集62529536→81870848字节，峰值81870848。运行时errors/workerErrors/droppedBuffers均0，最终图载荷为0，reservation/release为69502/69502。只测视频，A/V漂移不适用。源自然完成exit0；CLI在停源后按NoProgress退出1；动态控制及发送抓包exit0，发送抓包内核丢包0。此前01/02轮受61884外部流干扰，分别在输入观测/源身份校验失败，不记为通过。

2026-09-14复核包内文件哈希及运行依赖：库与CLI仍为实测产物；CLI和C示例无RPATH/RUNPATH，ffenv on选择/home/tang/ffmpeg-ab1e61a。静态库SHA256为578c88714e466fc47320046a5c9b6204da012ac1cd5caf79c8c9e72c0b73d205；CLI为3bd0068a465c152c833ee305a768fbd2f6cd7d32f53bee220508e01404f5a2c1。这里只复核既有完整测试，没有声称9月14日重跑媒体链路。