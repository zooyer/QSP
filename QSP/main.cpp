#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include <string.h>
using namespace std;

#include "qsp.h"
#include "systime.h"
#include "wrap.h"

#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>
#elif !defined(__unix)
#define __unix
#endif
#ifdef __unix
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#endif

#include <list>
#include <vector>

// 模拟网络
#if 0
// 带延迟的数据包
class DelayPacket
{
public:
	virtual ~DelayPacket() {
		if (_ptr) delete _ptr;
		_ptr = NULL;
	}

	DelayPacket(int size, const void *src = NULL) {
		_ptr = new unsigned char[size];
		_size = size;
		if (src) {
			memcpy(_ptr, src, size);
		}
	}

	unsigned char* ptr() { return _ptr; }
	const unsigned char* ptr() const { return _ptr; }

	int size() const { return _size; }
	IUINT32 ts() const { return _ts; }
	void setts(IUINT32 ts) { _ts = ts; }

protected:
	unsigned char *_ptr;
	int _size;
	IUINT32 _ts;
};

// 均匀分布的随机数
class Random
{
public:
	Random(int size) {
		this->size = 0;
		seeds.resize(size);
	}

	int random() {
		int x, i;
		if (seeds.size() == 0) return 0;
		if (size == 0) {
			for (i = 0; i < (int)seeds.size(); i++) {
				seeds[i] = i;
			}
			size = (int)seeds.size();
		}
		i = rand() % size;
		x = seeds[i];
		seeds[i] = seeds[--size];
		return x;
	}

protected:
	int size;
	std::vector<int> seeds;
};

// 网络延迟模拟器
class LatencySimulator
{
public:

	virtual ~LatencySimulator() {
		clear();
	}

	// lostrate: 往返一周丢包率的百分比，默认 10%
	// rttmin：rtt最小值，默认 60
	// rttmax：rtt最大值，默认 125
	LatencySimulator(int lostrate = 10, int rttmin = 60, int rttmax = 125, int nmax = 1000) :
		r12(100), r21(100) {
		current = iclock();
		this->lostrate = lostrate / 2;	// 上面数据是往返丢包率，单程除以2
		this->rttmin = rttmin / 2;
		this->rttmax = rttmax / 2;
		this->nmax = nmax;
		tx1 = tx2 = 0;
	}

	// 清除数据
	void clear() {
		DelayTunnel::iterator it;
		for (it = p12.begin(); it != p12.end(); it++) {
			delete *it;
		}
		for (it = p21.begin(); it != p21.end(); it++) {
			delete *it;
		}
		p12.clear();
		p21.clear();
	}

	// 发送数据
	// peer - 端点0/1，从0发送，从1接收；从1发送从0接收
	void send(int peer, const void *data, int size) {
		if (peer == 0) {
			tx1++;
			if (r12.random() < lostrate) return;
			if ((int)p12.size() >= nmax) return;
		}
		else {
			tx2++;
			if (r21.random() < lostrate) return;
			if ((int)p21.size() >= nmax) return;
		}
		DelayPacket *pkt = new DelayPacket(size, data);
		current = iclock();
		IUINT32 delay = rttmin;
		if (rttmax > rttmin) delay += rand() % (rttmax - rttmin);
		pkt->setts(current + delay);
		if (peer == 0) {
			p12.push_back(pkt);
		}
		else {
			p21.push_back(pkt);
		}
	}

	// 接收数据
	int recv(int peer, void *data, int maxsize) {
		DelayTunnel::iterator it;
		if (peer == 0) {
			it = p21.begin();
			if (p21.size() == 0) return -1;
		}
		else {
			it = p12.begin();
			if (p12.size() == 0) return -1;
		}
		DelayPacket *pkt = *it;
		current = iclock();
		if (current < pkt->ts()) return -2;
		if (maxsize < pkt->size()) return -3;
		if (peer == 0) {
			p21.erase(it);
		}
		else {
			p12.erase(it);
		}
		maxsize = pkt->size();
		memcpy(data, pkt->ptr(), maxsize);
		delete pkt;
		return maxsize;
	}

public:
	int tx1;
	int tx2;

protected:
	IUINT32 current;
	int lostrate;
	int rttmin;
	int rttmax;
	int nmax;
	typedef std::list<DelayPacket*> DelayTunnel;
	DelayTunnel p12;
	DelayTunnel p21;
	Random r12;
	Random r21;
};

