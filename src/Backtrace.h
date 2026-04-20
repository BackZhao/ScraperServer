#pragma once

/**
 * @brief 更新时间缓冲区, 用于记录SGEV发生的时间
 * 
 */
void UpdateTimeBuf();

/**
 * @brief 打印当前的函数堆栈
 * 
 * @param signum 信号值
 */
void PrintStackTrace(int signum, void* uctx);