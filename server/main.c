#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include "c_type.h"
#include "sock.h"

#include "server.h"

#define INFP_DEFAFULT_PORT 45124 // TODO: 配置文件获取
#define INFP_POLL_MAX 20		// 随手写的, 目前只监听12个端口

static infp_t gl_infp = {};
struct pollfd poll_arr[INFP_POLL_MAX];
int curfds = 0;	// 当前pollfd中最大有效下标

int infp_init(void)
{
	int i = 0;

	//初始化链表
	INIT_LIST_HEAD(&gl_infp.dev_list);
	for(i = 0; i < INFP_HASH_MAX; i++)
	{
		INIT_HLIST_HEAD(&gl_infp.dev_hash[i]);
	}

	//初始化端口表
	gl_infp.port = INFP_DEFAFULT_PORT;
	for(i = 0; i < sizeof(gl_infp.port_arr)/sizeof(gl_infp.port_arr[0]); i++)
	{
		gl_infp.port_arr[i] = INFP_DEFAFULT_PORT + (i+1);
	}

	//初始化sock
	if(create_udp(&gl_infp.main_sock, 0, gl_infp.port))
		return -1;

	for(i = 0; i < sizeof(gl_infp.sock_arr)/sizeof(gl_infp.sock_arr[0]); i++)
	{
		if(create_udp(&gl_infp.sock_arr[i], 0, gl_infp.port_arr[i]))
			return -1;
	}

	return 0;
}

int init_poll(void)
{
	int i;
	memset(poll_arr, 0, sizeof(poll_arr));
	for(i = 0; i < INFP_POLL_MAX; i++)
	{
		poll_arr[i].fd = -1;
		curfds = sock_add_poll(poll_arr+i, INFP_POLL_MAX, i ? &gl_infp.sock_arr[i-1] : &gl_infp.main_sock);
		if(curfds < 0)
		{
			return -1;
		}
	}

	return 0;
}

void infp_poll_run(void)
{
	while(1)
	{
		
	}
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

	infp_poll_run();
OUT:
	return 0;
}

