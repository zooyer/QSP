#include "qsp.h"

// 分配一个新的qsp报文节点（size：data的数据大小 ）
static QSPNODE* qsp_segment_new(int size)
{
	if (size < 0)
	{
		write_log("[qsp_segment_new : %d] : error, argument error", __LINE__);
		return NULL;
	}

	QSPNODE *qnode = (QSPNODE*)malloc_hook(sizeof(QSPNODE) + size);
	memset(qnode, 0, sizeof(QSPNODE) + size);
	iqueue_init(&qnode->node);

	return qnode;
}

// 释放一个qsp报文
static void qsp_segment_delete(QSPNODE *segnode)
{
	if (segnode == NULL)
	{
		write_log("[qsp_segment_delete : %d] : error, argument error", __LINE__);
		return;
	}

	free_hook(segnode);
}

// 输入数据、执行回调函数（从input回调函数中读入数据）
static int qsp_input(QSP *qsp, void*buf, int len)
{
	assert(qsp);
	assert(buf);

	if (qsp->input == NULL)
	{
		write_log("[qsp_input : %d] : error, input callback is NULL", __LINE__);
		return -1;
	}
	return qsp->input((char*)buf, len, qsp, qsp->user);
}

// 输出数据、执行回调函数（输出到output回调函数中）
static int qsp_output(QSP *qsp, const void*buf, int len)
{
	assert(qsp);
	assert(buf);

	if (qsp->output == NULL)
	{
		write_log("[qsp_output : %d] : error, output callback is NULL", __LINE__);
		return -1;
	}

	return qsp->output((const char*)buf, len, qsp, qsp->user);
}

// 时钟（获取当前系统时间：毫秒）
static IUINT32 qsp_click(QSP *qsp)
{
	assert(qsp);

	if (qsp->systime == NULL)
	{
		write_log("[qsp_click : %d] : error, systime callback is NULL", __LINE__);
		return 0;
	}

	return qsp->systime();
}

// 对报文头部数据编码/转为小端（返回报文头部的尾地址 + 1）
static char* qsp_encode_seg(char *ptr, const QSPNODE *qnode)
{
	assert(ptr);
	assert(qnode);

	ptr = qsp_encode32u(ptr, qnode->seg.conv);
	ptr = qsp_encode32u(ptr, qnode->seg.frg);
	ptr = qsp_encode32u(ptr, qnode->seg.ts);
	ptr = qsp_encode32u(ptr, qnode->seg.sn);
	ptr = qsp_encode16u(ptr, qnode->seg.cmd);
	ptr = qsp_encode16u(ptr, qnode->seg.mode);
	ptr = qsp_encode16u(ptr, qnode->seg.ver);
	ptr = qsp_encode16u(ptr, qnode->seg.len);

	return ptr;
}

// 解析ACK，处理ACK回应（删除snd_buf队列中的节点）
static void qsp_parse_ack(QSP *qsp, IUINT32 frg)
{
	assert(qsp);

	struct IQUEUEHEAD *p, *next;

	// 删除snd_buf队列中的frg节点
	for (p = qsp->snd_buf.next; p != &qsp->snd_buf; p = next)
	{
		QSPNODE *qnode = iqueue_entry(p, QSPNODE, node);
		next = p->next;
		if (frg == qnode->seg.frg)
		{
			iqueue_del(p);
			qsp_segment_delete(qnode);
			qsp->nsnd_buf--;
			break;
		}
	}
}

// 回应ACK，创建ACK报文并发送（return 0：single）
static int qsp_respond_ack(QSP *qsp, QSPNODE *qnode)
{
	assert(qsp);
	assert(qnode);

	if (qnode->seg.mode == QSP_MODE_HALF)
	{
		QSPNODE *qnode_ack = qsp->buff;
		qnode_ack->seg.conv = qnode->seg.conv;
		qnode_ack->seg.frg = qnode->seg.frg;
		qnode_ack->seg.ts = qsp->systime();
		qnode_ack->seg.sn = 1;
		qnode_ack->seg.cmd = QSP_CMD_ACK;
		qnode_ack->seg.mode = qnode->seg.mode;
		qnode_ack->seg.ver = QSP_VERSION;
		qnode_ack->seg.len = 0;
		qsp_encode_seg(qnode_ack, qnode_ack);

		QSPSEG *seg = (QSPSEG *)qnode_ack;

		return qsp_output(qsp, qsp->buff, QSP_HEAD_SIZE);
	}
	else if (qnode->seg.mode == QSP_MODE_WEAK)
	{
		IUINT8 ack = (IUINT8)qnode->seg.frg;
		qsp_encode8u((char*)(&ack), ack);
		return qsp_output(qsp, &ack, 1);
	}
	else
	{
		//write_log("[qsp_respond_ack : %d] : warning, mode are not allowed to respond ack", __LINE__);
		return 0;
	}
	return 0;
}

