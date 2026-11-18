# rk458：源组排空基础接口构建记录

状态：基础切片双独立源码审查通过；RKMPP增量构建完成，303项、退出0。未进行媒体验收，不代表BYE尾媒体问题已修复。

## 范围与证据

- 基于ddc4b69f，新增EndGeneration控制、源组单一关闭状态、固定接收frontier、逐成员完成确认、SourceDrained回收校验及完整Epoch owner固定准入计费。
- retained control保持对象身份及所在媒体域generation的精确校验；未启用生产BYE入口。新增控制对象不扩字段；每源仅一个固定关闭状态，无媒体队列或第二decoder。
- pacing_diagnostic_review、remaining_acceptance_audit均给出基础切片SOURCE PASS。源码核验修正clock-group与playback代次区别，以及startup后单transaction通道的整组屏障；详见排空调查文档。
- 构建目标为既有RKMPP Release目录，8核增量、全部目标。构建PID=3209814，SSH会话对应既有目标项目。Windows没有构建或修改FFmpeg。
- 目标原CLI SHA256：6b5cb59a7394d62c2560c06197f06ab881222c7caa73e07c5563e110081c2dd1。原CLI、被覆盖源码和文件清单保留在目标项目out/rollback/rk458，便于回退；三项新增源码可据incoming与existing清单差异识别。
- 第一次SCP因目标out/acceptance目录不存在失败；创建该目录后重传退出0。无依赖修改。

## 验证边界与后续

本轮尚未运行CLI/FFmpeg/VLC媒体链路，不能新增成功验收项。下一切片继续decoder及其他holder的排空、上游整组发布屏障、下游aggregate真实贡献完成、接收截止及新代交接；接线后须原规格RKMPP实流及共享核心Windows回归。rk454发送失败后的停止挂起仍未关闭。

本轮临时产物清单：本机out/acceptance/rk458-source.tar；目标out/acceptance/rk458-source.tar、rk458-build.log。构建完成记录结果后按此清单清理；源媒体、构建结果、回退目录保留。

## 构建结果

PID3209814已结束，wait返回0；303/303完成并链接realtime CLI。产物13052368字节，SHA256=26e12070a17d1f00832f3d8322a3db56d664bdce4ccd3bc43ae2667c792774d8。该产物仅含基础13文件，不含随后解码器四文件切片。没有运行local CLI。

清理完成：上述3项临时文件已逐项删除，本机/目标rk458临时项无残留，构建PID不存在；正式构建与回退目录保留。
