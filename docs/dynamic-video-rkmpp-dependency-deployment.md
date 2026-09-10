# RKMPP 修复依赖部署证据

日期：2026-09-10。FFmpeg 源码父提交 d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf，关闭所有权修复提交 ab1e61adaa21ff129caa8e01a1198156044567ed。独立工作树 /home/tang/ffmpeg-rkmpp-close-ownership，独立安装 /home/tang/ffmpeg-ab1e61a。原 /root/ffmpeg-rockchip 干净，未覆盖 /usr/local 的 FFmpeg。MPP源码提交 c08762ebfadeb4e986d2fed993bc7a54862d3ebe。

完整configure/make/install进程PID3819346，wait实际退出0。生成版本头、version.sh和已安装ffmpeg -version均为ab1e61a；旧版本与新版本不混用。原始构建日志提取结果后删除。

实际构建配置：
```text
--prefix=/home/tang/ffmpeg-ab1e61a --enable-gpl --enable-version3 --enable-libdrm --enable-gnutls --enable-rkmpp --enable-rkrga --disable-static --enable-shared --enable-pic --extra-cflags='-I/usr/local/include -I/usr/local/include/rga' --extra-ldflags='-L/usr/local/lib -L/usr/local/lib/aarch64-linux-gnu'
```

安装后的FFmpeg ldd确认全部七个FFmpeg共享库来自同一独立prefix；MPP来自/usr/local/lib，RGA来自/usr/local/lib/aarch64-linux-gnu。核心98742fd6在/home/tang/dynamic-video-98742fd6全量构建615项成功，PID3919127的wait实际退出0；新CLI SHA256为9b7789cd6b9713fe36571f7b9e5ac057ce51f0afa85a601ce263c433f073eb5f。其ldd确认实际依赖的六个FFmpeg库均来自新prefix（CLI未链接libavdevice），MPP/RGA路径与上表一致，CMakeCache头文件/库目录也为新prefix。随后RKMPP03真实CLI PID1490370的/proc/maps确认六个FFmpeg库实际来自新prefix，MPP/RGA映射来自上述平台路径；同规格实流三项通过且payload归零，详见dynamic-video-rkmpp-03.md。

| 文件 | SHA256 |
|---|---|
| lib/libavcodec.so.62.28.102 | 7e81d6b42ccd8b70f32a59889c2ccb410c5ef7b67ae204eb12551e32cd91a591 |
| lib/libavfilter.so.11.14.102 | 35db0197f8f55ee4b3f7c871fef786439c74725935f8903696d9b6ceca5f42e5 |
| lib/libavutil.so.60.26.102 | 0948c93b18d042bb4d6d5ac5698151c9d95c6290f8067d5dd3f034e14b0af907 |
| lib/libavformat.so.62.12.102 | 7be668928d8f364205b5b6b78ccb9faf7b16ffe88f24532388a021d6e0d0dc45 |
| lib/libavdevice.so.62.3.102 | 7bac88f1feb692083639250d780a80e03c60c28e0487067a716912127ec4cb10 |
| lib/libswscale.so.9.5.102 | 51370c417b0efb3206d6177529687be0349c0049bb396971f6a6099023bad22b |
| lib/libswresample.so.6.3.102 | 2fddc917d381dd09a84b1a22a01df5e72b476739be516e4b1a17ddc39052ae88 |
| bin/ffmpeg | 76ae4d35c91d420e721982840ba71d09014ee4a6e239c128d07f0f3ce600986f |
| /usr/local/lib/librockchip_mpp.so.1 | a5235b4d60afdaef44574c68569eb5b39969e43db1b1c6c9a7b4ef98c90c480b |
| /usr/local/lib/aarch64-linux-gnu/librga.so.2 | 53d97c03ad7f2e514106a9cd849eae2f17a61441a30b23856c08c5a93dc17e3c |

相对路径均相对独立安装prefix。版本查询仍打印client 12 driver is not ready；该未选中客户端探测提示保留，不能据此声称所选硬件链路失败或正常。关闭修复依据及适用边界见[依赖记录](../3rds/ffmpeg-rockchip/README.md)。