// 请求重发报文片段（只有在半双工时调用）
static int qsp_request_again(QSP *qsp, IUINT32 frg)
{
	assert(qsp);

	QSPNODE *qnode_ack = qsp->buff;

	qnode_ack->seg.conv = qsp->conv;
	qnode_ack->seg.frg = frg;
	qnode_ack->seg.ts = qsp->systime();
	qnode_ack->seg.sn = 1;
	qnode_ack->seg.cmd = QSP_CMD_AGAIN;
	qnode_ack->seg.mode = qsp->mode;
	qnode_ack->seg.ver = qsp->ver;
	qnode_ack->seg.len = 0;

	qsp_encode_seg(qnode_ack, qnode_ack);

	return qsp_output(qsp, qsp->buff, QSP_HEAD_SIZE);
}

// 发送一个节点（印上时间戳）
static int qsp_send_node(QSP *qsp, QSPNODE *qnode)
{
	assert(qsp);
	assert(qnode);

	char *buf = qsp->buff;
	int datalen = qnode->seg.len;
	int size = 0;

	memcpy(buf, &qnode->seg, QSP_HEAD_SIZE);
	size = QSP_HEAD_SIZE;

	buf = qsp_encode_seg(buf, qnode);

	memcpy(buf, qnode->seg.data, datalen);
	size += datalen;

	qnode->ts = qsp_click(qsp);					// 保存该节点发送的时间戳

	return qsp_output(qsp, qsp->buff, size);	// 调用用户发送数据回调函数
}

// 解析数据:判断该节点，如果不重复就放入rcv_buf中，如果是rev_queue的下一个数据，就移动到recv_queue中
// 返回值：0该报文不是重复的报文 ，非0是重复的报文
int qsp_parse_data(QSP *qsp, QSPNODE *newnode)
{
	assert(qsp);
	assert(newnode);

	int repeat = 0;	//是否是重复的报文
	struct IQUEUEHEAD *p, *prev;
	IUINT32 sn = newnode->seg.sn;

	// 判断报文是否重复
	if (qsp->last.frg == newnode->seg.frg && qsp->last.sn == newnode->seg.sn && qsp->last.ts == newnode->seg.ts)
		repeat = 1;
	else
		qsp->last = newnode->seg;	//保存报文头信息

									// 是否是第一个报文
	if (newnode->seg.sn - newnode->seg.frg == 1 && qsp->rcv_nxt == 0)
		qsp->rcv_nxt = newnode->seg.frg;

	// 再次判断报文片段是否重复（冗余判断，保证数据的完整）
	for (p = qsp->rcv_buf.prev; p != &qsp->rcv_buf; p = prev)
	{
		QSPNODE *qnode = iqueue_entry(p, QSPNODE, node);
		prev = p->prev;
		if (qnode->seg.sn == sn)
		{
			repeat = 1;	//重复的报文
			break;
		}
	}

	// 丢弃重复的报文
	if (repeat == 1)
	{
		qsp_segment_delete(newnode);
	}
	else
	{
		iqueue_init(&newnode->node);
		iqueue_add(&newnode->node, p);
		qsp->nrcv_buf++;
	}

	// 移动有效（与rev_queue的节点的下一个）的数据到rev_queue
	while (!iqueue_is_empty(&qsp->rcv_buf))
	{
		QSPNODE *qnode = iqueue_entry(qsp->rcv_buf.next, QSPNODE, node);
		if (qnode->seg.frg == qsp->rcv_nxt)
		{
			iqueue_del(&qnode->node);
			qsp->nrcv_buf--;
			iqueue_add_tail(&qnode->node, &qsp->rcv_queue);
			qsp->nrcv_que++;
			qsp->rcv_nxt--;
		}
		else
		{
			write_log("[qsp_parse_data %d]: error", __LINE__);
			break;
		}
	}

	return repeat;
}

