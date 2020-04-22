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

#include <stdlib.h>
#include <unistd.h>

#include "work.h"
#include "cJSON.h"
#include "debug.h"

void memxor(unsigned char* data, int len)
{
	int i = 0;
	for(i = 0; i < len; i++)
		data[i] ^= 0x71;
}

int cli_infp_send(__u32 ip, __u16 port, sock_t* sock, char *data, int len)
{
	int ret;
	struct sockaddr_in addr;
	int socklen = sizeof(addr);

	set_sockaddr_in(&addr, ip, port);
	CYM_LOG(LV_DEBUG, "send [%s]\n", data);
	memxor((__u8*)data, len);

	ret = sendto(sock->fd, data, len, 0, (struct sockaddr*)&addr, socklen);
	return ret;
}

int cli_infp_send_login(sock_t* sock, cli_infp_t* infp)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"login\",\"ip\":\"%s\",\"port\":\"%d\","
					"\"mode\":\"%s\",\"name\":\"%s\",\"allow_tcp\":\"1\"}"
					, IpToStr(local_ip)
					, infp->main_port
					, "client"
					, infp->name
					);

	return cli_infp_send(infp->server_ip, infp->svr_m_port, sock, send_buf, len);
}

int cli_infp_send_heart(sock_t* sock, cli_infp_t* infp)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"heart_beat\",\"ip\":\"%s\",\"name\":\"%s\",\"connected\":\"%d\"}"
					, IpToStr(local_ip)
					, infp->name
					, (infp->dst.uptime && jiffies - infp->dst.uptime < 10 * HZ) ? 1 : 0
					);

	return cli_infp_send(infp->server_ip, infp->svr_m_port, sock, send_buf, len);
}

int cli_infp_send_get_nat_type(sock_t* sock, cli_infp_t* infp)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"get_nat_type\",\"ip\":\"%s\",\"port\":\"%d\",\"name\":\"%s\"}"
					, IpToStr(local_ip)
					, infp->main_port
					, infp->name
					);

	return cli_infp_send(infp->server_ip, infp->svr_b_port, sock, send_buf, len);
}

int cli_infp_do_login_ack(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "next_hb");
	if(!j_value || !j_value->valueint)
	{
		CYM_LOG(LV_ERROR, "parse next_hb failed\n");
		goto out;
	}

	gl_cli_infp.state = CLI_INFP_LOGIN;
	gl_cli_infp.next_hb = jiffies + j_value->valueint * HZ;

	cli_infp_send_get_nat_type(sock, &gl_cli_infp);
	ret = 0;
out:
	return ret;
}

int cli_infp_do_heart_ack(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "next_hb");
	if(!j_value || !j_value->valueint)
	{
		CYM_LOG(LV_ERROR, "parse next_hb failed\n");
		goto out;
	}

	gl_cli_infp.next_hb = jiffies + j_value->valueint * HZ;

	ret = 0;
out:
	return ret;
}

int cli_infp_send_proxy_request(sock_t* sock, cli_infp_t* infp)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"proxy_request\",\"ip\":\"%s\",\"name\":\"%s\""
						",\"dst_ip\":\"%s\",\"dst_name\":\"%s\"}"
					, IpToStr(local_ip)
					, infp->name
					, infp->dst.ip
					, infp->dst.name
					);

	return cli_infp_send(infp->server_ip, infp->svr_m_port, sock, send_buf, len);
}

int cli_infp_do_nat_type_ack(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "type");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse type failed\n");
		goto out;
	}

	gl_cli_infp.nat_type = atoi(j_value->valuestring);

	CYM_LOG(LV_INFO, "nat_type = %d\n", gl_cli_infp.nat_type);

	cli_infp_send_proxy_request(sock, &gl_cli_infp);

	ret = 0;
out:
	return ret;
}

int cli_infp_send_get_nat_port(sock_t* sock, cli_infp_t* infp, int num)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"get_nat_port\",\"ip\":\"%s\",\"name\":\"%s\""
						",\"port\":\"%d\",\"num\":\"%d\",\"dst_ip\":\"%s\",\"dst_name\":\"%s\"}"
					, IpToStr(local_ip)
					, infp->name
					, infp->proxy_port[num]
					, num
					, infp->dst.ip
					, infp->dst.name
					);

	// 介个包, 姑且扔副端口
	return cli_infp_send(infp->server_ip, infp->svr_b_port, sock, send_buf, len);
}

