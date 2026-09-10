# RKMPP Beta库交付记录

## 动态增删基础包

路径：`/home/tang/packages/media-transcode-beta-rkmpp-d11b25a7-20260910/`。源码冻结d11b25a7，生产核心仍为已通过三项验收的98742fd6。依用户要求从该提交重新全量构建615项，configure及build均退出0，build PID1677834；不是复用旧库冒充新构建。

实际调用：

```bash
/home/tang/package_media_transcode_beta.sh /home/tang/dynamic-video-d11b25a7/out/build/rk-release /home/tang/packages/media-transcode-beta-rkmpp-d11b25a7-20260910
```

脚本原先固定从/home/tang/MediaTranscode取头文件，其哈希与构建源码不同；已改为从构建CMakeCache读取CMAKE_HOME_DIRECTORY，并归档在tools/packaging/package_media_transcode_beta.sh。未覆盖旧版本包。

包内lib/libmedia_transcode_beta.a合并Beta facade、realtime application和core三个archive；include/media_transcode_beta/realtime.h来自同一源码提交。实际导出add_output、remove_output、get_output_snapshots已核对。

| 文件 | SHA256 |
|---|---|
| lib/libmedia_transcode_beta.a | f769066b8ac3b8add3cf5bdebc2cb2ee3ca6c210a274d93128c03fa10919ba18 |
| include/media_transcode_beta/realtime.h | b668af6bdd413a1d99466bab0b286fc45b5fac9bd35e5696f1554105149cb300 |
| examples/beta_minimal.c | 07738442fccd0bd0c4374825a8509e856b479feb009a6ff033b1122ff4cb69f1 |
| bin/beta_minimal | 3534f1e4931aaef339fe0930de7173b7807f02c861acc56ace72dbac6d0ea07c |
| bin/media_transcode_realtime_video_cli | 9b7789cd6b9713fe36571f7b9e5ac057ce51f0afa85a601ce263c433f073eb5f |

示例基于用户指定ba190e3c旧包，保留配置/回调/快照结构，增加交互式动态增删和输出列表。C11 -Wall -Wextra -Werror编译及链接均退出0；README给出编译/运行/操作说明。SHA256SUMS在加入示例、程序、README及VERSION后重新生成，11个文件全通过。示例ldd确认FFmpeg来自/home/tang/ffmpeg-ab1e61a；仍依赖目标已安装的FFmpeg/MPP/RGA，不是全静态可移植系统包。

基础包未单独重跑示例实流；同包CLI哈希与RKMPP03验收二进制一致。后续profile扩展由用户另外授权，基础包不含该字段，不能与未来profile头文件混用。基础构建日志、传输归档及临时对象已在记录结果后删除，源码/构建目录和交付包保留。

## Profile扩展

待最新冻结重建及RKMPP实流。public output末尾增加profile字符串，初始和动态输出在返回前复制到既有video.profile；不新增默认值、fallback、独立编码链或level参数。调用方必须使用同包头文件重新编译。实际profile以输出SPS/PPS或HEVC PTL为准，不以readback捕获成功替代码流证明。

0b71a44c首次全量构建失败：Beta mapper引用了实时请求中不存在的profile成员；此前两份源码PASS未发现该遗漏，已撤销其profile结论并要求复审。修复补齐实时请求自有字符串及planRealtimeVideoParameters到既有编码请求的传递，初始、动态和编码组匹配统一复用；不更改发送、队列或线程模型。失败构建不作为验收通过。