// 查看待接收缓存队列中的数据大小
int qsp_peeksize(const QSP *qsp)
{
	assert(qsp);

	struct IQUEUEHEAD *p;
	QSPNODE *qnode;
	int length = 0;

	// 队列为空
	if (iqueue_is_empty(&qsp->rcv_queue))
		return length;

	// 只有一个节点，没有后续报文片段
	qnode = iqueue_entry(qsp->rcv_queue.next, QSPNODE, node);
	if (qnode->seg.frg == 0) return qnode->seg.len;

	// 有多个报文片段，累计各个片段长度总和
	for (p = qsp->rcv_queue.next; p != &qsp->rcv_queue; p = p->next) {
		qnode = iqueue_entry(p, QSPNODE, node);
		length += qnode->seg.len;
		if (qnode->seg.frg == 0) break;
	}

	return length;
}

// 检测待发送队列 -> 发送一组数据（放入snd_buf中，待ACK确认）
int qsp_send_flush(QSP *qsp)
{
	assert(qsp);

	printf("--in qsp_send_flush()\n");

	// 检测待发送队列 -> 发送数据（放入snd_buf中，待ACK确认）
	QSPNODE *qnode;
	for (qnode = qsp->snd_queue.next; qnode != &qsp->snd_queue; qnode = qsp->snd_queue.next)
	{
		// 根据通信模式，来判断发送次数
		int count = qsp->mode == QSP_MODE_SINGLE ? QSP_SINGLE_NUM : 1;
		for (int i = 0; i < count; i++)
		{
			if (qsp_send_node(qsp, qnode) != qnode->seg.len + QSP_HEAD_SIZE)
				write_log("[qsp_send_flush : %d] : error, qsp_send_node return length error", __LINE__);
		}

		// 单工通信模式不需要超时重发，与ACK确认
		if (qsp->mode != QSP_MODE_SINGLE)
		{
			iqueue_del(&qnode->node);
			iqueue_add_tail(&qnode->node, &qsp->snd_buf);
		}
		else
		{
			iqueue_del(&qnode->node);
			qsp_segment_delete(qnode);
		}
	}

	// 单工通信模式不需要ACK回应
	if (qsp->mode == QSP_MODE_SINGLE)
		return 0;

	// 接收ACK回应
	while (qsp->snd_buf.next != &qsp->snd_buf)
	{
		// 刷新接收缓冲区
		if (qsp_recv_flush(qsp))
			write_log("[qsp_send_flush : %d] : error, qsp_recv_flush return < 0", __LINE__);

		// 遍历snd_buf队列，检测没有回应ACK超时的节点重新发送。
		QSPNODE *qn;
		for (qn = qsp->snd_buf.next; qn != &qsp->snd_buf; qn = qn->node.next)
		{
			// 超时重发数据
			if (qn->ts + QSP_TIME_OUT < qsp->systime())
			{
				write_log("ack is timeout...");
				if (qsp_send_node(qsp, qn) != qn->seg.len + QSP_HEAD_SIZE)
					write_log("[qsp_send_flush : %d] : error, qsp_send_node return length error", __LINE__);
			}
		}
	}
	printf("--out qsp_send_flush()\n");

	return 0;
}