int cli_infp_send_proxy_task_ack(sock_t* sock, cli_infp_t* infp, int ret)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"proxy_task_ack\",\"ip\":\"%s\",\"name\":\"%s\""
						",\"dst_ip\":\"%s\",\"dst_name\":\"%s\",\"ret\":\"%d\"}"
					, IpToStr(local_ip)
					, infp->name
					, infp->dst.ip
					, infp->dst.name
					, ret
					);

	// 介个包, 姑且扔副端口
	return cli_infp_send(infp->server_ip, infp->svr_b_port, sock, send_buf, len);
}


int cli_infp_send_get_tcp_nat_port(sock_t* sock, cli_infp_t* infp, int num)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"get_tcp_nat_port\",\"ip\":\"%s\",\"name\":\"%s\""
						",\"port\":\"%d\",\"num\":\"%d\",\"dst_ip\":\"%s\",\"dst_name\":\"%s\"}"
					, IpToStr(local_ip)
					, infp->name
					, infp->proxy_port[num]
					, num
					, infp->dst.ip
					, infp->dst.name
					);

	// 介个包, 只能扔主端口
	return cli_infp_send(infp->server_ip, infp->svr_m_port, sock, send_buf, len);
}

int cli_infp_get_nat_port(sock_t* sock, cli_infp_t* infp, int tcp_mode)
{
	int try_times = 0;
	int i = 0;
	__u16 port = 0;

	for(i = 0; i < 4; i++)
	{
		if(gl_cli_infp.proxy_sock[i].fd > 0)
		{
			close_sock(&gl_cli_infp.proxy_sock[i]);
		}
	}

	try_times = 0;
try_bind:
	if(try_times++ > 50)
	{
		CYM_LOG(LV_ERROR, "out of udp port???\n");
		return -1;
	}

	port = (rand() % 35535) + 12000;
	for(i = 0; i < 4; i++)
	{
		gl_cli_infp.proxy_port[i] = port + i;
		if(tcp_mode)
		{
			if(create_tcp(&gl_cli_infp.proxy_sock[i], 0, htons(gl_cli_infp.proxy_port[i]), 0) < 0)
				goto try_bind;
		}
		else
		{
			if(create_udp(&gl_cli_infp.proxy_sock[i], 0, htons(gl_cli_infp.proxy_port[i])) < 0)
				goto try_bind;
			set_sock_nonblock(gl_cli_infp.proxy_sock[i].fd);
		}
	}

	if(tcp_mode)
	{
		if(tcp_just_connect(gl_cli_infp.proxy_sock[0].fd, gl_cli_infp.server_ip, gl_cli_infp.svr_m_port, 3))
			goto try_bind;
		cli_infp_send_get_tcp_nat_port(&gl_cli_infp.proxy_sock[0], infp, 0);
	}
	else
	{
		cli_infp_send_get_nat_port(&gl_cli_infp.proxy_sock[0], infp, 0);
		cli_infp_send_get_nat_port(&gl_cli_infp.proxy_sock[1], infp, 1);
	}
// 需统计与服务器延迟, 然后在此处进行回应包的收取
	return 0;
}

int cli_infp_do_proxy_ack(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	cJSON* j_value = NULL;
	int tcp_mode = 0;

	j_value = cJSON_GetObjectItem(root, "dst_ip");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "not dst_ip!\n");
		goto OUT;
	}
	snprintf(gl_cli_infp.dst.ip, sizeof(gl_cli_infp.dst.ip), "%s", j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "dst_name");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "not dst_ip!\n");
		goto OUT;
	}
	snprintf((char*)gl_cli_infp.dst.name, sizeof(gl_cli_infp.dst.name), "%s", j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "mode");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "not mode!\n");
		goto OUT;
	}
	if(!strcmp(j_value->valuestring, "tcp"))
		tcp_mode = 1;

OUT:
	return cli_infp_get_nat_port(sock, &gl_cli_infp, tcp_mode);
}

int cli_infp_send_stun_hello(sock_t* sock, cli_infp_t* infp, __u32 ip, __u16 port)
{
	char send_buf[256] = "hello";
	return cli_infp_send(ip, port, sock, send_buf, 5);
}

void cli_infp_recv_print_send(sock_t *sock)
{
	static __u32 last_send = 0;
	static __u32 send_count = 0;
	struct sockaddr_in addr;
	// 总会收包报错的
	send(sock->fd, "hello", 5, 0);
	printf("send hello\n");
	last_send = jiffies;
	send_count++;

	while(udp_sock_recv(sock, &addr) > 0)
	{
		printf("recv %s\n",sock->recv_buf);
		if(!strcmp((char*)sock->recv_buf, "world"))
		{
			printf("%lu ms\n", jiffies - last_send);
			send_count--;
		}
		else
		{
			send(sock->fd, "world", 5, 0);
		}
		memset(sock->recv_buf, 0, sock->recv_buf_len);
		sock->recv_len = 0;

		if(send_count <= 0)
		{
			break;
		}
	}
}

