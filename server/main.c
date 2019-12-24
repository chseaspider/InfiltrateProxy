#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>

#include "c_type.h"
#include "sock.h"

#include "server.h"
#include "work.h"

int debug_level = 0;

#define INFP_DEFAFULT_PORT 45124 // TODO: 配置文件获取
#define INFP_POLL_MAX 20		// 随手写的, 目前只监听12个端口

infp_t gl_infp = {};
struct pollfd poll_arr[INFP_POLL_MAX];
int curfds = 0;	// 当前pollfd中最大有效下标

void infp_timeout(unsigned long data)
{
	infp_cli_t *cli;
	struct list_head *pos, *n;
	__u32 now_time = jiffies;

	list_for_each_safe(pos, n, &gl_infp.dev_list)
	{
		cli = list_entry(pos, infp_cli_t, list_to);
		if(now_time - cli->uptime > 180 * HZ)
		{
			infp_del_cli(cli);
		}
	}

	mod_timer(&gl_infp.timer, jiffies + HZ);
}

int infp_init(void)
{
	int i = 0;

	// 初始化jiffies
	init_timer_module();

	init_timer(&gl_infp.timer);
	gl_infp.timer.function = infp_timeout;
	add_timer(&gl_infp.timer);

	//初始化链表
	INIT_LIST_HEAD(&gl_infp.dev_list);
	for(i = 0; i < INFP_HASH_MAX; i++)
	{
		INIT_HLIST_HEAD(&gl_infp.dev_hash[i]);
	}

	//初始化端口表
	gl_infp.main_port = INFP_DEFAFULT_PORT;
	gl_infp.back_port = INFP_DEFAFULT_PORT + 1;

	//初始化sock
	if(create_udp(&gl_infp.main_sock, 0, htons(gl_infp.main_port)))
		return -1;
	// 设置非阻塞
	set_sock_nonblock(gl_infp.main_sock.fd);

	if(create_udp(&gl_infp.back_sock, 0, htons(gl_infp.back_port)))
		return -1;
	// 设置非阻塞
	set_sock_nonblock(gl_infp.back_sock.fd);

	return 0;
}

int init_poll(void)
{
	int i;
	memset(poll_arr, 0, sizeof(poll_arr));
	curfds = sock_add_poll(&poll_arr[0], INFP_POLL_MAX, &gl_infp.main_sock);
	if(curfds < 0)
	{
		return -1;
	}

	curfds = sock_add_poll(&poll_arr[1], INFP_POLL_MAX, &gl_infp.back_sock);
	if(curfds < 0)
	{
		return -1;
	}

	for(i = 2; i < INFP_POLL_MAX; i++)
	{
		poll_arr[i].fd = -1;
	}

	return 0;
}

void infp_main_recv(sock_t* sock)
{
	struct sockaddr_in addr;
	// 总会收包报错的
	while(udp_sock_recv(sock, &addr) > 0)
	{
		infp_recv_do(sock, &addr);
	}
}

int infp_poll_run(int timeout)
{
	int ret = -1;
	int nready = 0, i = 0;
	nready = poll(poll_arr, curfds, timeout);
	if (nready == -1)
	{
		perror("poll error:");
		abort();
	}

	for(i = 0; i < curfds; i++)
	{
		if(poll_arr[i].fd == gl_infp.main_sock.fd
			|| poll_arr[i].fd == gl_infp.back_sock.fd)
		{
			if(poll_arr[i].events & POLLIN)
			{
				sock_t *sock = NULL;
				if(poll_arr[i].fd == gl_infp.main_sock.fd)
					sock = &gl_infp.main_sock;
				else if(poll_arr[i].fd == gl_infp.main_sock.fd)
					sock = &gl_infp.back_sock;
				else
					goto out;	// 没这种情况

				infp_main_recv(sock);
			}

			// 没有POLLOUT这个说法, 直接sendto
			if(poll_arr[i].events & POLLERR)
			{
				goto out;
			}
		}
		else
		{
			printf("???\n");
			goto out;
		}

		if(--nready <= 0)
			break;
	}

	ret = 0;
out:
	return ret;
}

int main(int argc, char *argv[])
{
	if(infp_init())
	{
		printf("infp_init failed\n");
		goto OUT;
	}

	if(init_poll())
	{
		printf("init_poll failed\n");
	}

	while(1)
	{
		if(infp_poll_run(30))
			break;

		run_timer_list();
	}
OUT:
	return 0;
}
