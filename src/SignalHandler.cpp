#include "SignalHandler.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Backtrace.h"

std::atomic<bool> GStopFlag{false}; // 停止所有业务线程的标志位

const int CRASH_SIGNALS[] = {
    SIGSEGV,
    SIGABRT,
    SIGBUS,
    SIGILL,
    SIGFPE,
#ifdef SIGSYS
    SIGSYS
#endif
}; // 进程崩溃的信号集合

void SetupAltstack()
{
    stack_t ss{};
    ss.ss_size = SIGSTKSZ;
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_flags = 0;
    sigaltstack(&ss, nullptr);
}

void SignalThread()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    int sig;
    sigwait(&set, &sig);

    GStopFlag.store(true, std::memory_order_relaxed);
    fprintf(stderr, "Signal %d recved, quitting...\n", sig);
}

void SignalMask()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

static void CrashSignalHandler(int signum, siginfo_t* info, void* uctx)
{
    PrintStackTrace(signum, uctx);
}

void InstallCrashHandler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = CrashSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;

    for (int signum : CRASH_SIGNALS) {
        sigaction(signum, &sa, nullptr);
    }
}