void cli_infp_recv_print(sock_t* sock)
{
	struct sockaddr_in addr;
	// 总会收包报错的
	while(udp_sock_recv(sock, &addr) > 0)
	{
		memxor(sock->recv_buf, sock->recv_len);
		printf("recv %s\n",sock->recv_buf);
		memset(sock->recv_buf, 0, sock->recv_buf_len);
		sock->recv_len = 0;
	}
	printf("%d done\n", sock->fd);
}

int cli_infp_do_stun_hello(cli_infp_t* infp, int offset, int mode, __u32 ip, __u16 port, int listen)
{
	int i = 0;
	int old_ttl = -1;
	int new_ttl = 10;

	if(listen)
	{
		if(mode)
		{
			get_sock_ttl(infp->proxy_sock[0].fd, &old_ttl);
			set_sock_ttl(infp->proxy_sock[0].fd, &new_ttl);
			for(i = 0; i < offset; i++)
			{
				printf("sendto %s:%d\n", IpToStr(ip), port+1);
				cli_infp_send_stun_hello(&infp->proxy_sock[0], infp, ip, htons(port+i));
			}
			set_sock_ttl(infp->proxy_sock[0].fd, &old_ttl);
			cli_infp_send_proxy_task_ack(&infp->main_sock, infp, 3);
			sleep(1);
			cli_infp_recv_print(&infp->proxy_sock[0]);
		}
		else
		{
			for(i = 0; i < offset; i++)
			{
				get_sock_ttl(infp->proxy_sock[i].fd, &old_ttl);
				set_sock_ttl(infp->proxy_sock[i].fd, &new_ttl);
				printf("sendto %s:%d\n", IpToStr(ip), port);
				cli_infp_send_stun_hello(&infp->proxy_sock[i], infp, ip, htons(port));
				set_sock_ttl(infp->proxy_sock[i].fd, &old_ttl);
			}
			cli_infp_send_proxy_task_ack(&infp->main_sock, infp, 3);
			sleep(1);
			for(i = 0; i < offset; i++)
				cli_infp_recv_print(&infp->proxy_sock[i]);
		}
	}
	else
	{
		if(mode)
		{
			for(i = 0; i < offset; i++)
			{
				printf("sendto %s:%d\n", IpToStr(ip), port+1);
				cli_infp_send_stun_hello(&infp->proxy_sock[0], infp, ip, htons(port+i));
			}
			sleep(1);
			cli_infp_recv_print(&infp->proxy_sock[0]);
		}
		else
		{
			for(i = 0; i < offset; i++)
			{
				printf("sendto %s:%d\n", IpToStr(ip), port);
				cli_infp_send_stun_hello(&infp->proxy_sock[i], infp, ip, htons(port));
			}
			sleep(1);
			for(i = 0; i < offset; i++)
				cli_infp_recv_print(&infp->proxy_sock[i]);
		}
	}

	return 0;
}

int cli_infp_do_tcp_stun_hello(cli_infp_t* infp, int offset, int mode, __u32 ip, __u16 port, int listen)
{
	int i = 0;
	int ttl = 10;

	// 作服务端的
	if(listen)
	{
		if(mode)
		{
			for(i = 0; i < offset; i++)
			{
				// 第二个端口开始连
				infp_try_connect(gl_cli_infp.ip, IpToStr(ip), infp->proxy_port[1], port+i, ttl);
			}
		}
		else
		{
			for(i = 0; i < offset; i++)
			{
				// 同样第二个端口开连
				infp_try_connect(gl_cli_infp.ip, IpToStr(ip), infp->proxy_port[i+1], port, ttl);
			}
		}

		for(i = 0; i < 4; i++)
		{
			close_sock(&infp->proxy_sock[i]);
			create_tcp(&infp->proxy_sock[i], 0, 0, 1);
		}

		cli_infp_send_proxy_task_ack(&infp->main_sock, infp, 2);

		while(1)
		{
			if(mode)
			{
				sock_t* new_sock = tcp_accept(&infp->proxy_sock[1]);
				while(new_sock)
				{
					cli_infp_recv_print_send(new_sock);
					sleep(1);
				}
			}
			else
			{
				for(i = 0; i < offset; i++)
				{
					sock_t* new_sock = tcp_accept(&infp->proxy_sock[i+1]);
					while(new_sock)
					{
						cli_infp_recv_print_send(new_sock);
						sleep(1);
					}
				}
			}
		}
	}
	else
	{
		// 客户端, 瞎连就完事儿
		if(mode)
		{
			for(i = 0; i < offset; i++)
			{
				while(!tcp_just_connect(infp->proxy_sock[1].fd, ip, htons(port+i), 0))
				{
					cli_infp_send_proxy_task_ack(&infp->main_sock, infp, 1);
					while(1)
					{
						cli_infp_recv_print_send(&infp->proxy_sock[1]);
						sleep(1);
					}
				}
			}
		}
		else
		{
			for(i = 0; i < offset; i++)
			{
				while(!tcp_just_connect(infp->proxy_sock[i+1].fd, ip, htons(port), 0))
				{
					cli_infp_send_proxy_task_ack(&infp->main_sock, infp, 1);
					while(1)
					{
						cli_infp_recv_print_send(&infp->proxy_sock[i+1]);
						sleep(1);
					}
				}
			}
		}
	}

	return 0;
}


