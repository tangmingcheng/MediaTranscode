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

隔离 MPP 与 FFmpeg 依赖已完成实现、双独立源码审查、构建及安装，版本和实际加载身份已核验。项目正在使用新依赖增量构建，原规格实时媒体验收仍待执行；不得将依赖完成视为合屏完成。原 Windows 与 RKMPP 安装保持原样。

回退CLI已实际复制至项目out/dependency-work/rollback-75a83c7e/media_transcode_realtime_video_cli，源/副本SHA256一致：adb299c6c8f3a2a5ff47aafa55dfeeef8cfbb6d0040282d194fece193e709bd2；同目录manifest.json记录源码与依赖路径。该备份验证仅为字节一致，不代表已执行回退后的媒体验收。

## 首批实现检查点

MPP输入userdata引用、packet/frame生命周期、H264/HEVC picture传播及实时几何query已实现于隔离树。首轮8核Release全目标构建PID2179343退出0，之后补context_packet_slots查询字段；后续增量复编尚待终态，不把首轮成功扩大到最终源码。FFmpeg桥接已实现COPY_OPAQUE属性传递、export所有权、discard/infochange处理与每次送包前排空输出；两份补丁保存3rds/rkmpp-lineage，原树/原库不变。

本机将native与backend opaque容量事实分为variant：native公式及options保持，backend读opened私有只读option形成上界再checked+2；opened校验拒绝混合/未知字段。SourcePresentation要求COPY_OPAQUE；旧依赖identity暂时保留，待最终依赖版本冻结后更新，当前不开放运行能力。单独本机5文件初审PASS，但两位整批独立审查仍进行中。没有新媒体验收结果。

## 错误路径修复与隔离构建进展

MPP补齐packet复制分配失败、segments独立所有权及put入队失败回收；只有入队成功才消费源packet。最终8核增量构建PID2257585退出0，隔离prefix-mpp安装PID2262247退出0。FFmpeg EOS改为单次提交并分类错误，只有成功接受EOS才进入draining；可重试失败先排空，致命错误直接返回。补丁hash已更新README。

两名未参与实现的独立审查者已对冻结补丁明确SOURCE PASS，本机5处源码亦通过源码审查；容量结论仅为受审模式下的身份持有上界，不等于RSS总额。设备永久停滞的退出能力仍待真实失败链路验证。FFmpeg隔离configure PID2265961退出0，首次8核all构建PID2275937正在进行。新隔离树无可复用对象缓存，首次构建不是项目的全量重建；后续保留对象增量续编。尚未运行新依赖下的媒体验收，不把源码审查与构建结果视为合屏通过。

## 新依赖构建与接入进度

FFmpeg首段PID2275937/2275938超时退出124；保留对象续编PID2311656退出0。隔离提交92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696，版本增量2327082与安装2330856均退出0。MPP隔离提交560734e83e7d3d8a0a2a337d07108f04f4030caa。

目标项目5个opaque合同源码已在检查旧hash后同步dee00de1，写后hash全部一致；目标目录无.git。初次SSH命令因Windows长度限制未启动，改为临时同步脚本执行退出0。本机out/dependency-sync-command.txt、out/dependency-sync.py和远端out/dependency-work/project-source-sync.py已按精确路径清理。

两名独立审查者对4文件identity与证据文字更新明确SOURCE PASS；其他5个RKMPP能力合同继续有效。新props不引用输入packet.buf，其opaque引用由新增合同覆盖，不重复加入压缩输入槽位；后端malloc不宣称属于引擎PMR预算。原rk-release需刷新FFMPEG_*、pkgcfg_lib_FFMPEG_*、__pkg_config_*FFMPEG缓存再增量构建。新依赖媒体验收仍未开始。

## 隔离 FFmpeg 构建最终证据（2026-11-13）

原依赖源码树没有可复用的 `rkmppdec.o`；隔离树首次建立对象缓存，后续均保留对象增量编译。所有构建与安装仅位于 `/home/tang/MediaTranscode-composition-09093919/out/dependency-work`，未写入原 FFmpeg 安装或 `/usr/local`。

| 阶段 | 精确 PID | 实际命令及退出结果 |
| --- | --- | --- |
| configure | 2265961 | `timeout 300 ./configure ...`，退出 0 |
| 首段全目标 | 2275937；timeout 子进程 2275938 | `timeout 300 make -j8 all`，退出 124；300 秒到期终止，未见编译 error，不能记为成功 |
| 保留对象续编 | 2311656 | `timeout 300 make -j8 all`，退出 0，ffmpeg/ffprobe 链接完成 |
| 提交后的版本增量 | 2327082 | `timeout 300 make -j8 all`，退出 0 |
| 隔离安装 | 2330856 | `timeout 300 make -j8 install`，退出 0 |

