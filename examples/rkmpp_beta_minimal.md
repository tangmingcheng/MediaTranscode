# RKMPP 动态输出 C 示例

基于旧包 `media-transcode-beta-rkmpp-ba190e3c-20260908/examples/beta_minimal.c` 的配置、事件回调及快照循环更新，源码为 `rkmpp_beta_minimal.c`；发布包内命名为 `examples/beta_minimal.c`。须使用同包新版头文件及库，旧结构字段不再适用。

示例配置沿用原部署事实：H264 RTP 输入 `192.168.130.229:61884`，PT96、90kHz；初始 HEVC MPEG-TS/RTP 发往 `192.168.96.122:6200`，1920×1080、25fps、CBR6Mbps、GOP50，出口50Mbps、最大wire residence100ms。按实际部署修改这些已有配置字段，库内部时序与容量仍由planner推导。

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
add hevc 6202
add h264 6204
list
remove 1
remove 2
remove 3
add hevc 6206
list
```

上面的1/2/3仅是新会话的演示ID，实际操作须使用事件、add返回值或list给出的output_id。先等待初始输出及每次新增的 `output_state=3`（RUNNING），再进行下一项；删除后等待 `output_state=5`（RETIRED）。同完整编码配置的HEVC输出复用编码组；H264建立独立组。全部输出退役后可重新添加，共享输入继续运行。

`status=0`只表示命令受理，不代表输出已经出流或退役；失败状态及错误会保留打印。RTCP占用RTP端口+1，因此各路端口不得重叠。快照集合持有的字符串只在集合释放前使用，回调字符串只在回调期间有效。

EOF不停止媒体会话；验收由120秒源自然结束驱动。裸RTP没有有限文件EOS，停源后现有无输入策略可能报告SOURCE_LOSS并退出1，不能伪装为正常EOS。手动Ctrl+C/SIGTERM通过主线程请求停止，release始终位于回调之外。示例只演示已有视频API，不增加音频或新的生产参数。
