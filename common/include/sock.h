#ifndef __SOCK_H__
#define __SOCK_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>

#include "c_type.h"

typedef struct sock_s
{
	int fd;
	int poll_i;

	char* recv_buf;		// 接收缓存
	int recv_buf_len;	// 接收缓存总大小
	int recv_len;		// 当前已接收数据大小

	char* send_buf;		// 发送缓存
	int send_buf_len;	// 发送缓存总大小
	int send_len;		// 当前待发送数据大小
}sock_t;

static inline void poll_add_write(struct pollfd* _poll)
{
	_poll->events |= POLLOUT;
}

static inline void poll_del_write(struct pollfd* _poll)
{
	_poll->events &= (~POLLOUT);
}

static inline void poll_add_read(struct pollfd* _poll)
{
	_poll->events |= POLLIN;
}

static inline void poll_del_read(struct pollfd* _poll)
{
	_poll->events &= (~POLLIN);
}

static inline int set_sock_block(int fd)
{
	//设置套接字非阻塞
	int ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
	if(ret == -1)
	{
		perror("set sock block:");
	}

	return ret;
}

static inline int set_sock_nonblock(int fd)
{
	//设置套接字非阻塞
	int ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	if(ret == -1)
	{
		perror("set sock nonblock:");
	}

	return ret;
}

int sock_add_poll(struct pollfd* _poll, int max, sock_t* sock);
int sock_del_poll(struct pollfd* _poll, int max, sock_t* sock);
int create_udp(sock_t *sock, __u32 ip, __u16 port);

#endif

