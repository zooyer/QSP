#include "log.h"

#include <stdio.h>
#include <stdarg.h>

// 自定义日志输出回调函数
void(*__write_log)(const char *log) = NULL;

// 设置log输出的回调函数
void set_outlog(void(*write_log_func)(const char *log))
{
	__write_log = write_log_func;
}

// 打印log日志
void write_log(const char * fmt, ...)
{
	char buffer[4096];
	va_list argptr;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);

	if (__write_log)
		__write_log(buffer);
	else
		printf("[default log] : %s\n", buffer);
}
