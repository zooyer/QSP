/**********************************************************
 *
 * Author        : zhangzhongyuan
 * Email         : zzyservers@163.com
 * Create time   : 2016-07-01 23:48
 * Last modified : 2016-07-02 00:52
 * Filename      : wrap.c
 * Description   : socket函数族进行错误处理进行封装
 *
 * *******************************************************/

#include "wrap.h"

/* 打印错误原因并退出 */
void perr_exit(const char *err)
{
    perror(err);
    exit(1);

}

/* 错误处理动作 */
void err_action(const char *errinfo)
{
    // TODO error action or log
    
    perr_exit(errinfo);
}

/* accept出错封装 */
int Accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    int cfd;//保存用于通信的socket文件描述符
    while ( (cfd = accept(fd, addr, addrlen)) < 0 )
    {
        //如果连接终止 或 被信号中断 则再试一次
        if ((errno != ECONNABORTED) && (errno != EINTR))
            err_action("accept error");
    }
    
    return cfd;
}

/* 绑定IP、端口号和socket,出错则返回错误信息 */
int Bind(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret = bind(fd, addr, addrlen);
    if (ret < 0)
        err_action("bind error");

    return ret;
}

/* 客户机连接服务器函数封装,错误则打印出错信息 */
int Connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret = connect(fd, addr, addrlen);
    if (ret < 0)
        err_action("connect error");

    return ret;
}

/* listen函数出错处理封装 */
int Listen(int fd, int backlog)
{
    int ret = listen(fd, backlog);
    if (ret < 0)
        err_action("listen error");

    return ret;
}

/* 建立套接字socket出错处理封装 */
int Socket(int family, int type, int protocol)
{
    //保存建立socket时返回的文件描述符
    int fd = socket(family, type, protocol);
    if (fd < 0)
        err_action("socket error");

    return fd;
}

/* close关闭文件出错处理封装 */
int Close(int fd)
{
    int ret = close(fd);
    if (ret == -1)
        err_action("close error");

    return ret;
}

/* read函数出错处理封装 */
ssize_t Read(int fd, void *buf, size_t count)
{
    ssize_t ret;//读到的字节数
    while( (ret = read(fd, buf, count)) == -1 )
    {
        //如果连接终止 或 被信号中断 则再试一次
        if ((errno != ECONNABORTED) && (errno != EINTR))
            err_action("read error");
    }

    return ret;
}

/* write函数出错处理封装 */
ssize_t Write(int fd, const void *buf, size_t count)
{
    ssize_t ret;//被写入的字节数
    while ( (ret = write(fd, buf, count)) == -1 )
    {
        //如果连接终止 或 被信号中断 则再试一次
        if ((errno != ECONNABORTED) && (errno != EINTR))
            err_action("write error");
    }

    return ret;
}

/* 至少读满n个字符再返回 */
ssize_t Readn(int fd, void *buf, size_t n)
{
    size_t nleft;   //剩下的字符数
    ssize_t nread;  //已读的字符数
    char *ptr;      //读写指针

    /* 初始化 */
    ptr = (char*)buf;
    nleft = n;

    /* 至少读满n个字节返回,或出错返回 */
    while (nleft > 0)
    {
        if ( (nread = read(fd, ptr, nleft)) < 0 )//如果读出错
        {
            if (errno == EINTR)//是由于信号中断的话 重新读取
                continue;
            else
                return -1;

        }
        else if (nread == 0)//信号中断 或 读到空 则退出
            break;

        nleft -= nread;//记录每次读完,还剩的未读的字节数
        ptr += nread;  //为将下一次读到的值补充到这次的末尾做准备

    }

    return n - nleft;  //返回已读到的字节数
}

/* 写满n个字符才返回 */
ssize_t Writen(int fd, const void *buf, size_t n)
{
    size_t nleft;       //剩下还要写的字节个数
    ssize_t nwritten;   //已经写的字节个数
    const char *ptr;    //写指针

    /* 初始化 */
    ptr = (char*)buf;
    nleft = n;

    while (nleft > 0)
    {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0 )
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;
            else
                return -1;

        }

        nleft -= nwritten;  //写完这次之后,剩下还要写的字节个数
        ptr += nwritten;    //写完这次之后,从这次的末尾继续写

    }
    return n;

}

/* 一次读一行 */
ssize_t Readline(int fd, void *vptr, size_t maxlen)
{
    size_t n, rc;
    char c, *ptr;

    ptr = (char*)vptr;
    for (n = 1; n < maxlen; n++) {
        if ( (rc = my_read(fd, &c)) == 1 )//获取一个字符
        {
            *ptr++ = c;
            if (c == '\n')//如果是换行就退出
                break;

        } else if (rc == 0)//如果读完了就返回读到的字节数
        {
            *ptr = 0;
            return n - 1;

        } else
            return -1;

    }
    *ptr = 0;
    return n;

}

/*返回第一次调用时字符串的一个字符,调用一次返回下一个字符,供下面的函数调用*/
/* fd为要读的文件 ptr为要返回的字符(为传出参数) */
static ssize_t my_read(int fd, char *ptr)
{
    /* 定义静态变量,上一次调用的值会被保存 */
    static int read_cnt;    //剩下待返回的字符串的长度,系统将其初始化为0
    static char *read_ptr;  //当前读到的位置
    static char read_buf[128];  //读到的字符串

    if (read_cnt <= 0)
    {
again:
        if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0 )
        {
            if (errno == EINTR)//如果是被信号中断 则再试一次
                goto again;
            return -1;

        } else if (read_cnt == 0)//如果读到的字节数为0(即读到的文件为空)
            return 0;

        /* 否则有读到内容 则将读指针指向该字符串的首部 */
        read_ptr = read_buf;

    }

    //返回当前指向的字符,之后指向下一个字符,供下一调用时使用
        *ptr = *read_ptr++;
            read_cnt--;//剩下待返回的字符串的长度减一
                return 1;//调用成功返回1
    
}
