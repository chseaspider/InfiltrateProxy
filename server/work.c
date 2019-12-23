
static inline struct hlist_head* infp_get_hash_head(char *str)
{
	return &gl_infp.dev_hash[(SDBMHash(str) & INFP_HASH_MASK)];
}

infp_cli_t *infp_create_cli(char *str)
{
	infp_cli_t *temp = mem_malloc(sizeof(infp_cli_t));
	if(temp)
	{
		list_add_tail(&temp->list_to, &gl_infp.dev_list);
		hlist_add_head(&temp->hash_to, infp_get_hash_head(str));

		snprintf(temp->des, sizeof(temp->des), "%s", str);
	}

	return temp;
}

infp_cli_t *infp_find_cli(char *str)
{
	char des[64];
	struct hlist_node *pos;
	infp_cli_t *temp;

	hlist_for_each(pos, infp_get_hash_head(str))
	{
		temp = hlist_entry(pos, infp_cli_t, hash_to);
		if(!strcmp(temp->des, str))
		{
			return temp;
		}
	}

	return NULL;
}

infp_cli_t *infp_find_create_cli(char *str)
{
	infp_cli_t *temp = infp_find_cli(str);
	if(temp)
		return temp;

	return infp_create_cli(str);
}

int cli_parse_mode(char *mode)
{
	if(!strcmp(mode, "host"))
		return 1;

	return 0;
}

void init_cli_info(infp_cli_t *cli, char *ip, int port
					, char *mode, char *name, struct sockaddr_in *addr)
{
	snprintf(cli->ip, sizeof(cli->ip), "%s", ip);
	cli->main_port.src_port = port;
	cli->main_port.nat_port = ntohs(addr.sin_port);
	cli->mode = cli_parse_mode(mode);
	snprintf(cli->name, sizeof(cli->name), "%s", name);
	cli->nat_ip = addr.sin_addr.s_addr;
}

int infp_do_login(cJSON* root, struct sockaddr_in *addr)
{
	int ret = -1;
	char ip[32];
	int port;
	char mode[16];
	char name[32];
	char des[64];	// ip + name
	cJSON* j_value;
	infp_cli_t *cli;

	j_value = cJSON_GetObjectItem(root, "ip");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse ip failed\n");
		goto ret;
	}
	snprintf(ip, sizeof(ip), "%s", j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "port");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse port failed\n");
		goto ret;
	}
	port = atoi(j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "mode");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse mode failed\n");
		goto ret;
	}
	snprintf(mode, sizeof(mode), "%s", j_value->valuestring);

	j_value = cJSON_GetObjectItem(root, "name");
	if(!j_value || !j_value->valuestring)
	{
		CYM_LOG(LV_ERROR, "parse name failed\n");
		goto ret;
	}
	snprintf(name, sizeof(name), "%s", j_value->valuestring);

	snprintf(des, sizeof(des), "%s%s", ip, name);
	cli = infp_find_create_cli(des);
	if(!cli)
	{
		CYM_LOG(LV_FATAL, "find create cli failed\n");
		goto out;
	}

	init_cli_info(cli, ip, port, mode, name, addr);

	ret = 0;
out:
	return ret;
}

int infp_recv_do(sock_t *sock, struct sockaddr_in *addr)
{
	int ret = -1;

	if(sock->recv_buf && sock->recv_len)
	{
		cJSON* root = cJSON_Parse(sock->recv_buf);
		if(root)
		{
			cJSON* j_value;
			j_value = cJSON_GetObjectItem(root, "cmd");
			if(j_value && j_value->valuestring)
			{
				if(!strcmp(j_value->valuestring, "login"))
					ret = infp_do_login(root, addr);
				else if(!strcmp(j_value->valuestring, "heart_beat"))
					ret = infp_do_heart_beat(root, addr);;
				else if(!strcmp(j_value->valuestring, "get_nat_type"))
					ret = infp_do_get_nat_type(root, addr);;
				else if(!strcmp(j_value->valuestring, "proxy_request"))
					ret = infp_do_proxy_request(root, addr);;
				else if(!strcmp(j_value->valuestring, "get_nat_port"))
					ret = infp_do_get_nat_port(root, addr);;
				else if(!strcmp(j_value->valuestring, "proxy_task_ack"))
					ret = infp_do_proxy_task_ack(root, addr);;
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

