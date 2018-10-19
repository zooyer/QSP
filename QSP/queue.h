#ifndef __QUEUE_H_
#define __QUEUE_H_

//=====================================================================
// QUEUE DEFINITION    队列定义 -  iqueue_head   
//=====================================================================

//=====================================================================
// iqueue_init		IQUEUE_INIT			初始化队列（初始化为环形队列）
// iqueue_entry		IQUEUE_ENTRY		节点首地址
// iqueue_add		IQUEUE_ADD			在head节点之后插入新节点
// iqueue_add_tail	IQUEUE_ADD_TAIL		在head节点之前插入新节点(环形队列，插入到队尾)
// iqueue_del		IQUEUE_DEL			删除该节点，连接prev与next节点，并对该节点置零
// iqueue_del_init	IQUEUE_DEL_INIT		删除该节点，连接prev与next节点，并对该节点初始化（环形）
// iqueue_is_empty	IQUEUE_IS_EMPTY		判断节点是否有next节点（判断节点的next指向是否是自己）
//=====================================================================
// iqueue_foreach	IQUEUE_FOREACH		遍历该队列（格式：for(xxx; xxx; xxx)）：可用于自定义结构体
// iqueue_foreach_entry					遍历该队列（格式：for(xxx; xxx; xxx)）：只能用于队列节点类型
// __iqueue_splice						把list分解拼接到head队列上
// iqueue_splice						如果list不为空，则把list分解拼接到head队列上
// iqueue_splice_init					把list拼接到head上，并对list做初始化
//=====================================================================
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

#ifdef __cplusplus
extern "C" {
#endif

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;

//---------------------------------------------------------------------
// queue init                                                         
//---------------------------------------------------------------------
#define IQUEUE_HEAD_INIT(name) { &(name), &(name) }
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

// 初始化节点，使其成为环形队列
#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

// 获取结构体中成员距首地址偏移量
#define IOFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

// 获取队列节点的首地址(根据结构体中的成员地址 - 结构体中成员距首地址偏移量 = 节点首地址)
// 根据node.next的地址求出该my_type节点的首地址
// struct my_type
// {
// 	int a;
// 	int b;
// 	struct node *next;
// };
#define ICONTAINEROF(ptr, type, member) ( \
		(type*)( ((char*)((type*)ptr)) - IOFFSETOF(type, member)) )

// 获取队列节点的首地址
#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)



//---------------------------------------------------------------------
// queue operation                     
//---------------------------------------------------------------------
// 在head节点之后插入新节点
#define IQUEUE_ADD(node, head) ( \
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

// 在head节点之前插入新节点
#define IQUEUE_ADD_TAIL(node, head) ( \
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

// 删除两个节点之间的节点，连接两个节点
#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

// 删除该节点，连接prev与next节点
#define IQUEUE_DEL(entry) (\
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

// 删除该节点，并对该节点进行初始化（环形）
#define IQUEUE_DEL_INIT(entry) do { \
	IQUEUE_DEL(entry); IQUEUE_INIT(entry); } while (0)

// 是否为空节点（判断节点的next是否指向自己）
#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init		IQUEUE_INIT
#define iqueue_entry	IQUEUE_ENTRY
#define iqueue_add		IQUEUE_ADD
#define iqueue_add_tail	IQUEUE_ADD_TAIL
#define iqueue_del		IQUEUE_DEL
#define iqueue_del_init	IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

// 遍历该队列：可用于自定义结构体（结构体中含有队列节点）（格式：for(xxx; xxx; xxx)）
#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER) \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		&((iterator)->MEMBER) != (head); \
		(iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

// 遍历该队列：可用于自定义结构体（结构体中含有队列节点）（格式：for(xxx; xxx; xxx)）
#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)

// 遍历该队列：只能用于队列节点的遍历，只做地址遍历（格式：for(xxx; xxx; xxx)）
#define iqueue_foreach_entry(pos, head) \
	for( (pos) = (head)->next; (pos) != (head) ; (pos) = (pos)->next )

// 拼接两个环形队列成为一个环形队列（把list分解拼接到head队列上）
#define __iqueue_splice(list, head) do {	\
		iqueue_head *first = (list)->next, *last = (list)->prev; \
		iqueue_head *at = (head)->next; \
		(first)->prev = (head), (head)->next = (first);		\
		(last)->next = (at), (at)->prev = (last); }	while (0)

// 判断如果list不为空，则拼接到head上
#define iqueue_splice(list, head) do { \
	if (!iqueue_is_empty(list)) __iqueue_splice(list, head); } while (0)

// 把list拼接到head上，并对list做初始化
#define iqueue_splice_init(list, head) do {	\
	iqueue_splice(list, head);	iqueue_init(list); } while (0)


// VS编译器禁止警告
#ifdef _MSC_VER
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif // !_MSC_VER

#endif // !__IQUEUE_DEF__

#ifdef __cplusplus
}
#endif

#endif // !__QUEUE_H_
