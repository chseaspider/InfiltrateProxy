#include "work.h"

int cli_infp_send(__u32 ip, __u16 port, sock_t* sock, char *data, int len)
{
	struct sockaddr_in addr;
	int socklen = sizeof(addr);

	set_sockaddr_in(&addr, ip, port);
	memxor(data, len);

	return sendto(sock->fd, data, len, 0, (struct sockaddr*)&addr, socklen);
}

int cli_infp_send_login(sock_t* sock, cli_infp_t* infp)
{
	char send_buf[1024];
	__u32 local_ip = get_default_local_ip();

	int len = snprintf(send_buf, sizeof(send_buf)
					, "{\"cmd\":\"login\",\"ip\":\"%s\",\"port\":\"%d\","
					"\"mode\":\"%s\",\"name\":\"%s\"}"
					, IpToStr(local_ip)
					, infp->main_port
					, infp->mode
					, infp->name
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
	int next_hb;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "next_hb");
	if(!j_value || !j_value->valueint)
	{
		CYM_LOG(LV_ERROR, "parse next_hb failed\n");
		goto out;
	}

	gl_cli_infp.state = CLI_INFP_LOGIN;
	gl_cli_infp.next_hb = jiffies + 60 * HZ;

	cli_infp_send_get_nat_type(sock, &gl_cli_infp);
	ret = 0;
out:
	return ret;
}

int cli_infp_do_heart_ack(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	int next_hb;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "next_hb");
	if(!j_value || !j_value->valueint)
	{
		CYM_LOG(LV_ERROR, "parse next_hb failed\n");
		goto out;
	}

	gl_cli_infp.next_hb = jiffies + 60 * HZ;

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
					, "{\"cmd\":\"proxy_request\",\"ip\":\"%s\",\"name\":\"%s\""
						",\"port\":\"%d\",\"num\":\"%d\",\"dst_ip\":\"%s\",\"dst_name\":\"%s\"}"
					, IpToStr(local_ip)
					, infp->name
					, infp->back_port[num]
					, num
					, infp->dst.ip
					, infp->dst.name
					);

	// 介个包, 姑且扔副端口
	return cli_infp_send(infp->server_ip, infp->svr_b_port, sock, send_buf, len);
}

int cli_infp_get_nat_port(sock_t* sock, cli_infp_t* infp)
{
	int try_times = 0;
	int i = 0;
	__u16 port = 0;

	for(i = 0; i < 2; i++)
	{
		if(gl_cli_infp.back_sock[i].fd > 0)
		{
			close_sock(&gl_cli_infp.back_sock[i]);
		}
	}

	for(i = 0; i < 3; i++)
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

	for(i = 0; i < 2; i++)
	{
		gl_cli_infp.back_port[i] = port + i;
		if(create_udp(&gl_cli_infp.back_sock[i], 0, htons(gl_cli_infp.back_port[i]))
			goto try_bind;
	}

	for(i = 0; i < 3; i++)
	{
		gl_cli_infp.proxy_port[i] = port + i + 2;
		if(create_udp(&gl_cli_infp.proxy_sock[i], 0, htons(gl_cli_infp.proxy_port[i])))
			goto try_bind;
	}

	cli_infp_send_get_nat_port(&gl_cli_infp.back_sock[0], infp, 0);
	cli_infp_send_get_nat_port(&gl_cli_infp.back_sock[1], infp, 1);
// 需统计与服务器延迟, 然后在此处进行回应包的收取
}

int cli_infp_do_proxy_ack(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	cJSON* j_value;

	j_value = cJSON_GetObjectItem(root, "mode");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse type failed\n");
		goto out;
	}

	gl_cli_infp.nat_type = atoi(j_value->valuestring);

	CYM_LOG(LV_INFO, "nat_type = %d\n", gl_cli_infp.nat_type);

	cli_infp_get_nat_port(sock, &gl_cli_infp);

	ret = 0;
out:
	return ret;
}

int cli_infp_send_stun_hello(sock_t* sock, cli_infp_t* infp, __u32 ip, __u16 port)
{
	return cli_infp_send(ip, port, sock, "hello", 5);
}

void cli_infp_recv_print(sock_t* sock)
{
	struct sockaddr_in addr;
	// 总会收包报错的
	while(udp_sock_recv(sock, &addr) > 0)
	{
		print("%s\n",sock->recv_buf);
		memset(sock->recv_buf, 0, sizeof(sock->recv_buf));
		sock->recv_len = 0;
	}
}

int cli_infp_do_stun_hello(cli_infp_t* infp, int offset, int mode, __u32 ip, __u16 port)
{
	int i = 0;

	if(mode)
	{
		for(i = 0; i < offset; i++)
		{
			cli_infp_send_stun_hello(&infp->proxy_sock[0], infp, ip, htons(port+i));
		}
		sleep(1);
		cli_infp_recv_print(&infp->proxy_sock[0]);
	}
	else
	{
		for(i = 0; i < offset; i++)
		{
			cli_infp_send_stun_hello(&infp->proxy_sock[i], infp, ip, htons(port));
		}
		sleep(1);
		for(i = 0; i < offset; i++)
			cli_infp_recv_print(&infp->proxy_sock[i]);
	}
}

int cli_infp_do_proxy_task(cJSON* root, struct sockaddr_in *addr, sock_t *sock)
{
	int ret = -1;
	int mode = -1;
	int offset = 0;
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
	port = StrToIp(j_value->valuestring);

	while(1)
	{
		// 尝试打通NAT
		cli_infp_do_stun_hello(&gl_cli_infp, offset, mode, ip, port);
		sleep(10);
	}

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
		cJSON* root = cJSON_Parse(sock->recv_buf);
		if(root)
		{
			cJSON* j_value;
			j_value = cJSON_GetObjectItem(root, "ret");
			if(!j_value || j_value->valueint != 0)
			{
				CYM_LOG(LV_WARNING, "ret error, data:\n%s\n", sock->recv_buf);
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

