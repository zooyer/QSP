#ifndef __NETWORK_H_
#define __NETWORK_H_

#include "typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// 字节序宏定义   IWORDS_BIG_ENDIAN 为 1 则是大端，为 0 则是小端
//---------------------------------------------------------------------
#ifndef IWORDS_BIG_ENDIAN
#ifdef _BIG_ENDIAN_
#if _BIG_ENDIAN_
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
#if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MIPSEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) || defined(__powerpc__) || \
            defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
#define IWORDS_BIG_ENDIAN 1
#endif
#endif
#ifndef IWORDS_BIG_ENDIAN
#define IWORDS_BIG_ENDIAN  0
#endif
#endif

/* encode 8 bits unsigned int */
char* qsp_encode8u(char *p, unsigned char c);

/* decode 8 bits unsigned int */
const char* qsp_decode8u(const char *p, unsigned char *c);

/* encode 16 bits unsigned int (lsb) */
char* qsp_encode16u(char *p, unsigned short w);

/* decode 16 bits unsigned int (lsb) */
const char* qsp_decode16u(const char *p, unsigned short *w);

/* encode 32 bits unsigned int (lsb) */
char* qsp_encode32u(char *p, IUINT32 l);

/* decode 32 bits unsigned int (lsb) */
const char* qsp_decode32u(const char *p, IUINT32 *l);

/* 返回最小值 */
IUINT32 _imin_(IUINT32 a, IUINT32 b);

/* 返回最大值 */
IUINT32 _imax_(IUINT32 a, IUINT32 b);

/* 返回中间值 */
IUINT32 _ibound_(IUINT32 lower, IUINT32 middle, IUINT32 upper);

/* 返回差值 */
long _itimediff(IUINT32 later, IUINT32 earlier);

#ifdef __cplusplus
}
#endif

#endif // !__NETWORK_H_
