# rk486：Linux有限等待修正构建与验证（进行中）

基线61215106，有限等待修正5源码与设计文档已获source_drain_review_a/b双源码PASS。仅Linux平台等待adapter调整，原媒体deadline、发送服务率、pacing算法和Windows等待实现不变，不改外部FFmpeg。详细依据见realtime-composition-linux-finite-wait.md。

RKMPP8核增量PID3386328；回退out/rollback/rk486保留3个原源码及CLI，恢复需另删除新增MediaLinuxTimerSlackScope.h/cpp并刷新恢复源码时间戳后增量构建。临时清单：本机/远端out/acceptance/rk486-source.tar，远端out/acceptance/rk486-build.log。原规格实流待验证，源码通过不证明rk485已修复。

构建11/11退出0，CLI SHA256 f15a75de26a88fdf14e01cb673ab186a106bc35dac5cef4b6077867d6c1668c5。记录后逐项清理传输tar与构建日志，保留回退；实流另记rk487。
