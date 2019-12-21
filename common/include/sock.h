#ifndef __SOCK_H__
#define __SOCK_H__

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

#endif