// 模拟网络
LatencySimulator *vnet;

// 模拟网络：模拟发送一个 udp包
int udp_output(const char *buf, int len, QSP *qsp, void *user)
{
	union { int id; void *ptr; } parameter;
	parameter.ptr = user;
	vnet->send(parameter.id, buf, len);
	return len;
}
// 模拟网络：模拟接收一个 udp包
int udp_input(char *buf, int len, QSP *qsp, void *user)
{
	union { int id; void *ptr; } parameter;
	parameter.ptr = user;

	return vnet->recv(parameter.id, buf, len);
}


void test(int mode)
{
	// 创建模拟网络：丢包率10%，Rtt 60ms~125ms
	vnet = new LatencySimulator(10, 60, 125);

	// 创建两个端点的 kcp对象，第一个参数 conv是会话编号，同一个会话需要相同
	// 最后一个是 user参数，用来传递标识
	QSP *qsp1 = qsp_create(0x11223344, (void*)0);
	QSP *qsp2 = qsp_create(0x11223344, (void*)1);

	// 设置kcp的下层输出，这里为 udp_output，模拟udp网络输出函数
	qsp1->input = udp_input;
	qsp2->input = udp_input;
	qsp1->output = udp_output;
	qsp2->output = udp_output;
	qsp1->systime = iclock;
	qsp2->systime = iclock;

	IUINT32 current = iclock();
	IUINT32 slap = current - 100;
	IUINT32 index = 0;
	IUINT32 next = 0;
	IINT64 sumrtt = 0;
	int count = 0;
	int maxrtt = 0;

	// 配置窗口大小：平均延迟200ms，每20ms发送一个包，
	// 而考虑到丢包重发，设置最大收发窗口为128
	//ikcp_wndsize(kcp1, 128, 128);
	//ikcp_wndsize(kcp2, 128, 128);

#if 0
	// 判断测试用例的模式
	if (mode == 0) {
		// 默认模式
		ikcp_nodelay(kcp1, 0, 10, 0, 0);
		ikcp_nodelay(kcp2, 0, 10, 0, 0);
	}
	else if (mode == 1) {
		// 普通模式，关闭流控等
		ikcp_nodelay(kcp1, 0, 10, 0, 1);
		ikcp_nodelay(kcp2, 0, 10, 0, 1);
	}
	else {
		// 启动快速模式
		// 第二个参数 nodelay-启用以后若干常规加速将启动
		// 第三个参数 interval为内部处理时钟，默认设置为 10ms
		// 第四个参数 resend为快速重传指标，设置为2
		// 第五个参数 为是否禁用常规流控，这里禁止
		ikcp_nodelay(kcp1, 1, 10, 2, 1);
		ikcp_nodelay(kcp2, 1, 10, 2, 1);
		kcp1->rx_minrto = 10;
		kcp1->fastresend = 1;
	}
#endif

	char buffer[2000];
	int hr;

	IUINT32 ts1 = iclock();

	while (1) {
		cout << "----" << endl;
		isleep(1);
		current = iclock();
		//ikcp_update(kcp1, iclock());
		//ikcp_update(kcp2, iclock());

		// 每隔 20ms，kcp1发送数据
		for (; current >= slap; slap += 20) {
			((IUINT32*)buffer)[0] = index++;
			((IUINT32*)buffer)[1] = current;

			// 发送上层协议包
			qsp_send(qsp1, buffer, 8);
			qsp_print(&qsp1->snd_queue);
			sleep(20);
		}

		// 处理虚拟网络：检测是否有udp包从p1->p2
		while (1) {
			hr = qsp_recv(qsp2, buffer, 2000);
			//hr = vnet->recv(1, buffer, 2000);
			if (hr < 0) break;
			// 如果 p2收到udp，则作为下层协议输入到kcp2
			//ikcp_input(kcp2, buffer, hr);
		}

		// 处理虚拟网络：检测是否有udp包从p2->p1
		while (1) {
			hr = qsp_recv(qsp1, buffer, 2000);
			//hr = vnet->recv(0, buffer, 2000);
			if (hr < 0) break;
			// 如果 p1收到udp，则作为下层协议输入到kcp1
			//ikcp_input(kcp1, buffer, hr);
		}

		// kcp2接收到任何包都返回回去
		while (1) {
			hr = qsp_recv(qsp2, buffer, 10);
			// 没有收到包就退出
			if (hr < 0) break;
			// 如果收到包就回射
			qsp_send(qsp2, buffer, hr);
		}

		// kcp1收到kcp2的回射数据
		while (1) {
			hr = qsp_recv(qsp1, buffer, 10);
			// 没有收到包就退出
			if (hr < 0) break;
			IUINT32 sn = *(IUINT32*)(buffer + 0);
			IUINT32 ts = *(IUINT32*)(buffer + 4);
			IUINT32 rtt = current - ts;

			if (sn != next) {
				// 如果收到的包不连续
				printf("ERROR sn %d<->%d\n", (int)count, (int)next);
				return;
			}

			next++;
			sumrtt += rtt;
			count++;
			if (rtt > (IUINT32)maxrtt) maxrtt = rtt;

			printf("[RECV] mode=%d sn=%d rtt=%d\n", mode, (int)sn, (int)rtt);
		}
		if (next > 1000) break;
	}

	ts1 = iclock() - ts1;

	qsp_release(qsp1);
	qsp_release(qsp2);

	const char *names[3] = { "default", "normal", "fast" };
	printf("%s mode result (%dms):\n", names[mode], (int)ts1);
	printf("avgrtt=%d maxrtt=%d tx=%d\n", (int)(sumrtt / count), (int)maxrtt, (int)vnet->tx1);
	printf("press enter to next ...\n");
	char ch; scanf("%c", &ch);
}

