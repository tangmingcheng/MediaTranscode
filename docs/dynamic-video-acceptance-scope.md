# 动态视频多输出验收边界与Windows复核

用户明确本次仅验收三项：发出有无超契约突发、动态增删是否正常、发出码流能否由VLC正常解码。保持固定120秒源及原编解码规格，先Windows后RKMPP；不增加播放器零显示晚帧、系统调度或GPU专项门禁。显示晚帧如实保留，不能等同解码失败，也不据此宣称根因已定位。

## Windows：按明确范围复核PASS

链路：H.264 RTP 1280×720 30fps 8,013,434bps输入，HEVC/H.264 MPEG-TS/RTP 1920×1080 25fps CBR6Mbps GOP50输出。生产冻结98742fd6，二进制SHA256 EABCDBD7370A166A7A92DE06038CDC653ED3082A909895CB2C534C7FCE1B0053。

| 验收项 | 判定与实流证据 |
|---|---|
| 发送突发 | PASS：Windows28聚合78017包/98138632 IP字节与scope一致，50Mbps服务曲线超额1356B等于最大包。限定为既定发送契约内的单包粒度边界，不宣称物理链路绝无瞬时聚集。 |
| 动态增删 | PASS：四路Running；第二路复用group1，第三路新建group2；前三路正常退役，删除至零后新建group3恢复输出；最终payload0，40527次申请释放相等。 |
| VLC解码 | PASS：四路D3D11VA，四张1920×1080真实画面已查看，RTP到达序号连续、TS检查无错误。保留第三路一次20ms显示丢弃，不把displayed当唯一帧数，不补写未记录的解码错误计数。 |

实际CLI/FFmpeg/VLC命令及完整结果见[Windows28](dynamic-video-windows-28.md)，同版本发送失败计数及排空补证见[Windows27](dynamic-video-windows-27.md)。两位未参与实现者windows_scope_review_a/b分别逐项明确PASS，允许进入RKMPP。原始材料已按用户要求清理，本次复核已审核报告，未重新分析原始抓包，未无变化重跑Windows。

保留历史报告按当时门禁记录的FAIL；本记录是依据用户明确范围的独立复核结论。RKMPP尚未通过，完整交付仍待目标实流验收。评分不因边界澄清自行上调。
