#ifndef __SOCK_H__
#define __SOCK_H__

#include "c_type.h"

typedef struct sock_s
{
	int fd;
	int poll_i;

	char* recv_buf;		// ���ջ���
	int recv_buf_len;	// ���ջ����ܴ�С
	int recv_len;		// ��ǰ�ѽ������ݴ�С

	char* send_buf;		// ���ͻ���
	int send_buf_len;	// ���ͻ����ܴ�С
	int send_len;		// ��ǰ���������ݴ�С
}sock_t;

#endif

