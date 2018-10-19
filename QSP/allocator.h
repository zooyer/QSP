#ifndef __ALLOCATOR_H_
#define __ALLOCATOR_H_

#ifdef __cplusplus
extern "C" {
#endif

// malloc/free钩子函数（默认为空）
static void* (*__malloc_hook)(size_t) = 0;
static void(*__free_hook)(void *) = 0;

// 设置分配器
#define set_allocator(malloc, free) \
	__malloc_hook=malloc,__free_hook=free

// 分配空间
#define malloc_hook(size) \
	__malloc_hook ? __malloc_hook(size) : malloc(size)

// 释放空间
#define free_hook(ptr) \
	__free_hook ? __free_hook(ptr) : free(ptr)

#ifdef __cplusplus
}
#endif

#endif // !__ALLOCATOR_H_
