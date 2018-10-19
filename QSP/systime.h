#ifndef __SYSTIME_H_
#define __SYSTIME_H_

#include "typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* get system time 获取系统时间 */
void itimeofday(long *sec, long *usec);

/* get clock in millisecond 64 获取时间（毫秒） */
IINT64 iclock64(void);

IUINT32 iclock();

/* sleep in millisecond 睡眠（毫秒） */
void isleep(unsigned long millisecond);

#ifdef __cplusplus
}
#endif

#endif