#endif

// 测试用例
#if 0
void qsp_print(struct IQUEUEHEAD *head);

int output(const char *buf, int len, QSP *qsp, void *user)
{
	printf("[sendto]:%s\n", buf);
	return len;
}

int input(char *buf, int len, QSP *qsp, void *user)
{
	return 10;//fscanf(stdin, buf, len);
}
#endif

// Linux Socket
#if 1

struct user_info
{
	int fd;
	struct sockaddr_in addr;
	socklen_t addrlen;
};

struct test_data
{
	int id;
	unsigned long ctime;
};

void print_mem(const void *buf, int len)
{
	if (buf == NULL || len <= 0)
		return;

	for (int i = 0; i < len; i++)
	{
		printf("%02X", ((char*)buf)[i]);
	}
	printf("\n");
}

int udp_output(const char *buf, int len, QSP *qsp, void *user)
{
	qsp++;
	struct user_info * info = (struct user_info*)user;
	if (rand() % 100 < 0)
	{
		return len;
	}
	int ret = sendto(info->fd, buf, len, 0, (struct sockaddr*)&info->addr, sizeof(info->addr));
	//printf("sendto data : %d byte\n", ret);
	//printf("sendto data:\n");
	//print_mem(buf, ret);
	printf("[send][%s][send to is ok]\n", len == 24 || len == 1 ? "ACK" : "DATA");

	return ret;
}

int udp_input(char *buf, int len, QSP *qsp, void *user)
{
	qsp++;
	struct user_info * info = (struct user_info*)user;
	int ret = recvfrom(info->fd, buf, len, 0, (struct sockaddr*)&info->addr, &info->addrlen);
	//printf("recvfrom data : %d byte\n", ret);
	//printf("recvfrom data:\n");
	//print_mem(buf, ret);
	if (ret == -1 && errno == EAGAIN)
	{
		//isleep(1);
		return 0;
		for (int i = 0; i < rand() % 10 + 10; i++)
		{
			printf("--");
		}
		printf("\n");
		return 0;
	}

	printf("[recv][%s][recv from is ok]\n", ret == 24 || ret == 1 ? "ACK" : "DATA");
	return ret;
}

int init_socket()
{
	int fd;

	fd = Socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8989);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	Bind(fd, (struct sockaddr*)&addr, sizeof(addr));

	return fd;
}