int cli_infp_do_proxy_task(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	int mode = -1;
	int offset = 0;
	int listen = 0;
	__u32 ip = 0;
	__u16 port = 0;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "mode");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse mode failed\n");
		goto out;
	}
	mode = atoi(j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "offset");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse offset failed\n");
		goto out;
	}
	offset = atoi(j_value->valuestring);

	if(offset > 3)
	{
		CYM_LOG(LV_ERROR, "offset [%d] is too big\n", offset);
		goto out;
	}

	j_value = cJSON_GetObjectItem(root, "dst_ip");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse dst_ip failed\n");
		goto out;
	}
	ip = StrToIp(j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "guess_port");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse offset failed\n");
		goto out;
	}
	port = atoi(j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "main");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse main failed\n");
		goto out;
	}
	listen = atoi(j_value->valuestring);

	while(1)
	{
		// 尝试打通NAT
		cli_infp_do_stun_hello(&gl_cli_infp, offset, mode, ip, port, listen);
	}

	ret = 0;
out:
	return ret;
}

int cli_infp_do_proxy_tcp_task(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	int mode = -1;
	int offset = 0;
	__u32 ip = 0;
	__u16 port = 0;
	int listen = 0;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "mode");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse mode failed\n");
		goto out;
	}
	mode = atoi(j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "offset");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse offset failed\n");
		goto out;
	}
	offset = atoi(j_value->valuestring);

	if(offset > 3)
	{
		CYM_LOG(LV_ERROR, "offset [%d] is too big\n", offset);
		goto out;
	}

	j_value = cJSON_GetObjectItem(root, "dst_ip");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse dst_ip failed\n");
		goto out;
	}
	ip = StrToIp(j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "guess_port");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse guess_port failed\n");
		goto out;
	}
	port = atoi(j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "main");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse main failed\n");
		goto out;
	}
	listen = atoi(j_value->valuestring);

	// 尝试打通NAT
	cli_infp_do_tcp_stun_hello(&gl_cli_infp, offset, mode, ip, port, listen);

	ret = 0;
out:
	return ret;
}


int cli_infp_recv_do(sock_t *sock, struct sockaddr_in *addr)
{
	int ret = -1;

	if(sock->recv_buf && sock->recv_len)
	{
		memxor(sock->recv_buf, sock->recv_len);
		CYM_LOG(LV_DEBUG, "recv [%s]\n", sock->recv_buf);
		cJSON* root = cJSON_Parse((char*)sock->recv_buf);
		if(root)
		{
			cJSON* j_value = cJSON_GetObjectItem(root, "ret");
			if(!j_value || j_value->valueint != 0)
			{
				CYM_LOG(LV_WARNING, "ret error, data:\n%s\n", sock->recv_buf);
				goto next;
			}

			j_value = cJSON_GetObjectItem(root, "cmd");
			if(j_value && j_value->valuestring)
			{
				if(!strcmp(j_value->valuestring, "login_ack"))
					ret = cli_infp_do_login_ack(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "heart_ack"))
					ret = cli_infp_do_heart_ack(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "nat_type_ack"))
					ret = cli_infp_do_nat_type_ack(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "proxy_ack"))
					ret = cli_infp_do_proxy_ack(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "proxy_task"))
					ret = cli_infp_do_proxy_task(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "proxy_tcp_task"))
					ret = cli_infp_do_proxy_tcp_task(root, addr, sock);
				else
				{
					CYM_LOG(LV_WARNING,"unknown cmd [%s]\n", j_value->valuestring);
				}
			}
		next:
			cJSON_Delete(root);
		}
		else
		{
			CYM_LOG(LV_WARNING, "json parse error, data:\n%s\n", sock->recv_buf);
		}
	}

	memset(sock->recv_buf, 0, sock->recv_buf_len);
	sock->recv_len = 0;
	return ret;
}