// 接收一组数据 -> 待确认接收队列（放入recv_buf中，处理放入recv_queue）
int qsp_recv_flush(QSP *qsp)
{
	assert(qsp);

	// 接收数据 -> 判断cmd类型：QSP_CMD_ACK、QSP_CMD_PUSH、QSP_CMD_AGAIN
	while (1)
	{
		char *buf = qsp->buff;
		int ret = qsp_input(qsp, qsp->buff, QSP_BUF_SIZE);

		if (ret < 0)
		{
			write_log("[qsp_recv_flush.qsp_input : %d] : error, ret < 0", __LINE__);
			return -1;
		}
		else if (ret == 0)	// 非阻塞没有读到数据
		{
			break;
		}
		else if (ret == 1)	// QSP_MODE_WEAK（ACK报文）
		{
			IUINT8 ack;
			qsp_decode8u(qsp->buff, &ack);
			qsp_parse_ack(qsp, ack);
			break;
		}
		else if (ret > 1)	// QSP_MODE_HALF 或 QSP_MODE_SINGLE 模式
		{
			IUINT32 conv, frg, ts, sn;
			IUINT16 cmd, mode, ver, len;
			QSPNODE *qnode;

			// 判断报文标识
			buf = qsp_decode32u(buf, &conv);
			if (conv != qsp->conv)
			{
				write_log("[qsp_recv_flush : %d] : error, recv conv != qsp->conv", __LINE__);
				return -2;
			}

			buf = qsp_decode32u(buf, &frg);
			buf = qsp_decode32u(buf, &ts);
			buf = qsp_decode32u(buf, &sn);
			buf = qsp_decode16u(buf, &cmd);
			buf = qsp_decode16u(buf, &mode);
			buf = qsp_decode16u(buf, &ver);
			buf = qsp_decode16u(buf, &len);

			// 判断cmd类型
			if (cmd != QSP_CMD_PUSH && cmd != QSP_CMD_ACK &&
				cmd != QSP_CMD_AGAIN)
			{
				write_log("[qsp_recv_flush : %d] : error, cmd is unknow", __LINE__);
				return -3;
			}

			if (cmd == QSP_CMD_ACK)
			{
				qsp_parse_ack(qsp, frg);
				break;
			}
			else if (cmd == QSP_CMD_PUSH)
			{
				//创建一个新节点
				qnode = qsp_segment_new(len);

				qnode->seg.conv = conv;
				qnode->seg.frg = frg;
				qnode->seg.ts = ts;
				qnode->seg.sn = sn;
				qnode->seg.cmd = cmd;
				qnode->seg.mode = mode;
				qnode->seg.len = len;

				if (len > 0)
					memcpy(qnode->seg.data, buf, len);

				// 解析数据，如果重复报文则丢弃
				qsp_parse_data(qsp, qnode);

				// 判断丢包重补(半双工模式)
				if (qnode->seg.mode == QSP_MODE_HALF)
				{
					// rcv_buf中有节点，说明有丢包，要求重补
					int count = 0;
					QSPNODE *q;
					for (q = qsp->rcv_buf.next; q != &qsp->rcv_buf; q = q->node.next)
						count++;
					if (count > QSP_PASS_NUM)
						qsp_request_again(qsp, qsp->rcv_nxt);
				}

				// 回应ACK报文
				if (qsp_respond_ack(qsp, qnode) < 0)
					return -2;

				// 满足接收完一组完整的报文片段，跳出循环
				if (qsp->rcv_nxt == (IUINT32)-1)
				{
					qsp->rcv_nxt = 0;
					break;
				}
			}
			else if (cmd == QSP_CMD_AGAIN)
			{
				QSPNODE *qnode = NULL;
				for (qnode = qsp->snd_buf.next; qnode != &qsp->snd_buf; )
				{
					if (qnode->seg.frg == frg)
						break;
				}
				if (qnode != NULL)
					qsp_send_node(qsp, qnode);

				break;
			}
			else
			{
				write_log("[qsp_recv_flush : %d] : error, cmd is unknow", __LINE__);
				return -3;
			}
		}
		else
		{
			return -1;
		}
	}

	return 0;
}


//--------------------------------------------------
//	USER INTERFACE
//--------------------------------------------------

// 打印队列中节点信息
void qsp_print(struct IQUEUEHEAD *head)
{
	assert(head);

	QSPNODE *qnode = head->next;

	for (qnode = head->next; qnode != head; qnode = qnode->node.next)
	{
		printf("--------------------------------------\n");
		printf("conv : 0x%X\n", qnode->seg.conv);
		printf("frg : %lu\n", qnode->seg.frg);
		printf("ts : %lu\n", qnode->seg.ts);
		printf("sn : %lu\n", qnode->seg.sn);
		switch (qnode->seg.cmd)
		{
		case QSP_CMD_PUSH:
			printf("cmd : %s\n", "QSP_CMD_PUSH");
			break;
		case QSP_CMD_ACK:
			printf("cmd : %s\n", "QSP_CMD_ACK");
			break;
		default:
			printf("cmd : %s\n", "unknow command");
			break;
		}
		switch (qnode->seg.mode)
		{
		case QSP_MODE_HALF:
			printf("mode : %s\n", "QSP_MODE_HALF");
			break;
		case QSP_MODE_WEAK:
			printf("mode : %s\n", "QSP_MODE_WEAK");
			break;
		case QSP_MODE_SINGLE:
			printf("mode : %s\n", "QSP_MODE_SINGLE");
			break;
		default:
			printf("mode : %s\n", "unknow mode");
			break;
		}
		printf("ver : %lu\n", qnode->seg.ver);
		printf("len : %lu\n", qnode->seg.len);
		printf("--------------------------------------\n");
		printf("############     data     ############\n");
		printf("--------------------------------------\n");
	}
}

