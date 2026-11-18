# RKMPP 四路合屏 C 调用示例

`rkmpp_composition.c`直接调用公共 C API，不调用 CLI，也不创建第二套媒体管线。示例已在RKMPP目标机以C11 -Wall -Wextra -Werror编译并与当前三静态库链接成功；完整实流及最终包验收仍待完成。

使用同一包的头文件和静态库，按现场修改 main 中的显式配置；这些数值是演示部署，不是库默认值。示例接受两个地址：本机 RTP 绑定地址、接收端输出地址，例如 `./bin/rkmpp_composition 192.168.130.229 192.168.96.122`。端口及其他字段在 main 中逐项可见，改后重新编译。不使用不同版本的头文件或 archive 混搭。

## 调用顺序与线程

1. 填写每源 video、可选 audio 和 input_latency_ms，再填写合屏、输出、部署配置。
2. `mt_beta_realtime_start_composition`返回 OK 表示会话已创建，不表示媒体准备或输出已成功；持续检查事件及 snapshot。
3. 事件回调只打印，不持有 event 字符串。输入数组及字符串在 start 返回前复制；user_data 仍由调用方持有至 release 完成。
4. 主线程定期 get_snapshot。SIGINT/SIGTERM处理器仅置sig_atomic_t标志，主线程调用request_stop；信号处理器不调用库。
5. 观察 COMPLETED/FAILED 后在回调外 release，并保证没有其他并发 API 调用。release 将句柄置空。失败也必须 release。

全失源时合屏继续输出黑场/静音，不能把源结束当作会话 EOF；由调用方明确停止。所选源的音视频按组重新取得时钟，单独丢音频可能使该源视频格也进入黑场，其他源应继续。输入布局由数组顺序决定：双源左右，三/四源2×2按行排列，三源剩余格黑。

## 每源参数：来自真实 RTP/RTCP 会话

| 字段 | 含义与单位 | 示例/来源 |
|---|---|---|
| video.bind_address | 本机接收网卡地址，不是发送方地址 | argv[1]，按路可不同 |
| video.port | 正偶数RTP端口，RTCP使用相邻port+1 | 61030、61040、61020、61050；避免端口冲突 |
| video.codec | 输入实际编码，H264或HEVC枚举 | 第四路HEVC，其余H264 |
| video.payload_type | RTP PT（0–127），必须与发送端/SDP一致 | 96 |
| video.clock_rate | RTP时钟频率Hz，不是帧率 | 视频90000 |
| video.fmtp | 会话提供的格式参数字符串 | 本示例NULL，发送端在带内提供参数集；不能任意省略实际所需参数 |
| audio | 指向完整音频协议事实；无音频为NULL | 仅inputs[2]声明音频 |
| audio.bind_address/port | 本机音频RTP接收地址/端口 | 61022，RTCP61023 |
| audio.codec_name | 实际音频编码名称 | `aac` |
| audio.payload_type | 音频PT | 97 |
| audio.clock_rate/channels | 会话音频时钟Hz/声道数 | 44100/2 |
| audio.fmtp | 实际会话的AAC-hbr配置，包括AudioSpecificConfig | 示例config=1210，仅适用该44100Hz双声道AAC源；不得对其他源照抄 |
| input_latency_ms | 部署允许该源迟到/乱序等待的上限ms，必须正值 | 分别100/150/200/250，可逐源设置；不是自动测出的网络时延 |

## 合屏与输出参数

| 字段 | 含义与约束 | 示例 |
|---|---|---|
| media_id | 会话标识，便于关联诊断 | four-source-composition |
| inputs/input_count | 输入数组/源数 | 2–4，本例4 |
| grid | 固定网格布局枚举 | MT_BETA_COMPOSITION_GRID_FIXED |
| audio_source | 所选音源的零基数组索引，必须有完整audio声明 | 2，不混音 |
| audio_bitrate_bps | 正的音频目标bit/s，必须能精确表示为整数kbps | 192000；不是音频CBR保证 |
| preparation_storage_limit_bytes | 全部输入共享的引擎准备存储预算byte | 2147483648；不是进程RSS或GPU/驱动内存上限 |
| output.protocol | 输出封装/传输枚举 | 本例MPEGTS_RTP；MPEGTS_UDP等其他枚举须以对应链路证据为准 |
| destination_address/port | 接收端地址/RTP端口 | argv[2]:62024，RTCP62025 |
| output.codec | 输出视频编码目标 | H264；可按目标改为HEVC |
| output.profile | 编码profile约束 | NULL表示调用方不约束；不是保证选定某profile，支持值取决于编码器 |
| width/height | 完整输出画布像素尺寸，不是每格尺寸 | 1280×720 |
| frame_rate_num/den | 输出帧率有理数 | 30/1 |
| gop_frames | 输出GOP帧数 | 60 |
| rate_control_mode | CBR或VBR | VBR |
| rate_control.vbr.target/min/max_bitrate_bps | VBR目标/最低/最高bit/s | 8000000/8000000/12000000 |
| rate_control.cbr.bitrate_bps | 仅选择CBR时填写该union分支 | 例如8000000；不要同时填VBR分支 |
| deployment.provisioned_egress_capacity_bps | 部署提供给会话的出口服务容量bit/s | 50000000，不是根据目标码率随意乘系数 |
| deployment.maximum_wire_residence_ms | 允许的出口最大驻留预算ms | 100，不是输入乱序预算或端到端时延保证 |

不要求调用方传内部队列容量、编码器延迟、封装包数或时钟策略。未暴露的音频输出编码、采样率及声道等由当前准备链路决定并校验；不能把示例实测的AAC44100Hz双声道写成适用所有源的公共保证。参数合法不等于目标硬件一定支持，必须处理异步准备失败。

## 运行结果的含义

事件保留error_code、failure_stage、native_code、detail。snapshot的selected_backend是现有会话报告口径，不代表每个源；异构视频input_codec为UNKNOWN，准备完成前同样可能UNKNOWN。CPU machine percent按整机核数归一，working_set_bytes为进程RSS口径；公共snapshot没有完整准备存储账本或内容级音画同步证明。输出SDP路径由会话持有，输出退役/release后可能删除，要留存需在此之前自行复制。

## 编译

在最终交付包中使用随包的编译说明和已核验依赖路径。库为aarch64 Linux静态业务库，仍依赖匹配的FFmpeg、MPP、RGA、C++运行时及RKMPP设备权限；不是包含驱动的全静态包。以C11编译示例，再用C++链接器链接合并库、FFmpeg六库、RGA和pthread；不得使用过期示例可执行文件代替重新链接。

示例回调的fprintf及主循环日志为可读性服务，慢终端/管道可能阻塞；生产调用方应按自己的有界日志策略处理，不能据示例宣称回调无阻塞。