首段超时后已确认原 PID 和 PGID 2275938 无残留才续编；最终确认上述 PID 全部退出。源码提交仅发生在隔离 FFmpeg 树，revision 为 `92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696`，工作树干净。生成的 `libavutil/ffversion.h`、隔离 `ffmpeg -version`、Python ctypes 直接调用隔离 libavutil 的 `av_version_info()` 均返回 `92aec19`。

configure 沿实际旧安装功能配置：GPL、version3、libdrm、gnutls、rkmpp、rkrga、shared、pic，保留 `--disable-static`；只将安装前缀改为上述目录的 `prefix-ffmpeg`，并将 `prefix-mpp/include`、`prefix-mpp/lib` 放在原 `/usr/local` include/lib 搜索路径之前。编译环境为 `source /opt/mt-tools/mtenv.sh; mtenv on`（GCC 12.4.0）。`pkg-config --variable=prefix rockchip_mpp` 实际返回隔离 `prefix-mpp`。

运行核验使用 `LD_LIBRARY_PATH` 顺序：隔离 `prefix-ffmpeg/lib`、`prefix-mpp/lib`、`/opt/mt-tools/gcc-12.4-native/lib64`。对隔离 `prefix-ffmpeg/bin/ffmpeg` 执行 `ldd`：avcodec、avutil、avfilter 均解析至隔离 `prefix-ffmpeg/lib`，`librockchip_mpp.so.1` 解析至隔离 `prefix-mpp/lib`，无 `not found`。库版本为 avcodec 62.28.102、avutil 60.26.102、avfilter 11.14.102。

| 实际库 | 新隔离库 SHA256 | 原安装库 SHA256（安装前后相同） |
| --- | --- | --- |
| libavcodec.so.62 | `c24ac984faba646fd2c143c70b24895927350bd121b4d5d474df201d3e7e4673` | `7e81d6b42ccd8b70f32a59889c2ccb410c5ef7b67ae204eb12551e32cd91a591` |
| libavutil.so.60 | `53326bc4ea6cfa1e36ed38b128894a0a8b6069c37ccdecb8dcd316518a51e2a5` | `0948c93b18d042bb4d6d5ac5698151c9d95c6290f8067d5dd3f034e14b0af907` |
| libavfilter.so.11 | `e4de2d8efad2540fc491d38ea455acf2a8d9d24d88d1e3b9108bcbab7eb17c9b` | `35db0197f8f55ee4b3f7c871fef786439c74725935f8903696d9b6ceca5f42e5` |
| librockchip_mpp.so.1 | `92d29267a7b07311261973ecfa985c25be0b554f5b9b5e75621f6dd144bb21af` | `a5235b4d60afdaef44574c68569eb5b39969e43db1b1c6c9a7b4ef98c90c480b` |

原 FFmpeg 库路径为 `/home/tang/ffmpeg-ab1e61a/lib`，原 MPP 路径为 `/usr/local/lib/librockchip_mpp.so.1`。只读版本调用仍输出 `mpp_platform: client 12 driver is not ready!`；此次没有启动解码，不能据此判定硬件解码失败或通过。以上只证明依赖构建、安装及加载身份，不代表实时合屏验收完成。
清理记录：主线程与 MPP 作者确认无需继续读取后，已逐项删除本轮 14 个远端临时文本：`ffmpeg-configure`、`ffmpeg-build-1`、`ffmpeg-build-2`、`ffmpeg-build-version`、`ffmpeg-install` 各自的 `.log`/`.exit`，以及 `ffmpeg-version.txt`、`ffmpeg-ldd.txt`、`dependency-sha256.txt`、`original-dependency-sha256-after.txt`。清理后精确清单残留为零；源码、对象缓存、隔离安装、rollback 与两份 patch 均保留，未触碰项目 rk-release 构建日志。

## 项目增量首次失败与修正

原rk-release重新配置PID2350470退出0，六个库和头文件目录均已指向prefix-ffmpeg。项目8核增量PID2353126退出1，停于246/820，不是超时：MediaDecoderOpaqueRetentionAdapter.cpp向avcodec_is_open(AVCodecContext*)传入const指针。Windows现有头文件同样签名，属于新共享源码编译问题，不是平台API差异。修正只读查询调用，与既有MediaEncoderOpenContractAdapter采用相同const_cast边界；不改变查询语义或Windows依赖。对象缓存保留，双审后续编；Windows暂不构建遵从用户要求，故不能声称本轮Windows复验通过。rk410尚未启动。
