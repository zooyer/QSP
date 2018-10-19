#ifndef __LOG_H_
#define __LOG_H_

// VS编译器禁止警告
#ifdef _MSC_VER
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif

// 设置log输出的回调函数
void set_outlog(void(*write_log)(const char *log));

// 打印log日志
void write_log(const char *fmt, ...);

#endif // !__LOG_H_
