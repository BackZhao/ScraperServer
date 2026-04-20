#include "Backtrace.h"

#include <atomic>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>

#include "Logger.h"
#include "version.h"

char GTimeBuf[2][256]; // 双缓冲时间字符串缓冲区
std::atomic<int> GTimeIndex{0}; // 双缓冲时间字符串索引

struct SigMap { int num; const char* name; };

static const SigMap SIGMAP[] = {
    {SIGSEGV, "SIGSEGV"},
    {SIGABRT, "SIGABRT"},
    {SIGBUS, "SIGBUS"},
    {SIGILL, "SIGILL"},
    {SIGFPE, "SIGFPE"},
#ifdef SIGSYS
    {SIGSYS, "SIGSYS"},
#endif
};

void UpdateTimeBuf()
{
    int next = 1 - GTimeIndex.load(std::memory_order_relaxed);

    time_t    t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);

    strftime(GTimeBuf[next], sizeof(GTimeBuf[next]), "%Y-%m-%d %H:%M:%S", &tm);

    GTimeIndex.store(next, std::memory_order_release);
}

/**
 * @brief
 *
 * @param s
 * @param n
 */
static inline void CrashWrite(const char* s, size_t n)
{
    write(STDERR_FILENO, s, n);
    for (auto fd : GCrashLogFds) {
        if (fd >= 0) {
            write(fd, s, n);
        }
    }
}

static void CrashWriteHex(uintptr_t v)
{
    char buf[2 + sizeof(uintptr_t) * 2 + 2];
    buf[0] = '0';
    buf[1] = 'x';

    int pos = 2;
    for (int i = sizeof(uintptr_t) * 2 - 1; i >= 0; --i) {
        int d      = (v >> (i * 4)) & 0xf;
        buf[pos++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
    }
    buf[pos++] = '\n';

    CrashWrite(buf, pos);
}

void CrashWriteSignal(int sig) {

    // 查映射表写名字
    const char* name = "UNKNOWN";
    for (size_t i = 0; i < sizeof(SIGMAP)/sizeof(SIGMAP[0]); ++i) {
        if (SIGMAP[i].num == sig) {
            name = SIGMAP[i].name;
            break;
        }
    }
    CrashWrite(name, strlen(name));
}

void PrintStackTrace(int signum, void* uctx)
{
// 写入标题
    const char hdr[] = "\n======= Crash Capture =======\n";
    CrashWrite(hdr, sizeof(hdr) - 1);

    // 写入时间
    int idx = GTimeIndex.load(std::memory_order_acquire);
    CrashWrite("Time: ", 6);
    CrashWrite(GTimeBuf[idx], strnlen(GTimeBuf[idx], sizeof(GTimeBuf[idx])));
    CrashWrite("\n", 1);

    // 写入捕获的信号值
    CrashWrite("Signal: ", 8);
    CrashWriteSignal(signum);
    CrashWrite("\n", 1);

    // 写入版本号
    CrashWrite("Version: ", 9);
    CrashWrite(VERSION, strlen(VERSION));
    CrashWrite("\n", 1);

    // 写入构建信息
    CrashWrite("Compiler: ", 10);
    CrashWrite(BUILD_COMPILER_PATH, strlen(BUILD_COMPILER_PATH));
    CrashWrite(" (", 2);
    CrashWrite(BUILD_COMPILER_ID, strlen(BUILD_COMPILER_ID));
    CrashWrite(" ", 1);
    CrashWrite(BUILD_COMPILER_VERSION, strlen(BUILD_COMPILER_VERSION));
    CrashWrite(")\n", 2);
    CrashWrite("Type: ", 6);
    CrashWrite(BUILD_TYPE, strlen(BUILD_TYPE));
    CrashWrite("\n", 1);

#ifdef SYLIXOS

    // 写入函数栈
    API_BacktraceShow(STD_ERR, 100);
    for (auto fd : GCrashLogFds) {
        if (fd >= 0) {
            API_BacktraceShow(fd, 100);
        }
    }

    // TODO: SylixOS下调用_exit()会触发SIGTERM, abort()为临时举措
    abort();

#else // SYLIXOS

    // 写入函数帧
    CrashWrite("Frames:\n", 8);
    void* frames[64];
    int   n = backtrace(frames, 64);

    for (int i = 0; i < n; ++i) {
        Dl_info di;
        if (dladdr(frames[i], &di) && di.dli_fbase && di.dli_fname) {
            CrashWrite(di.dli_fname, strlen(di.dli_fname));
            CrashWrite(" + ", 3);
            uintptr_t off = (uintptr_t)frames[i] - (uintptr_t)di.dli_fbase;
            CrashWriteHex(off);
        } else {
            const char unk[] = "??\n";
            CrashWrite(unk, sizeof(unk) - 1);
        }
    }
    // 写入尾栏
    const char tail[] = "============ End ============\n";
    CrashWrite(tail, sizeof(tail) - 1);

    // 立刻终止进程
    _exit(128 + signum);

#endif // SYLIXOS

}
