#include <stdio.h>
#include <string.h>

#include "sock.h"
#include "mem.h"

#define SOCK_BUF_LEN 102400

int sock_add_poll(struct pollfd* _poll, int max, sock_t* sock)
{
	int curfd = 0;
	int i, found = 0;
	for(i = curfd; i < max; i++)
	{
		if(_poll[i].fd == -1)
		{
			_poll[i].fd = sock->fd;
			sock->poll_i = i;
			poll_add_read(&_poll[i]);
			found = 1;
		}
	}

	if(found)
	{
		for(i = max; i >= 0; i--)
		{
			if(_poll[i].fd != -1)
			{
				curfd = i;
				break;
			}
		}
	}
	else
	{
		printf("poll is full\n");
	}

	return found ? curfd+1 : -1;
}

int sock_del_poll(struct pollfd* _poll, int max, sock_t* sock)
{
	int curfd = 0;
	int i;

	memset(&_poll[sock->poll_i], 0, sizeof(_poll[sock->poll_i]));
	_poll[sock->poll_i].fd = -1;

	for(i = max; i >= 0; i--)
	{
		if(_poll[i].fd != -1)
		{
			curfd = i;
			break;
		}
	}

	return curfd;
}

// IP 网络序, PORT 网络序
void set_sockaddr_in(struct sockaddr_in *addr, __u32 ip, __u16 port)
{
	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = ip;
	addr->sin_port = port;
}

int udp_sock_send(sock_t *sock, void* data, int data_len, __u32 ip, __u16 port)
{
	int ret = 0;
	struct sockaddr_in addr; 
	socklen_t addr_len = sizeof(addr);

	if(!data || !data_len)
	{
		printf("data is null or data_len is zero\n");
		return 0;
	}

	set_sockaddr_in(&addr, ip, port);

	ret = sendto(sock->fd, data, data_len, 0, (struct sockaddr *)&addr, addr_len);
	if(ret <= 0)
		return ret;

	if(ret < data_len)
	{
		int len = data_len - ret;
		while(sock->send_buf_len < len)
		{
			sock->send_buf = mem_realloc(sock->send_buf, sock->send_buf_len + SOCK_BUF_LEN);
			if(!sock->send_buf)
			{
				printf("realloc failed\n");
				abort();
			}

			sock->send_buf_len += SOCK_BUF_LEN;
		}
		memcpy(sock->send_buf+sock->send_len, data+ret, len);
	}

	return ret;
}

int udp_sock_recv(sock_t *sock, struct sockaddr_in *addr)
{
	int ret = 0;
	socklen_t addr_len = sizeof(*addr);

	while(sock->recv_buf_len - sock->recv_len < SOCK_BUF_LEN)
	{
		sock->recv_buf = mem_realloc(sock->recv_buf, sock->recv_buf_len + SOCK_BUF_LEN);
		if(!sock->recv_buf)
		{
			printf("realloc failed\n");
			abort();
		}

		sock->recv_buf_len += SOCK_BUF_LEN;
	}

	ret = recvfrom(sock->fd, sock->recv_buf+sock->recv_len, SOCK_BUF_LEN, 0, (struct sockaddr *)addr, &addr_len);

	return ret;
}


int create_udp(sock_t *sock, __u32 ip, __u16 port)
{
	struct sockaddr_in addr; 
	socklen_t addr_len = sizeof(addr);

	if(!sock)
		return -1;

	if(sock->recv_buf)
		mem_free(sock->recv_buf);

	if(sock->send_buf)
		mem_free(sock->send_buf);

	memset(sock, 0, sizeof(sock_t));

	sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock->fd < 0)
	{
		perror("socket:");
		return -1;
	}

	set_sockaddr_in(&addr, ip, port);

	if(bind(sock->fd, (struct sockaddr *)&addr, addr_len) < 0)
	{
		perror("bind:");
		return -1;
	}

	return sock->fd;
}


