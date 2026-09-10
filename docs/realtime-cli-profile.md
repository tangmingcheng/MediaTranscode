# CLI与库的profile使用

使用同包CLI、头文件和库：`/home/tang/packages/media-transcode-beta-rkmpp-038f069c-20260910/`。旧f41a6554包只有库支持profile，CLI不能接收--profile。新包的初始CLI、动态add、Beta初始输出及动态输出均进入同一planner/open contract。

## 环境和输入

RKMPP终端先执行：

```bash
source /opt/mt-tools/mtenv.sh
mtenv on
ffenv on
export PATH=/home/tang/ffmpeg-ab1e61a/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/ffmpeg-ab1e61a/lib:$LD_LIBRARY_PATH
```

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
/home/tang/packages/media-transcode-beta-rkmpp-038f069c-20260910/bin/media_transcode_realtime_video_cli \
  --media-id cli-profile-rk-02 \
  --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 \
  --input-type rtp --open-timeout-ms 30000 --read-timeout-ms 2000 \
  --analyze-duration-us 5000000 --probe-size 5000000 \
  --video-rtp-url rtp://192.168.130.229:61884 \
  --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 \
  --output-layout mpegts --output-transport rtp \
  --rtp-host 192.168.96.122 --rtp-port 6200 --sdp /home/tang/cli-profile-rk-02-initial.sdp \
  --video-codec h264 --profile baseline \
  --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
```

另一个RKMPP终端发送固定连续120秒H264源：

```bash
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://192.168.130.229:61884?rtcpport=61885&pkt_size=1200'
```

## CLI动态增加及删除

在正在运行的CLI终端键入完整一行。同配置H264 Baseline输出可以复用编码组：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6202 --sdp /home/tang/cli-profile-rk-02-add.sdp --video-codec h264 --profile baseline --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

独立HEVC Main输出：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6204 --sdp /home/tang/cli-profile-rk-02-third.sdp --video-codec hevc --profile main --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
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
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6206 --sdp /home/tang/cli-profile-rk-02-readd.sdp --video-codec h264 --profile high --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

可选的错误组合验证（应进入failed，且不影响已有输出）：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6208 --sdp /home/tang/cli-profile-rk-02-invalid.sdp --video-codec hevc --profile baseline --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
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

让120秒源自然结束，裸RTP停源后CLI按现有NoProgress语义可能退出1；不要把它伪装成文件EOS。测试结束删除本例SDP、日志、抓包和截图。本包当前验证结果：全量编译及包内校验通过；真实码流确认H264 Baseline为profile_idc=66、CAVLC，H264 High为100、CABAC，HEVC Main为1；动态增删及VLC解码通过。发送抓包在启动阶段出现超出50Mbps服务曲线的聚集（75085.25字节，门限1356字节），该项尚未通过。本包按用户要求提前交付，不代表完整验收通过。

