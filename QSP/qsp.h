/**********************************************************
 *
 * Author        : zhangzhongyuan
 * Email         : zzyservers@163.com
 * Create time   : 2018-08-09 14:23
 * Last modified : 2018-08-09 14:23
 * Filename      : qsp.h
 * Description   : Quick Stable Protocol(QSP)
 *
 * *******************************************************/


#ifndef __QSP_H_
#define __QSP_H_

#include "log.h"
#include "queue.h"
#include "network.h"
#include "typedef.h"
#include "allocator.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------
//	QSP BASE CONFIG DEFINE
//--------------------------------------------------

#define QSP_MTU_SIZE 1400		// 提交给下层协议的MTU大小
#define QSP_HEAD_SIZE (IOFFSETOF(QSPSEG, data) - IOFFSETOF(QSPSEG, conv))	// 报文头部大小
#define QSP_BUF_SIZE (QSP_MTU_SIZE * 3)	// 缓冲区大小
#define QSP_PASS_NUM 2			// 错过两个报文片段，则认为是丢包，请求重补报文（非单工模式）

#define QSP_VERSION 65535		// QSP协议版本，16bit
#define QSP_TIME_OUT 200		// ACK超时间隔，单位：毫秒
#define QSP_SINGLE_NUM 3		// 单工模式下，每个报文片段发送3次

#define QSP_CMD_PUSH 81			// cmd: push (传输数据)
#define QSP_CMD_ACK 82			// cmd: ack (AKC应答)
#define QSP_CMD_AGAIN 83		// cmd: again (要求重传)

#define QSP_MODE_HALF 91		// mode: half (半双工)：可以回复任意数据
#define QSP_MODE_WEAK 92		// mode: weak (微双工)：只能回复一个字节的数据
#define QSP_MODE_SINGLE 93		// mode: single (单工)：不回复任何数据


//--------------------------------------------------
//	QSP TYPE DEFINE
//--------------------------------------------------

// head size 24
struct QSPSEG
{
	IUINT32 conv;			//会话编号（报文标识）
	IUINT32 frg;			//报文片段序号fragment
	IUINT32 ts;				//时间戳time stamp
	IUINT32 sn;				//报文片段序数量（seg num）
	IUINT16 cmd;			//报文处理动作（push/ack/again）
	IUINT16 mode;			//通信模式（half/weak/single）（weak：单段数据最大为：(MSS - HEAD_SIZE) * 256）
	IUINT16 ver;			//版本（最大值：65535）
	IUINT16 len;			//data数据的长度

	char data[1];			//数据段，用len来描述该段的大小
};

struct QSPNODE
{
	struct IQUEUEHEAD node;
	IUINT32  ts;				//time stamp时间戳（毫秒）
	struct QSPSEG seg;			//segment报文（可变长度）
};

struct QSP
{
	//会话编号 / MTU（最大传输单元） / MSS（最大报文长度） / 通信模式 / 版本
	IUINT32 conv, mtu, mss, mode, ver;
	//下一个待发送的包号 / 下一个待接收的包号
	IUINT32 snd_nxt, rcv_nxt;

	IUINT32 nrcv_que, nsnd_que;		// rcv_queue中的节点数，snd_queue中的节点数
	IUINT32 nrcv_buf, nsnd_buf;		// rcv_buf中的节点数，snd_buf中的节点数

	struct IQUEUEHEAD snd_queue;	// 已切片的报文的队列
	struct IQUEUEHEAD rcv_queue;	// 接收切片的报文队列，有序
	struct IQUEUEHEAD snd_buf;		// 发送报文保存到buf队列，等待ack
	struct IQUEUEHEAD rcv_buf;		// 接收报文切片的buf队列，如果丢包，则后续报文片段放入该队列

	void *user;						// 用户地址，用于input，output回调函数中使用
	void *buff;						// buffer缓冲区
	struct QSPSEG last;				// 保存上一个报文片段状态，判断是否报文重复

	IUINT32(*systime)(void);		// 获取系统时间的回调函数，用于时间戳（单位：毫秒）
	int(*input)(char *buf, int len, struct QSP *kcp, void *user);			// 输入数据
	int(*output)(const char *buf, int len, struct QSP *kcp, void *user);	// 输出数据
};

typedef struct QSP QSP;
typedef struct QSPSEG QSPSEG;
typedef struct QSPNODE QSPNODE;


//--------------------------------------------------
//	USER INTERFACE
//--------------------------------------------------

QSP* qsp_create(IUINT32 conv, void *user);
int qsp_release(QSP *qsp);
int qsp_version(QSP *qsp);
int qsp_send(QSP *qsp, const void *buf, int len);
int qsp_recv(QSP *qsp, void *buf, int len);

int qsp_setinput(QSP *qsp, int(*input)(char *buf, int len, QSP *qsp, void *user));
int qsp_setoutput(QSP *qsp, int(*output)(const char *buf, int len, QSP *qsp, void *user));
int qsp_setsystime(QSP *qsp, IUINT32(*systime)(void));
int qsp_setmode(QSP *qsp, IUINT32 mode);
void qsp_print(struct IQUEUEHEAD *head);

#ifdef __cplusplus
}
#endif

#endif // !__QSP_H_
