#pragma once

#include <atomic>

extern std::atomic<bool> GStopFlag;

/**
 * @brief 为当前线程设置独立备用栈, 防止栈溢出时无法调用信号处理函数
 * 
 */
void SetupAltstack();

/**
 * @brief 信号处理线程函数, 等待SIGINT/SIGTERM, 设置状态同步变量
 * 
 */
void SignalThread();

/**
 * @brief 设置信号掩码, 屏蔽SIGINT/SIGTERM
 * 
 */
void SignalMask();

/**
 * @brief 设置安装程序崩溃信号的处理函数
 * 
 */
void InstallCrashHandler();