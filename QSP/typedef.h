#ifndef __TYPEDEF_H_
#define __TYPEDEF_H_

#ifdef __cplusplus
extern "C" {
#endif

//=====================================================================
// 32bit整形定义
//=====================================================================
// 跨屏台处理定义
// ISTDUINT32(32bit)：	无符号32bit整形
// ISTDINT32(32bit)：	有符号32bit整形
//=====================================================================
#ifndef __INTEGER_32_BITS__
#define __INTEGER_32_BITS__

#if defined(_WIN64) || defined(WIN64) || defined(__amd64__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IA64) || \
	defined(_M_AMD64)
typedef unsigned int ISTDUINT32;
typedef int ISTDINT32;

#elif defined(_WIN32) || defined(WIN32) || defined(__i386__) || \
	defined(__i386) || defined(_M_X86)
typedef unsigned long ISTDUINT32;
typedef long ISTDINT32;

#elif defined(__MACOS__)
typedef UInt32 ISTDUINT32;
typedef SInt32 ISTDINT32;

#elif defined(__APPLE__) && defined(__MACH__)
#include <sys/types.h>
typedef u_int32_t ISTDUINT32;
typedef int32_t ISTDINT32;

#elif defined(__BEOS__)
#include <sys/inttypes.h>
typedef u_int32_t ISTDUINT32;
typedef int32_t ISTDINT32;

#elif (defined(_MSC_VER) || defined(__BORLANDC__)) && (!defined(__MSDOS__))
typedef unsigned __int32 ISTDUINT32;
typedef __int32 ISTDINT32;

#elif defined(__GNUC__)
#include <stdint.h>
typedef uint32_t ISTDUINT32;
typedef int32_t ISTDINT32;

#else 
typedef unsigned long ISTDUINT32;
typedef long ISTDINT32;
#endif
#endif // !__INTEGER_32_BITS__

//=====================================================================
// 整形定义
//=====================================================================
// 定义类型
// IINT8(8bit)：	有符号8bit整形
// IUINT8(8bit)：	无符号8bit整形
// IINT16(16bit)：	有符号16bit整形
// IUINT16(16bit)：	无符号16bit整形
// IINT32(32bit)：	有符号32bit整形
// IUINT32(32bit)：	无符号32bit整形
// IINT64(64bit)：	有符号64bit整形
// IUINT64(64bit)：	无符号64bit整形
// INLINE:			定义内联函数宏
//=====================================================================
#ifndef __INTEGER_ALL_BITS__
#define __INTEGER_ALL_BITS__

#ifndef __IINT8_DEFINED
#define __IINT8_DEFINED
typedef char IINT8;
#endif

#ifndef __IUINT8_DEFINED
#define __IUINT8_DEFINED
typedef unsigned char IUINT8;
#endif

#ifndef __IUINT16_DEFINED
#define __IUINT16_DEFINED
typedef unsigned short IUINT16;
#endif

#ifndef __IINT16_DEFINED
#define __IINT16_DEFINED
typedef short IINT16;
#endif

#ifndef __IINT32_DEFINED
#define __IINT32_DEFINED
typedef ISTDINT32 IINT32;
#endif

#ifndef __IUINT32_DEFINED
#define __IUINT32_DEFINED
typedef ISTDUINT32 IUINT32;
#endif

#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

#ifndef __IUINT64_DEFINED
#define __IUINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 IUINT64;
#else
typedef unsigned long long IUINT64;
#endif
#endif

#ifndef INLINE
#if defined(__GNUC__)

#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define INLINE         __inline__ __attribute__((always_inline))
#else
#define INLINE         __inline__
#endif

#elif (defined(_MSC_VER) || defined(__BORLANDC__) || defined(__WATCOMC__))
#define INLINE __inline
#else
#define INLINE 
#endif
#endif

#if (!defined(__cplusplus)) && (!defined(inline))
#define inline INLINE
#endif

#endif // !__INTEGER_ALL_BITS__

#ifdef __cplusplus
}
#endif

#endif // !__TYPEDEF_H_
