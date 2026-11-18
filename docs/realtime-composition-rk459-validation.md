# rk459：解码器代内排空切片

状态：四文件修后双SOURCE PASS，RKMPP增量构建5项完成、退出0；尚未启用生产BYE，不作为媒体验收成功。

## 实施与复审

基于3c8f77e8，仅修改AudioDecodeNode.cpp/.h、VideoDecodeNode.cpp/.h。EndGeneration先要求已规划同步lineage并精确校验当前边generation，复用send-null/receive直到真实AVERROR_EOF，再结算lineage与发布原控制。基类先提交pending输出，标记不越过帧；完成后等待原owner purge执行codec flush/reset。视频EndGeneration不提前flushBuffers；EOF/Flush原行为保持。沿用原字段，无新增队列、第二codec或对象尺寸增长。

依据[FFmpeg官方send/receive状态机](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)。两位审查最初均FAIL：非同步lineage的授权函数直接成功，导致非同步decoder可能永久等待不存在的purge。已在两个入口明确拒绝，pacing_diagnostic_review与remaining_acceptance_audit复审均SOURCE PASS。

## 验证与回退

目标既有RKMPP Release目录，8核增量全部目标；PID3274013。原CLI SHA256为26e12070a17d1f00832f3d8322a3db56d664bdce4ccd3bc43ae2667c792774d8；原CLI、四文件源码及清单保留在目标out/rollback/rk459。没有运行local CLI或任何媒体测试；Windows FFmpeg未修改。

待完成：控制生产端、startup整组发布屏障、其他holder及aggregate排空、接收截止和新代交接，随后原规格双平台realtime实流。rk454失败停止问题仍在风险清单。

临时清单：本机out/acceptance/rk459-source.tar；目标out/acceptance/rk459-source.tar、rk459-build.log。结果归档后逐项清理，保留构建和回退产物。

构建结果：wait PID3274013返回0，5/5完成；realtime CLI为13052216字节，SHA256=4efcd8c404d3d8384434760af641e01b67887f1bd5e515b2413adfb1122483e6。local CLI仅作为全部目标被链接，未运行。

清理完成：本轮3项临时文件按清单删除，本机/目标rk459临时项无残留、构建PID不存在；构建和回退保留。
