/*
Copyright 2020 chseasipder

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>

#include "c_type.h"
#include "sock.h"

#include "debug.h"
#include "client.h"
#include "work.h"

int debug_level = 10;

#define INFP_POLL_MAX 20		// 随手写的, 目前只监听12个端口

cli_infp_t gl_cli_infp = {};
struct pollfd poll_arr[INFP_POLL_MAX];
int curfds = 0;	// 当前pollfd中最大有效下标

void infp_timeout(unsigned long data)
{
	cli_infp_t *infp = (cli_infp_t*)data;

	switch(infp->state)
	{
	case CLI_INFP_INIT:
		if(jiffies > infp->next_login)
		{
			cli_infp_send_login(&infp->main_sock, infp);
			infp->next_login = jiffies + 60 * HZ;
		}
		break;
	// 继续执行 CLI_INFP_LOGIN 需执行的内容
	case CLI_INFP_CONNECT:
		if(jiffies - infp->dst.uptime > 10 * HZ)
		{
			// TODO
			//cli_infp_keep_connect();
		}
	case CLI_INFP_LOGIN:
		if(jiffies > infp->next_hb)
		{
			cli_infp_send_heart(&infp->main_sock, infp);
		}
		break;
	default:
		CYM_LOG(LV_ERROR, "???\n");
	}

	mod_timer(&gl_cli_infp.timer, jiffies + HZ);
}

int infp_init(void)
{
	int try_times = 0;

	// 初始化jiffies
	init_timer_module();

	// TODO: modify default server info
	gl_cli_infp.server_ip = StrToIp("192.168.25.101");
	gl_cli_infp.svr_m_port = htons(INFP_DEFAFULT_PORT);
	gl_cli_infp.svr_b_port = htons(INFP_DEFAFULT_PORT+1);

	init_timer(&gl_cli_infp.timer);
	gl_cli_infp.timer.function = infp_timeout;
	gl_cli_infp.timer.data = (unsigned long)&gl_cli_infp;
	add_timer(&gl_cli_infp.timer);

	//初始化sock
	gl_cli_infp.main_sock.fd = -1;
	gl_cli_infp.main_port = (rand() % 35535) + 12000;
	try_times = 0;
	while(create_udp(&gl_cli_infp.main_sock, 0, htons(gl_cli_infp.main_port)) < 0)
	{
		CYM_LOG(LV_WARNING, "bind port %d failed\n", gl_cli_infp.main_port);
		if(try_times++ > 50)
		{
			CYM_LOG(LV_ERROR, "out of udp port???\n");
			return -1;
		}

		gl_cli_infp.main_port = (rand() % 35535) + 12000;
	}
	// 设置非阻塞
	set_sock_nonblock(gl_cli_infp.main_sock.fd);

	return 0;
}

int init_poll(void)
{
	int i;
	memset(poll_arr, 0, sizeof(poll_arr));
	for(i = 0; i < INFP_POLL_MAX; i++)
	{
		poll_arr[i].fd = -1;
	}

	curfds = sock_add_poll(poll_arr, INFP_POLL_MAX, &gl_cli_infp.main_sock);
	if(curfds < 0)
	{
		return -1;
	}

	return 0;
}

int infp_main_recv(sock_t* sock)
{
	struct sockaddr_in addr;
	int ret = 0;
	// 总会收包报错的
	while((ret = udp_sock_recv(sock, &addr)) > 0)
	{
		cli_infp_recv_do(sock, &addr);
	}

	return ret;
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
		if(poll_arr[i].fd == gl_cli_infp.main_sock.fd)
		{
			if(poll_arr[i].events & POLLIN)
			{
				sock_t *sock = &gl_cli_infp.main_sock;

				if(infp_main_recv(sock))
				{
					if(--nready <= 0)
						break;
				}
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
	}

	ret = 0;
out:
	return ret;
}

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("usage: ./client src_name dst_name\n");
		return -1;
	}

	snprintf(gl_cli_infp.name, sizeof(gl_cli_infp.name), "%s", argv[1]);
	snprintf(gl_cli_infp.dst.name, sizeof(gl_cli_infp.dst.name), "%s", argv[2]);

	CYM_LOG(LV_QUIET, "start\n");

	if(infp_init())
	{
		printf("infp_init failed\n");
		goto OUT;
	}

	if(init_poll())
	{
		printf("init_poll failed\n");
		goto OUT;
	}

	// 第一个timer里发
	//cli_infp_send_login(&gl_cli_infp.main_sock, &gl_cli_infp);

	CYM_LOG(LV_QUIET, "init ok\n");
	while(1)
	{
		if(infp_poll_run(30))
			break;

		run_timer_list();
	}
OUT:
	return 0;
}

