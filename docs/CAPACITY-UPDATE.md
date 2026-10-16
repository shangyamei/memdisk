# Capacity policy update (development build 0.1.6)

This update removes the 1 GiB single-disk cap, 4 GiB aggregate cap, 64 MiB
minimum, 25%-of-total-RAM admission and fixed 512 MiB reserve. A creation request
must be a positive whole MiB and <=80% of available physical RAM measured by the
service immediately before the driver call. At most 8 instances remain.
The displayed GUI limit is a snapshot; the service rechecks at submission.
Nonpaged pool limits, concurrent allocations, and Windows filesystem size
requirements can still cause failure. No data memory is allocated lazily.

## Replace components in the isolated VM

The wire ABI remains v1; its field sizes and meanings have not changed. Older
components retain their old limits, so replace driver, service, CLI and GUI
together using the 0.1.6 development package.
Back up needed files elsewhere, safely destroy all RAM disks, close the GUI,
then follow the existing uninstall/install instructions in README-VM.md.
Do not replace a loaded driver or destroy instances implicitly during upgrade.
No driver is installed or loaded on the build host.

## VM regression

1. Run Verify.ps1 for both NTFS and exFAT, including restricted-user access and
   occupied-volume refusal. Retain failed instances and their IDs for diagnosis.
2. On a VM with enough available RAM, create a 2048 MiB disk via CLI and GUI.
   Verify capacity and write/read hashes across 16 MiB chunk boundaries and beyond
   the former 1 GiB boundary. Unmount, remount, recheck hashes and safely destroy.
3. If the VM memory budget permits, create disks totaling more than 4 GiB. Check
   that each new request uses the remaining-memory budget, not original RAM.
4. Submit a request above the current 80% budget. Expect a memory-admission error
   with the current maximum; no extra instance should be published. Since memory
   changes continuously, check the rejection message's snapshot, not stale UI text.
5. Exercise the exact 80% boundary and one MiB above in a controlled VM. Below
   budget requests may still fail allocation; partial failure must leave no new
   device and release its table and data chunks. Use VM Driver Verifier/fault
   injection for rollback and repeated-cycle leak checks.
6. Try zero, negative, non-integer and overflowing CLI capacities; reject before
   contacting the service. Small positive disks may fail Windows formatting and
   must follow the existing safe rollback/diagnostic path.

These runtime and fault-injection checks require VM evidence; host unit tests
and a successful build do not establish kernel or filesystem runtime safety.
