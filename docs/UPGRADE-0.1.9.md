# 0.1.9：普通用户删除与重命名权限

用户在普通 PowerShell 内执行 Git clone，初始化 .git/config 时无法删除
config.lock。源码发现 NTFS 根目录已授予可继承 Modify，但磁盘设备的
Authenticated Users ACE 只有 GR/GW/GX，没有 DELETE。设备启用了
FILE_DEVICE_SECURE_OPEN，命名空间检查可能在文件 ACL 之前拒绝对应访问。

此次为设备命名空间增加 SD（DELETE），并使用新的私有安全类 GUID 避免复用
旧的持久化安全默认值。没有授予普通用户修改设备 ACL 或所有者的权限。
原始磁盘的读写和 DELETE 打开继续由驱动捕获请求者令牌并限制为管理员/SYSTEM；
控制设备和管理管道 ACL 不变。NTFS 根目录权限保持现有 Modify 策略。

## 更新（在隔离 VM 执行）

本次包含驱动修改，仅更换 GUI 无效。保存需保留的数据到其他盘，显式安全销毁
所有实例，成功卸载旧版后再安装新包。卸载可使用 0.1.8 及之后版本脚本。
不要覆盖正在加载的 SYS，不为升级绕过锁卷；锁失败则保留实例诊断。
如自行选择重启以卸载旧驱动，先保存数据；内存盘内容不会跨重启保留。

## 回归

1. 管理员运行 Verify.ps1。它分别创建 NTFS/exFAT，受限令牌执行嵌套目录的
   config.lock 写入、关闭、重命名为 config、替换已有 config 及显式删除。
   任一失败报告具体阶段和 Win32 错误，保留测试实例供诊断。
2. 普通终端运行 memdisk-access-test.exe R（替换实际盘符）。确认上述文件操作
   通过，原始数据读写、原始 DELETE 和控制设备访问继续被拒绝。
3. VM 已有 Git 时，在普通 PowerShell 的新目录中执行 git init，检查初始
   config 创建；再次执行 git config probe.value test，确认配置替换成功。
   再执行用户原来的 git clone。Git 不属于自动验证的额外依赖。
4. 失败时分别贴出 icacls R:\、目标 .git 目录及 config.lock 的 ACL 和 attrib
   输出（每条命令单独一行），以及 Procmon 失败的 CreateFile、rename/delete
   事件。ACL、只读属性或外部过滤器仍可能造成其他访问失败。

主机测试只验证安全描述符的预期访问权，不能替代设备命名空间及文件系统的
VM 集成验证；该修复也不证明此前 FSCTL_LOCK_VOLUME 拒绝访问的根因。

2026-10-17 用户确认原先普通终端 Git clone 失败的问题已经修复。
此反馈针对原始复现场景，不等于完整 NTFS/exFAT、原始访问拒绝及压力测试通过。

参考：https://learn.microsoft.com/windows-hardware/drivers/kernel/controlling-device-namespace-access
