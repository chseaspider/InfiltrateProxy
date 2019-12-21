#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "sock.h"
#include "mem.h"

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

	return found ? curfd : -1;
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

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(port);

	if(bind(sock->fd, (struct sockaddr *)&addr, addr_len) < 0)
	{
		perror("bind:");
		return -1;
	}

	return sock->fd;
}