// 创建qsp对象
QSP * qsp_create(IUINT32 conv, void *user)
{
	QSP *qsp = malloc_hook(sizeof(QSP));
	if (qsp == NULL)
	{
		write_log("[qsp_create : %d] : error, malloc_hook function return NULL", __LINE__);
		return NULL;
	}

	// init
	qsp->conv = conv;
	qsp->mtu = QSP_MTU_SIZE;
	qsp->mss = QSP_MTU_SIZE - QSP_HEAD_SIZE;
	qsp->mode = QSP_MODE_HALF;
	qsp->ver = QSP_VERSION;

	qsp->snd_nxt = 0;
	qsp->rcv_nxt = 0;

	qsp->nrcv_buf = 0;
	qsp->nsnd_buf = 0;
	qsp->nrcv_que = 0;
	qsp->nsnd_que = 0;

	iqueue_init(&qsp->snd_queue);
	iqueue_init(&qsp->rcv_queue);
	iqueue_init(&qsp->snd_buf);
	iqueue_init(&qsp->rcv_buf);

	qsp->user = user;
	qsp->buff = (char*)malloc_hook(QSP_BUF_SIZE);
	memset(&qsp->last, 0, sizeof(qsp->last));

	qsp->systime = NULL;
	qsp->input = NULL;
	qsp->output = NULL;

	return qsp;
}

// 释放qsp对象
int qsp_release(QSP * qsp)
{
	if (qsp == NULL)
	{
		write_log("[qsp_release : %d] : error, argument error", __LINE__);
		return -1;
	}

	// TODO release all child objects
	if (qsp->buff != NULL)
		free_hook(qsp->buff);

	free_hook(qsp);

	return 0;
}

// 获取版本号
int qsp_version(QSP * qsp)
{
	return QSP_VERSION;
}

// 发送数据 -> buf队列（切片写入到待发送队列中）
int qsp_send(QSP *qsp, const void * buf, int len)
{
	if (qsp == NULL || buf == NULL || len < 0)
	{
		write_log("[qsp_send : %d] : error, argument error", __LINE__);
		return -1;
	}

	if (qsp->mode == QSP_MODE_WEAK && len > 256 * qsp->mss)
	{
		write_log("[qsp_send : %d] : error, QSP_MODE_WEAK allow max length is %d", __LINE__, 256 * qsp->mss);
		return -2;
	}

	QSPNODE *qnode;
	int count, i;
	int datalen = len;

	if (len <= (int)qsp->mss)
		count = 1;
	else
		count = (len + qsp->mss - 1) / qsp->mss;

	if (count == 0)
		count = 1;

	// 对数据进行切片
	for (i = 0; i < count; i++)
	{
		int size = len >(int)qsp->mss ? (int)qsp->mss : len;

		qnode = qsp_segment_new(size);
		if (qnode == NULL)
		{
			write_log("[qsp_send : %d] : error, qsp_segment_new function return NULL", __LINE__);
			return -3;
		}

		if (buf && len > 0) {
			memcpy(qnode->seg.data, buf, size);
		}

		// 封装报文头部信息
		qnode->seg.conv = qsp->conv;					// 报文标识
		qnode->seg.frg = count - i - 1;					// 包号与包发送的顺序相反（最后一个包序号：0）
		qnode->seg.ts = qsp_click(qsp);					// 时间戳，用来判断报文是否重复
		qnode->seg.sn = count;							// 该组报文的总数
		qnode->seg.cmd = QSP_CMD_PUSH;					// 报文的动作类型
		qnode->seg.mode = qsp->mode;					// 通信模式
		qnode->seg.ver = qsp->ver;						// 协议版本
		qnode->seg.len = size;							// 数据段的长度

		iqueue_init(&qnode->node);
		iqueue_add_tail(&qnode->node, &qsp->snd_queue);	// 插入到snd_queue队尾中，待发送
		qsp->nsnd_que++;

		if (buf)
			buf = (const char*)buf + size;

		len -= size;
	}

	if (qsp_send_flush(qsp))
	{
		write_log("[qsp_send : %d] : error, qsp_send_flush error", __LINE__);
		return -3;
	}

	return datalen;
}