int server()
{
	int fd = init_socket();

	struct sockaddr_in caddr;
	socklen_t addrlen = sizeof(caddr);

	char buf[1024];

	struct user_info user;
	user.fd = fd;
	user.addrlen = sizeof(user.addr);

	QSP *qsp = qsp_create(0xaabbccdd, &user);
	qsp_setinput(qsp, udp_input);
	qsp_setoutput(qsp, udp_output);
	qsp_setsystime(qsp, iclock);
	qsp_setmode(qsp, QSP_MODE_WEAK);

	int flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);

	int ret = -1;

	while (1)
	{
		bzero(buf, 1024);

		//ret = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&caddr, &addrlen);
		//if (ret < 0 && errno != EAGAIN)
		//	perror("recvfrom error");
		ret = qsp_recv(qsp, buf, sizeof(buf));
		if (ret < 0)
			perror("recvfrom error");

		printf("[recv package] : id=%d\n", *(int*)buf);

		//ret = sendto(fd, buf, ret, 0, (struct sockaddr*)&caddr, addrlen);
		//if (ret < 0 && errno != EAGAIN)
		//	perror("sento error");
		ret = qsp_send(qsp, buf, ret);
		if (ret < 0)
			perror("sento error");
	}

	return 0;
}

int client()
{
	int fd, ret;

	fd = Socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in s_addr;
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(8989);
	s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	struct user_info user;
	user.fd = fd;
	user.addr = s_addr;
	user.addrlen = sizeof(user.addr);

	QSP *qsp = qsp_create(0xaabbccdd, &user);
	qsp_setinput(qsp, udp_input);
	qsp_setoutput(qsp, udp_output);
	qsp_setsystime(qsp, iclock);
	qsp_setmode(qsp, QSP_MODE_WEAK);

	struct test_data data, rdata;
	data.id = 0;
	data.ctime = iclock();

	char buf[1024];

	int flag = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);

	IUINT32 lastTime = iclock();

	while (1)
	{
		data.id++;
		data.ctime = iclock();
		if (data.id > 10000)
			break;
			//data.id = 1;

		//ret = sendto(fd, &data, sizeof(data), 0, (struct sockaddr*)&s_addr, sizeof(s_addr));
		//if (ret < 0)
		//	perror("sendto error");
		ret = qsp_send(qsp, &data, sizeof(data));
		if (ret < 0)
			perror("sendto error");

		memset(buf, 0, sizeof(buf));
		//ret = recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL);
		//if (ret < 0 && errno != EAGAIN)
		//	perror("recvfrom error");
		ret = qsp_recv(qsp, buf, sizeof(buf));
		if (ret < 0 && errno != EAGAIN)
			perror("recvfrom error");

		memcpy(&rdata, buf, sizeof(rdata));

		IUINT32 curr_time = iclock();
		printf("[send pack] : id = %d send_time = %lu recv_time = %lu rtt = %lu\n", rdata.id, data.ctime, curr_time, curr_time - rdata.ctime);

		//printf("Please continue!\n");
		//scanf("%s", buf);
		//usleep(1000000);    // 1s = 1000000us = 1000ms
		//usleep(10000);    // 1s = 1000000us = 1000ms
	}

	printf("all use time : %d\n", iclock() - lastTime);

	return 0;
}


void udp_test()
{
	srand((unsigned int)time(NULL));

	int num;

	do
	{
		cout << "please select run: " << endl;
		cout << "1.server" << endl;
		cout << "2.client" << endl;
		cin >> num;
	} while (num < 1 || num > 2);

	switch (num)
	{
	case 1:server(); break;
	case 2:client(); break;
	default:
		break;
	}
}

#endif

int main()
{
	//test();
	//test(QSP_MODE_HALF);

	udp_test();

	//QSP * qsp = qsp_create(0xaabbccdd, NULL);
	//qsp_setoutput(qsp, output);
	//qsp_setinput(qsp, input);
	//qsp_setsystime(qsp, iclock);

	//char buf[4096] = "hello world+++++++++++++++++++++++++++++++++++++++++++++++++";

	//qsp_send(qsp, buf, sizeof(buf));
	//qsp_print(&qsp->snd_queue);

	getchar();
	return EXIT_SUCCESS;
}