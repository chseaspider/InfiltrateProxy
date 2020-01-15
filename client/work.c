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

int cli_send_get_nat_type(sock_t* sock, cli_infp_t* infp)
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

	cli_send_get_nat_type(&gl_cli_infp, sock);
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
			j_value = cJSON_GetObjectItem(root, "cmd");
			if(j_value && j_value->valuestring)
			{
				if(!strcmp(j_value->valuestring, "login_ack"))
					ret = cli_infp_do_login_ack(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "heart_ack"))
					ret = infp_do_heart_beat(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "nat_type_ack"))
					ret = infp_do_get_nat_type(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "proxy_ack"))
					ret = infp_do_proxy_request(root, addr, sock);
				else if(!strcmp(j_value->valuestring, "proxy_task"))
					ret = infp_do_get_nat_port(root, addr, sock);
				else
				{
					CYM_LOG(LV_WARNING,"unknown cmd [%s]\n", j_value->valuestring);
				}
			}

			cJSON_Delete(root);
		}
	}

	memset(sock->recv_buf, 0, sock->recv_buf_len);
	return ret;
}

