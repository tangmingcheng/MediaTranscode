# rk483：Linux发送等待事实诊断（进行中）

基线2fe70532，新增3文件仅诊断实际等待线程TID、scheduler policy、timer slack及条件变量API起止，不设置策略、不放宽期限、不修改FFmpeg。两位独立审查者source_drain_review_a/b明确SOURCE PASS。

rk482窗口72.215微秒、等待目标超调95.130微秒、其后检查5.250微秒，最终晚28.165微秒；仅证明等待返回时已过截止，不能单独归因timer slack。Linux官方PR_SET_TIMERSLACK说明条件变量/futex受slack影响，按线程继承，零值恢复默认。实际shell读取50000ns仍不是sender线程证据：https://man7.org/linux/man-pages/man2/PR_SET_TIMERSLACK.2const.html 。

测量限制：新增3个系统调用位于相对wait_for之前，会增加总目标偏差；API区间包括重新获取mutex，不能当纯内核睡眠。Windows等待实现不变，但诊断结构变大，仍保留最终共享链路回归。

RKMPP源码核验：现用ffmpeg-lineage HEAD92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696，rkmppdec.c输入满后取帧100ms仍EAGAIN则同调用继续while循环；上游fork同结构，不能直接指认本地补丁引入：https://github.com/nyanmisaka/ffmpeg-rockchip/blob/master/libavcodec/rkmppdec.c 。FFmpeg send/receive合同不允许双端EAGAIN，因此不能简单改return EAGAIN或外层超时后并发flush/free：https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/avcodec.h 。本轮尚未修改依赖或确认其可取消接口。

暂停恢复后重连成功，无残留CLI/build。8核增量构建PID3275117，回退out/rollback/rk483含3个原源码和CLI。临时产物清单：本机/远端out/acceptance/rk483-source.tar、rk483-run.sh，远端rk483-build.log；实流尚未开始。

构建10/10退出0，CLI SHA256 2bb0f837529f6c790b98cedb8184e9cac8916e4dd0e5365548fe33ddbd794327。
