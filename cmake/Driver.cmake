set(WDK_ROOT "C:/Program Files (x86)/Windows Kits/10" CACHE PATH "WDK root")
set(WDK_VERSION "10.0.26100.0" CACHE STRING "WDK version")
set(KM_INCLUDE "${WDK_ROOT}/Include/${WDK_VERSION}")
set(KM_LIB "${WDK_ROOT}/Lib/${WDK_VERSION}/km/x64")
# C is only used for the kernel target; user-mode C++ keeps normal Debug flags.
string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
if(NOT EXISTS "${KM_INCLUDE}/km/ntddk.h")
  message(FATAL_ERROR "Install WDK ${WDK_VERSION} on the build host.")
endif()
add_executable(memdisk-driver driver/entry.c driver/control.c driver/memory.c
  driver/disk.c driver/io.c driver/storage.c driver/volume.c driver/security.c)
set_target_properties(memdisk-driver PROPERTIES PREFIX "" OUTPUT_NAME memdisk
  SUFFIX .sys PDB_NAME memdisk-driver MSVC_RUNTIME_LIBRARY "")
target_include_directories(memdisk-driver PRIVATE "${KM_INCLUDE}/km"
  "${KM_INCLUDE}/shared" driver)
target_compile_definitions(memdisk-driver PRIVATE _AMD64_ AMD64 NTSTRSAFE_LIB
  NTDDI_VERSION=0x0A00000C _WIN32_WINNT=0x0A00)
target_compile_options(memdisk-driver PRIVATE /kernel /GS /Zl /Zi /Oi /wd4201 /wd4214)
target_link_options(memdisk-driver PRIVATE /DRIVER /SUBSYSTEM:NATIVE /ENTRY:GsDriverEntry
  /NODEFAULTLIB /INCREMENTAL:NO /MANIFEST:NO /OPT:REF /OPT:ICF /DEBUG)
target_link_libraries(memdisk-driver PRIVATE "${KM_LIB}/ntoskrnl.lib"
  "${KM_LIB}/hal.lib" "${KM_LIB}/wdmsec.lib" "${KM_LIB}/BufferOverflowFastFailK.lib" "${KM_LIB}/ntstrsafe.lib")
