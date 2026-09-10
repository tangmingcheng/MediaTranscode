# RKMPP 动态输出 C 示例

基于旧包 `media-transcode-beta-rkmpp-ba190e3c-20260908/examples/beta_minimal.c` 的配置、事件回调及快照循环更新，源码为 `rkmpp_beta_minimal.c`；发布包内命名为 `examples/beta_minimal.c`。须使用同包新版头文件及库，旧结构字段不再适用。新版mt_beta_video_output增加profile指针字段，调用方必须使用同包头文件重新编译，不能混用旧二进制结构。

示例配置沿用原部署事实：H264 RTP 输入 `192.168.130.229:61884`，PT96、90kHz；初始 HEVC MPEG-TS/RTP 发往 `192.168.96.122:6200`，1920×1080、25fps、CBR6Mbps、GOP50，出口50Mbps、最大wire residence100ms。按实际部署修改这些已有配置字段，库内部时序与容量仍由planner推导。

## 先看这三步

main中直接使用config.output.destination_address、config.output.codec等字段设置初始输出。源码开头的add_video_output同样逐字段设置另一路的完整配置，然后直接调用动态增加API；后面的stdin解析仅用于交互演示。不使用配置工厂函数，也不隐藏每路转码配置。

在已有session运行时增加另一路H264 Baseline输出，核心代码是：

```c
mt_beta_video_output another = {0};
another.protocol = MT_BETA_OUTPUT_MPEGTS_RTP;
another.destination_address = "192.168.96.122";
another.destination_port = 6204;
another.codec = MT_BETA_VIDEO_CODEC_H264;
another.profile = "baseline";
/* another是独立配置；可在这里设置这一路自己的转码目标。 */
another.width = 1920;
another.height = 1080;
another.frame_rate_num = 25;
another.frame_rate_den = 1;
another.gop_frames = 50;
another.rate_control_mode = MT_BETA_RATE_CONTROL_CBR;
another.rate_control.cbr.bitrate_bps = UINT64_C(6000000);
uint64_t another_id = 0;
mt_beta_status status = mt_beta_realtime_add_output(session, &another, &another_id);
/* status == MT_BETA_STATUS_OK后保存another_id，等待对应RUNNING事件。 */
```

本演示先等待RUNNING，后续不需要该路时使用保存的ID删除：

```c
status = mt_beta_realtime_remove_output(session, another_id);
/* 等待对应RETIRED事件；不停止session和其他输出。 */
```

输入与部署配置属于session，不必为新增一路再次start。动态输出的转码配置独立提交；完整编码契约相同时库内部自动复用编码组。

## 编译与运行

在 RKMPP 目标机进入包目录后执行（包依赖已安装的指定FFmpeg、MPP/RGA及编译环境，不是独立于系统的全静态包）：

```bash
source /opt/mt-tools/mtenv.sh
mtenv on
ffenv on
export PKG_CONFIG_PATH=/home/tang/ffmpeg-ab1e61a/lib/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=/home/tang/ffmpeg-ab1e61a/lib:$LD_LIBRARY_PATH
gcc -std=c11 -Wall -Wextra -Werror -Iinclude -c examples/beta_minimal.c -o /home/tang/beta_minimal.o
g++ /home/tang/beta_minimal.o lib/libmedia_transcode_beta.a $(pkg-config --libs libavfilter libavcodec libavformat libavutil libswscale libswresample) -pthread -ldl -o bin/beta_minimal
rm /home/tang/beta_minimal.o
./bin/beta_minimal
```

先启动示例，再发送真实源。固定120秒源命令：

```bash
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://192.168.130.229:61884?rtcpport=61885&pkt_size=1200'
```

Windows VLC 使用默认硬解，分别打开 `rtp://@:6200`、`rtp://@:6202`、`rtp://@:6204`、`rtp://@:6206`。

## 动态增删

在示例标准输入键入完整行：

```text
list
add hevc 6202 main
add h264 6204 baseline
list
remove 1
remove 2
remove 3
add h264 6206 high
list
```

上面的1/2/3仅是新会话的演示ID，实际操作须使用事件、add返回值或list给出的output_id。先等待初始输出及每次新增的 `output_state=3`（RUNNING），再进行下一项；删除后等待 `output_state=5`（RETIRED）。同完整编码配置的HEVC输出复用编码组；H264建立独立组。全部输出退役后示例新增H264 High输出，共享输入继续运行。

`status=0`只表示命令受理，不代表输出已经出流或退役；失败状态及错误会保留打印。RTCP占用RTP端口+1，因此各路端口不得重叠。快照集合持有的字符串只在集合释放前使用，回调字符串只在回调期间有效。

EOF不停止媒体会话；验收由120秒源自然结束驱动。裸RTP没有有限文件EOS，停源后现有无输入策略可能报告SOURCE_LOSS并退出1，不能伪装为正常EOS。手动Ctrl+C/SIGTERM通过主线程请求停止，release始终位于回调之外。本次仅开放用户明确授权的profile字段，不增加音频或其他生产参数。

## Profile设置

初始输出设置config.output.profile，动态输出设置传给mt_beta_realtime_add_output的output.profile。字符串在start/add返回前复制；NULL或空串表示调用方不约束profile，mapper不填默认值。字段直接进入既有planner/open/readback及完整编码组契约，不建立额外链路。按用户指定恢复config.output成员名，字段位置和结构布局不变，不保留initial_output别名。

目标FFmpeg ab1e61a的h264_rkmpp支持baseline/main/high，hevc_rkmpp公开main；不同后端的可用值以编码器能力为准，不能推断通用main10支持。示例要求每次add显式传profile，编码器不支持的值在准备阶段报告失败；add返回0本身不代表请求达成，须观察输出状态并核对实际参数集。

## 同包 realtime CLI 的 profile

新版CLI初始输出以及标准输入的add命令均可使用--profile，例如--video-codec hevc --profile main，或--video-codec h264 --profile baseline。未传--profile表示调用方不约束，和库的NULL/空串语义一致；实际支持值由所选编码器在准备阶段校验。库和CLI共用实时请求、planner及编码组契约。
