# 0.1.2：安全卸载、销毁与旧实例恢复

## 修改与证据边界

旧版把打开卷、身份检查、锁卷、卸载等步骤的失败统一显示为“关闭文件后重试”。
仅凭该提示不能确定失败发生在哪一步，也不能把所有拒绝访问都解释为文件占用。

本次检查发现服务没有显式启用 SeManageVolumePrivilege。LocalSystem 默认持有
但不启用这项权限，而文件系统可能在卷维护控制操作中检查它。新版在操作线程
的私有令牌中启用该权限，操作结束恢复原令牌；不为普通用户增加卷管理权限。

新版还会：

- 明确显示权限启用、打开卷、身份校验、FSCTL_LOCK_VOLUME、FSCTL_DISMOUNT_VOLUME、
  移除盘符或驱动实例销毁的具体失败阶段及实例 ID。
- 对短暂占用重试锁卷，最多 10 次、间隔 100 ms；锁定失败绝不继续卸载或释放数据。
- 让服务与恢复 CLI 复用同一套安全流程，避免出现绕过锁定的“强制删除”。
- 在 VM 验证脚本中主动持有文件，确认卸载失败后实例保留，关闭句柄后再正常卸载。

本机构建和故障注入测试不能替代目标 VM 的实际卷验证；仍需运行新版 Verify.ps1。

## 无需先安装，即可处理旧实例

0.1.2 的恢复 CLI 兼容现有 v1 驱动协议。无需覆盖运行中的驱动、杀死服务或重启。
关闭旧 GUI、访问内存盘的资源管理器窗口及相关程序；终端当前目录不要位于内存盘。
把新版 ZIP 解压到新目录，在该目录打开管理员 PowerShell：

```powershell
.\memdisk.exe driver-list
$diskId = Read-Host '输入上一步列出的、需要卸载的实例 ID'
.\memdisk.exe recover-unmount $diskId
.\memdisk.exe driver-list
```

此命令直连驱动进行身份确认，并按“启用权限 → 打开卷 → 锁定 → 卸载 → 移除盘符”
执行。成功后实例显示 unmounted，内存及盘内数据保留。若需要恢复访问，可通过
旧服务的 mount 操作重新挂载。不要在恢复过程中同时从 GUI 操作同一个实例。

如果确认要丢弃该实例的全部文件并释放内存，显式执行：

```powershell
.\memdisk.exe recover-destroy $diskId --yes
```

其他实例需要逐一确认 ID，不能把盘符或示例数字当作实例 ID。

## 安装新版

先保存需要保留的文件到其他盘，再显式销毁所有旧实例。然后进入旧目录运行：

```powershell
.\Uninstall.ps1
```

再进入新版固定目录：

```powershell
.\Install.ps1
.\Verify.ps1
.\memdisk-ui.exe
```

无需安装 WDK、SDK 或额外工具；原 VM 测试签名设置继续使用。

## 新版仍拒绝时

保留完整错误阶段和错误码：

- `Enable SeManageVolumePrivilege`：检查是否使用管理员终端、服务是否使用预期账户。
- `Open volume` / `Verify instance identity`：检查驱动、服务和实例是否匹配，不跳过校验。
- `FSCTL_LOCK_VOLUME`：通常表示仍有文件、目录、基准测试或其他卷句柄占用。
  关闭对应程序后重试；该阶段仍可能因系统策略被拒绝，不能仅凭错误码 5 区分。
- 其他阶段：提供完整消息，由对应步骤继续定位，不使用强制删除掩盖错误。

参考：[卷管理权限](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/privileges)、
[LocalSystem 默认权限状态](https://learn.microsoft.com/en-us/windows/win32/services/localsystem-account)、
[安全卷卸载流程](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_dismount_volume)。
