/**********************************************************
 *
 * Author        : zhangzhongyuan
 * Email         : zzyservers@163.com
 * Create time   : 2016-07-01 23:48
 * Last modified : 2016-07-01 23:48
 * Filename      : wrap.h
 * Description   : socket函数族进行错误处理进行封装
 *
 * *******************************************************/

#ifndef __WRAP_H_
#define __WRAP_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

	void perr_exit(const char *err);
	void err_action(const char *errinfo);
	int Accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
	int Bind(int fd, const struct sockaddr *addr, socklen_t addrlen);
	int Connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
	int Listen(int fd, int backlog);
	int Socket(int family, int type, int protocol);
	int Close(int fd);
	ssize_t Read(int fd, void *buf, size_t count);
	ssize_t Write(int fd, const void *buf, size_t count);
	ssize_t Readn(int fd, void *vptr, size_t n);
	ssize_t Writen(int fd, const void *vptr, size_t n);
	ssize_t Readline(int fd, void *vptr, size_t maxlen);
	static ssize_t my_read(int fd, char *ptr);

#ifdef __cplusplus
}
#endif

#endif
