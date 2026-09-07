# RKMPP 输入异常证据

## 判定

第 18 次测试没有修改核心代码，二进制仍为
`96c5c71d3183cfe11846dba36ee48f4116983c775e7caa1b3eb5078188989b57`。

已确认的直接事实是：从 `192.168.130.161` 到目标机
`192.168.130.229:61884` 的 H.264 RTP 在进入 MediaTranscode 前已出现
长时间无包和分片丢失。证据不能进一步区分源服务 RTP 发送端和中间网络，
因此不把问题单独归责给源服务器，也不修改 MediaTranscode 核心。

## 同时段证据

测试原参数运行；源启动时间 `2026-09-04 16:55:45.045 +08:00`，
主动停止源流时间 `16:59:30.871`。CLI PID `1250774`、目标机抓包 PID
`1250757`、driver PID `1250754`、源状态采样 PID `1251153`。

目标机 `eth0` 抓包中，输入 RTP 只有一个流：
`192.168.130.161:<动态端口> -> 192.168.130.229:61884`、
SSRC `3`、PT `96`。输入共 `81871` 包，持续
`225.809815 s`，最大无包间隔 `2516.536 ms`。

一个可重复核对的窗口如下：

| 事实 | 值 |
|---|---|
| 目标机入口无 RTP 的时间 | `16:57:27.786 +08:00` 至 `16:57:30.264 +08:00` |
| 无包时长 | `2478.929 ms` |
| 同窗口源服务视频帧计数 | `2671632 -> 2671658 -> 2671683` |
| 源服务采样跨度 | `2010.226 ms`，增加 `51` 帧 |
| 三次 API 响应耗时 | `3.931 ms`、`3.958 ms`、`4.039 ms` |

这证明源服务内部视频轨仍以约 25 fps 前进时，目标机入口可以连续约
2.48 秒没有收到该 RTP 流。

完整输入抓包另发现 `21` 个序号缺口，全部为从 `N` 跳到 `N+2`，
没有对应迟到或乱序包。例如 `16:56:08.488555 +08:00`，同一
RTP timestamp `1020801898`、同一 H.264 FU-A IDR 分片从序号
`8731` 跳到 `8733`，序号 `8732` 未出现。缺失的是实际编码分片，
不是仅缺少统计或控制包。

同时段排除项：

- tcpdump：`223275 packets captured`、`223275 packets received by filter`、
  `0 packets dropped by kernel`。
- `ip -s link` 的 RX errors/dropped/overrun 前后均为 `0/3/0`，
  测试期间没有增长。
- 网卡 `rx_missed_cntr`、`rx_overflow_cntr`、CRC、RX FIFO/GMAC
  overflow、RX PAUSE 和 TX underflow 前后差值均为 `0`。

因此本次缺包不支持“MediaTranscode UDP 接收后才丢失”或“目标机抓包
进程丢包”这两个原因。确定最终责任点仍需要在 `192.168.130.161`
的 RTP 发出接口，以及必要时中间交换/路由设备上，对同一 SSRC 和序号
做同时段抓包。

## 核心运行结果

尽管输入存在上述异常，RKMPP 输出仍持续 `224.836184 s`，RTP
`141354` 包、RTCP `50` 包。源停止前 workerErrors、errors、
droppedBuffers 和 deadline_misses 均为 `0`；发送端 50 Mbps
服务曲线超额为一个最大数据报 `1356 B`，最大 wire residence
`44.534952 ms`。

该结果证明当前核心遇到输入空档和不完整 FU-A 后不会再退出，并会在有效
输入恢复后继续转码；它不证明输入连续，也不能把完整验收标记为 PASS。

## 证据文件

本地目录：
`out/acceptance/rk-a559-external/input-source-evidence-run18/`

- `input-gap-165727.pcap`：上述无包窗口及前后数据，SHA256
  `e8230850f08388e1356703293944aa538a4ca9afd7e5b818ee636ed4f74526b4`。
- `source-samples-gap.json`：同一窗口的源服务帧计数。
- `input-sequence-evidence.json`：全部输入序号缺口及分片字段。
- `source-gap-correlation.json`：目标机无包窗口与源服务计数的时间关联。
- `tcpdump.log`、`nic-before.txt`、`nic-after.txt`、
  `link-before.txt`、`link-after.txt`：抓包及网卡排除证据。
- `cli.log`、`resources.txt`、`packet-analysis.json`：核心运行证据。

完整目标机抓包保留在
`/home/tang/MediaTranscode/out/acceptance/rk-a559-external-run18/traffic.pcap`，
SHA256 为
`34e1aa720f9384516ba910b06891a46b6c4ef76a6c800681282641bd362f5b5c`。

