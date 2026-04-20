#pragma once

#include "version.h"

// from @ChatGPT
/* ================= 编译器适配 ================= */

#if defined(_MSC_VER)

/* MSVC / Clang-cl */
#pragma section(".buildinfo", read)
/* 防止被优化掉 */
#define BUILDINFO_USED
/* 放入指定 section */
#define BUILDINFO_SECTION __declspec(allocate(".buildinfo"))
/* 防止链接器丢弃 */
#pragma comment(linker, "/include:GBuildInfo")

#else // _MSC_VER

/* GCC / Clang (ELF / Mach-O) */
#define BUILDINFO_USED __attribute__((used))

#if defined(__APPLE__)

/* Mach-O 需要 segment,section */
#define BUILDINFO_SECTION __attribute__((section("__DATA,.buildinfo")))

#else // __APPLE__

/* ELF (Linux / 嵌入式 / SylixOS) */
#define BUILDINFO_SECTION __attribute__((section(".buildinfo")))

#endif // __APPLE__

#endif // _MSC_VER

/* ================= 构建信息 ================= */

BUILDINFO_SECTION BUILDINFO_USED
const char GBuildInfo[] =
    "BUILD_INFO\0"
    "Code Version: " VERSION "\0"
    "Compiler ID: " BUILD_COMPILER_ID "\0"
    "Compiler Version: " BUILD_COMPILER_VERSION "\0"
    "Compiler Path: " BUILD_COMPILER_PATH "\0"
    "Compiler Type: " BUILD_TYPE "\0";