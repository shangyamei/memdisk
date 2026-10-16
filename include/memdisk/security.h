#pragma once
/* File access and administration are different trust boundaries. */
#define MD_ADMIN_SDDL L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"
#define MD_DISK_SDDL L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGWGX;;;AU)"
#define MD_ROOT_SDDL L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;0x1301bf;;;AU)"