// 接收数据 <- recv_queue队列（从已接收队列中提取数据）
int qsp_recv(QSP *qsp, void * buf, int len)
{
	assert(qsp);
	assert(buf);

	printf("--in qsp_recv()\n");
restart:
	if (qsp_recv_flush(qsp) < 0)
	{
		write_log("[qsp_recv : %d] : error, qsp_recv_flush return < 0", __LINE__);
		return -1;
	}

	struct IQUEUEHEAD *p;
	int ispeek = (len < 0) ? 1 : 0;
	int peeksize;
	QSPNODE *qnode;

	if (iqueue_is_empty(&qsp->rcv_queue))
	{
		write_log("[qsp_recv : %d] : error, rcv_queue is empty", __LINE__);
		goto restart;
		return -1;
	}

	if (len < 0) len = -len;

	peeksize = qsp_peeksize(qsp);

	if (peeksize < 0)
		return -2;

	if (peeksize > len)
		return -3;

	// 片段拼接
	for (len = 0, p = qsp->rcv_queue.next; p != &qsp->rcv_queue; )
	{
		int fragment;
		qnode = iqueue_entry(p, QSPNODE, node);
		p = p->next;

		if (buf) {
			memcpy(buf, qnode->seg.data, qnode->seg.len);
			buf = (char*)buf + qnode->seg.len;
		}

		len += qnode->seg.len;
		fragment = qnode->seg.frg;

		// 偷窥数据不删除原来节点
		if (ispeek == 0) {
			iqueue_del(&qnode->node);
			qsp_segment_delete(qnode);
			qsp->nrcv_que--; // 需要接收的节点数减少
		}

		if (fragment == 0)
			break;
	}

	assert(len == peeksize);

	printf("--out qsp_recv()\n");
	return len;
}

// 设置接收数据回调函数(非阻塞)
int qsp_setinput(QSP * qsp, int(*input)(char *buf, int len, QSP *qsp, void *user))
{
	if (qsp == NULL || input == NULL)
		return -1;

	qsp->input = input;

	return 0;
}

// 设置发送数据回调函数(非阻塞)
int qsp_setoutput(QSP * qsp, int(*output)(const char *buf, int len, QSP *qsp, void *user))
{
	if (qsp == NULL || output == NULL)
		return -1;

	qsp->output = output;

	return 0;
}

// 设置时钟回调函数，获取当前系统时间（单位：毫秒）
int qsp_setsystime(QSP * qsp, IUINT32(*systime)(void))
{
	assert(qsp);
	assert(systime);

	qsp->systime = systime;

	return 0;
}

// 设置通信模式
int qsp_setmode(QSP * qsp, IUINT32 mode)
{
	assert(qsp);

	qsp->mode = mode;

	return 0;
}

void test()
{
	QSP *qsp = qsp_create(123456, 0x123456);
	QSPNODE *qnode = malloc_hook(sizeof(QSPNODE));
	qnode->seg.conv = qsp->conv;
	qnode->seg.frg = 100;
	qnode->seg.ts = 1;
	qnode->seg.sn = 1;
	qnode->seg.cmd = QSP_CMD_PUSH;
	qnode->seg.mode = QSP_MODE_WEAK;
	qnode->seg.ver = qsp->ver;
	qnode->seg.len = 1;

	//iqueue_add_tail(&qnode->node, &qsp->snd_buf);
	//qsp_parse_ack(qsp, 100);

	///qnode->seg.frg = 66666;

	//qsp_respond_ack(qsp, qnode);
	//qsp_request_again(qsp, 100);
	//qsp_send_node(qsp, qnode);

	char buf[1024] = { 0 };
	qsp_encode_seg(buf, qnode);

	IUINT32 conv = -1, frg = -1, wnd = -1, sn = -1;
	IUINT16 cmd = -1, mode = -1, ver = -1, len = -1;

	char *p = qsp_decode32u(buf, &conv);
	p = qsp_decode32u(p, &frg);
	p = qsp_decode32u(p, &wnd);
	p = qsp_decode32u(p, &sn);
	p = qsp_decode16u(p, &cmd);
	p = qsp_decode16u(p, &mode);
	p = qsp_decode16u(p, &ver);
	p = qsp_decode16u(p, &len);

}