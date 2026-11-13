# RKMPP 身份传播适配实施与回退

## 授权与目标

2026-11-13用户授权直接解决目标机依赖阻塞，必须保留回退手段并及时本机提交。Windows验证正常，因此本机FFmpeg既不修改源码，也不因本次RKMPP适配顺带更新。目标机可在隔离副本修改MPP/FFmpeg。只开发和本地/目标机验证，不推送、合并或系统部署。

目标仍为原2–4路RTP/RTCP固定合屏、选定音频、失源黑屏/静音和恢复；先解决rk409的解码身份及持有合同，不删除门禁、不用FIFO或近似PTS猜配。官方调查见[上游调查](realtime-composition-rkmpp-upstream-investigation.md)。

## 已建立的隔离与回退基线

目标项目：/home/tang/MediaTranscode-composition-09093919。

| 对象 | 原位置/身份 | 独立修改位置 |
| --- | --- | --- |
| MPP源码 | /root/mpp，c08762ebfadeb4e986d2fed993bc7a54862d3ebe；只有既有未跟踪out/ | 项目out/dependency-work/mpp-lineage |
| FFmpeg源码 | /home/tang/ffmpeg-rkmpp-close-ownership，ab1e61adaa21ff129caa8e01a1198156044567ed；工作树干净 | 项目out/dependency-work/ffmpeg-lineage |

两副本均以git clone --no-hardlinks --no-checkout创建，再从上述提交创建codex/rkmpp-lineage，实际命令退出0。目标机可用48GiB、nproc=8。副本是授权开发源，不是验收临时脚本，不在单轮日志清理范围。

原安装文件保持不变：

- /home/tang/ffmpeg-ab1e61a/lib/libavcodec.so.62 → libavcodec.so.62.28.102，SHA256 7e81d6b42ccd8b70f32a59889c2ccb410c5ef7b67ae204eb12551e32cd91a591。
- /usr/local/lib/librockchip_mpp.so.1 → librockchip_mpp.so.0，SHA256 a5235b4d60afdaef44574c68569eb5b39969e43db1b1c6c9a7b4ef98c90c480b。

新构建仅安装到项目out/dependency-work下的独立prefix，不改ffenv/mtenv、ldconfig、/usr/local或原FFmpeg目录；使用进程级运行环境选择。切回时结束本轮精确PID，恢复原ffenv/mtenv选择并复核上述hash/ldd。新版本项目adapter如果要求新能力，回退时须同时切回匹配的项目产物/源码，不能只换库；首次覆盖CLI产物前保留旧CLI及hash。当前尚未覆盖或安装任何库/CLI。

## 方案与实施顺序

参考[FFmpeg官方libdav1d桥接](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/libdav1d.c)：输入packet属性附着引用计数userdata，输出picture取对应packet并调用ff_decode_frame_props_from_pkt，最后释放由回调完成。该范例是AV1软件解码实现，复用其所有权模式，不宣称MPP已有同等接口。

1. MPP隔离树补齐可持有的输入关联引用，并沿H264/HEVC parser→picture/DPB→输出传递。metadata容器有引用计数但不自动证明嵌套pointer所有权；mpp_frame_set_meta不增加引用，需要显式统一copy/replace/release语义。先统一接口，再实施，审计错误、discard、field合并、reset/close及零/多输出路径。
2. FFmpeg隔离树按相同接口交接packet属性与opaque引用，输出使用对应picture属性；保留PTS时基转换，禁止把任意token塞进媒体时间戳。所有发送失败/EAGAIN、丢弃、flush/close路径保持引用和现有last_pkt关闭修复一致。
3. 由真实后端容器/配置推导并暴露持有上界事实；项目capability与planner形成RKMPP类型化契约，probe/runtime复用现有COPY_OPAQUE/registry路径。不得把Windows native DPB常数当RKMPP证据，不加调用方猜容量参数。
4. 本机保存基线明确的依赖patch、实现记录及项目改动并分阶段提交。两名未参与实现的审查者明确PASS后，目标机8核构建依赖；项目复用原构建目录增量all-target。原上限300秒，超时保留对象并观察真实终态后续编。
5. 使用原120秒源、原四路H264/HEVC/AAC规格复验realtime；检查身份重排、释放归零、预算/内存、失源重入、VLC原生快照与A/V。每轮记录后按文件清单清理，不以单路/低规格代替。共享代码变化的Windows影响要审查，但本轮遵守用户暂不Windows编译，未复验项明确列出。

线程沿用MPP parser/hal与FFmpeg原调用线程，不引入新轮询线程。引用回调不得调用已销毁context；所有者独立于context并遵守最后引用释放。队列和内部持有量必须纳入现有预算/背压，缺失身份或能力明确失败，不能静默退化。以上是实施约束，最终接口/数值上界仍须以具体源码审查确认。

## 当前进度

已完成基线核验与两独立源码副本创建；MPP和FFmpeg负责人正在对齐接口并实施。尚无新增库、项目构建或媒体验收通过结论。原Windows与RKMPP安装保持原样。

回退CLI已实际复制至项目out/dependency-work/rollback-75a83c7e/media_transcode_realtime_video_cli，源/副本SHA256一致：adb299c6c8f3a2a5ff47aafa55dfeeef8cfbb6d0040282d194fece193e709bd2；同目录manifest.json记录源码与依赖路径。该备份验证仅为字节一致，不代表已执行回退后的媒体验收。

## 首批实现检查点

MPP输入userdata引用、packet/frame生命周期、H264/HEVC picture传播及实时几何query已实现于隔离树。首轮8核Release全目标构建PID2179343退出0，之后补context_packet_slots查询字段；后续增量复编尚待终态，不把首轮成功扩大到最终源码。FFmpeg桥接已实现COPY_OPAQUE属性传递、export所有权、discard/infochange处理与每次送包前排空输出；两份补丁保存3rds/rkmpp-lineage，原树/原库不变。

本机将native与backend opaque容量事实分为variant：native公式及options保持，backend读opened私有只读option形成上界再checked+2；opened校验拒绝混合/未知字段。SourcePresentation要求COPY_OPAQUE；旧依赖identity暂时保留，待最终依赖版本冻结后更新，当前不开放运行能力。单独本机5文件初审PASS，但两位整批独立审查仍进行中。没有新媒体验收结果。
