# Windows 合屏等待区间取证

日期：2026-11-02。r376/r377 已取得同序号 Windows 等待 API 长停留证据，但未分离计时器到期与 Ready 线程调度延迟。这里只记录系统跟踪实际能力，不作为整项合屏任务阻塞。

## 独立会话能力探测

启动前专属会话查询4201（不存在），项目内专属ETL不存在。使用SDK tracelog实际支持的独立systemlogger，仅PROC_THREAD/CSWITCH/DISPATCHER，不启PROFILE、堆栈或默认NT Kernel Logger，不修改用户权限/注册表。

```powershell
& 'C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/tracelog.exe' -start MediaTranscode-Composition-Scheduling-Capability -systemlogger -eflag PROC_THREAD+CSWITCH+DISPATCHER -UsePerfCounter -lowcapacity -b 64 -min 2 -max 8 -cir 8 -f 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-scheduling-capability.etl'
$probeStartCode=$LASTEXITCODE
Write-Output "START_EXIT=$probeStartCode"
if($probeStartCode -eq 0){
 try{
  & 'C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/tracelog.exe' -q MediaTranscode-Composition-Scheduling-Capability -lp
  Write-Output "ACTIVE_QUERY_EXIT=$LASTEXITCODE"
 }finally{
  & 'C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/tracelog.exe' -stop MediaTranscode-Composition-Scheduling-Capability
  Write-Output "STOP_EXIT=$LASTEXITCODE"
 }
}
& 'C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/tracelog.exe' -q MediaTranscode-Composition-Scheduling-Capability
Write-Output "FINAL_QUERY_EXIT=$LASTEXITCODE"
exit 0
```

原始返回（本机工具部分中文输出解码为问号，保留原样）：
~~~text
Turning On group: PROC_THREAD
Turning On group: CSWITCH
Turning On group: DISPATCHER
Adding GroupMask ExtItem
Could not start logger: MediaTranscode-Composition-Scheduling-Capability
Operation Status:       5L
?????
Operation Status:       5L	?????

START_EXIT=5
Operation Status:       4201L	???????????????? WMI ???????

Logger Name:            MediaTranscode-Composition-Scheduling-Capability
FINAL_QUERY_EXIT=4201

~~~

实际Start返回5（Access denied），不是自动审批拒绝；后查4201，无会话残留，没有执行stop去影响其他会话。不能仅凭Performance Log Users成员身份声称当前token可采内核调度事件。此结果限制系统级归因，输出域拓扑、费用及联合solver仍可继续。WER不能替代CSwitch/ReadyThread时序。

工具/方法依据：[SystemTraceProvider](https://learn.microsoft.com/en-us/windows/win32/etw/configuring-and-starting-a-systemtraceprovider-session)、[Tracelog语法](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/tracelog-command-syntax)。相关运行证据见[r376](realtime-composition-r376-validation.md)、[r377](realtime-composition-r377-validation.md)。

## MMCSS Distribution 真实注册能力探测

日期：2026-11-02。通过 require_escalated 使用与媒体相同的本机执行环境；此选项不表示令牌成为管理员。单次直接 PowerShell 命令通过内存 Add-Type 创建专属短生命周期线程，没有写测试脚本或输出程序集，没有运行媒体或调整生产线程。首次 task index=0，注册完成立即在同线程 finally 撤销并 join；失败时立即读取原始 Win32 错误，成功时错误栏记0（不解释成功调用后未定义的 last-error）。

实际命令：

```powershell
$mmcssSource = @'
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Security.Principal;
public static class CompositionMmcssCapability {
 [DllImport("avrt.dll", CharSet=CharSet.Unicode, ExactSpelling=true, SetLastError=true)]
 static extern IntPtr AvSetMmThreadCharacteristicsW(string name, ref uint index);
 [DllImport("avrt.dll", ExactSpelling=true, SetLastError=true)]
 [return: MarshalAs(UnmanagedType.Bool)]
 static extern bool AvRevertMmThreadCharacteristics(IntPtr handle);
 [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
 [DllImport("kernel32.dll")] static extern uint GetCurrentProcessId();
 public static string Run() {
  uint owner=0, reverting=0, index=0, process=GetCurrentProcessId();
  bool registered=false, reverted=false, attempted=false;
  int registerError=0, revertError=0;
  string exception=null;
  bool admin=new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator);
  Thread probe=new Thread(delegate() {
   IntPtr handle=IntPtr.Zero;
   try {
    owner=GetCurrentThreadId();
    handle=AvSetMmThreadCharacteristicsW("Distribution", ref index);
    int rawError=Marshal.GetLastWin32Error();
    registered=handle!=IntPtr.Zero;
    registerError=registered ? 0 : rawError;
   } catch(Exception e) { exception=e.ToString(); }
   finally {
    if(handle!=IntPtr.Zero) {
     reverting=GetCurrentThreadId();
     attempted=true;
     reverted=AvRevertMmThreadCharacteristics(handle);
     int rawError=Marshal.GetLastWin32Error();
     revertError=reverted ? 0 : rawError;
    }
   }
  });
  probe.Name="MT-MMCSS-capability";
  probe.Start();
  probe.Join();
  return String.Format("utc={0:o}; pid={1}; admin_enabled={2}; owner_tid={3}; task=Distribution; task_index={4}; registered={5}; register_error={6}; revert_attempted={7}; revert_tid={8}; reverted={9}; revert_error={10}; joined={11}; exception={12}", DateTime.UtcNow,process,admin,owner,index,registered,registerError,attempted,reverting,reverted,revertError,!probe.IsAlive,exception ?? "none");
 }
}
'@
Add-Type -TypeDefinition $mmcssSource -Language CSharp
[CompositionMmcssCapability]::Run()
```

实际返回（命令退出码0）：

```text
utc=2026-11-02T03:20:38.6994702Z; pid=31984; admin_enabled=False; owner_tid=14856; task=Distribution; task_index=185; registered=True; register_error=0; revert_attempted=True; revert_tid=14856; reverted=True; revert_error=0; joined=True; exception=none
```

结论：当前非管理员启用令牌可真实注册既有 Distribution 任务；注册与撤销均成功，owner/revert TID均14856，线程已join，无异常。该进程退出后内存程序集随进程释放，无测试脚本、ETL、截图、媒体或日志文件产生，无探测线程残留。本次仅追加本文档。

能力边界：这证明本次令牌/任务的注册及同线程撤销可用，不证明未来生产 worker 注册必成功，不证明215微秒发送期限，也不代表r378发送已修复。真实 worker 仍需启动握手，注册失败必须保持原错误并在媒体执行前退出；MMCSS没有恢复已取消的Clock Rate保证。没有修改注册表、用户权限、其他进程，也没有调用SetThreadPriority或设置任意优先级。

官方依据：[AvSetMmThreadCharacteristicsW签名、首次index及失败返回](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avsetmmthreadcharacteristicsw)、[AvRevertMmThreadCharacteristics必须同线程调用](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avrevertmmthreadcharacteristics)、[MMCSS调度能力与Clock Rate保证边界](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service)。

