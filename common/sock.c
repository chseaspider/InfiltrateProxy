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
#include <string.h>

#include "sock.h"
#include "mem.h"
#include "debug.h"

#define SOCK_BUF_LEN 102400
#define GET_GATEWAY_CMD "route | grep 'default' | awk '{print $8}'"
#define SOCK_HASH_MAX 0x100
#define SOCK_HASH_MASK 0xff

struct hlist_head sock_hash[SOCK_HASH_MAX];
int sock_hash_init = 0;

// TODO:need support thread safe
void sock_add_hash(sock_t* sock)
{
	if(!sock_hash_init)
	{
		int i = 0;
		for(i = 0; i < SOCK_HASH_MAX; i++)
			INIT_HLIST_HEAD(&sock_hash[i]);

		sock_hash_init = 1;
	}

	// 以防万一, 先删
	hlist_del_init(&sock->hash_to);
	hlist_add_head(&sock->hash_to, &sock_hash[BobHash(sock->fd) & SOCK_HASH_MASK]);
}

sock_t* sock_find_fd(int fd)
{
	sock_t* sock;
	struct hlist_head* head = &sock_hash[BobHash(fd) & SOCK_HASH_MASK];

	hlist_for_each_entry(sock, head, hash_to)
	{
		if(sock->fd == fd)
			return sock;
	}

	return NULL;
}

#if 0
int get_gateway_devname(char *gate)
{
	FILE *fp = NULL;
	char temp[20] = {0};
	int i = 0;

	fp = popen(GET_GATEWAY_CMD, "r");
	if (fp == NULL)
	{
		perror("popen:");
		return -1;
	}
	else
	{
		if (fread(temp, sizeof(char), sizeof(temp), fp) == -1)
		{
			perror("fread:");
		}
	}

	pclose(fp);

	while (temp[i] != '\n') {
		i++;
	}
	temp[i] = '\0';

	memcpy(gate, temp, strlen(temp));

	return 0;
}
#endif

__u32 get_default_local_ip(void)
{
#if 0
	static __u32 ip = 0;
	char dev[32] = {0};
	int inet_sock;
	struct ifreq ifr = {};

	if(ip)
		return ip;

	if(get_gateway_devname(dev))
		return 0;

	inet_sock = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, dev);
	ioctl(inet_sock, SIOCGIFADDR, &ifr);

	memcpy(&ip, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, sizeof(ip));

	close(inet_sock);
	return ip;
#else
	return 0;
#endif
}

int sock_add_poll(struct pollfd* _poll, int max, sock_t* sock)
{
	int curfd = 0;
	int i, found = 0;
	for(i = curfd; i < max; i++)
	{
		if(_poll[i].fd == INVALID_SOCKET)
		{
			_poll[i].fd = sock->fd;
			sock->poll_i = i;
			poll_add_read(&_poll[i]);
			found = 1;
			break;
		}
	}

	if(found)
	{
		for(i = max - 1; i >= 0; i--)
		{
			if(_poll[i].fd != INVALID_SOCKET)
			{
				curfd = i;
				break;
			}
		}
	}
	else
	{
		CYM_LOG(LV_FATAL, "poll is full\n");
	}

	return curfd+1;
}

int sock_del_poll(struct pollfd* _poll, int max, sock_t* sock)
{
	int curfd = 0;
	int i;

	if (sock->poll_i <= 0)
	{
		CYM_LOG(LV_FATAL, "fd [%d], poll_i [%d] invaild\n", sock->fd, sock->poll_i);
	}
	else
	{
		memset(&_poll[sock->poll_i], 0, sizeof(_poll[sock->poll_i]));
		_poll[sock->poll_i].fd = INVALID_SOCKET;
	}

	for(i = max - 1; i >= 0; i--)
	{
		if(_poll[i].fd != INVALID_SOCKET)
		{
			curfd = i;
			break;
		}
	}

	CYM_LOG(LV_FATAL, "poll fd:");
	for (i = 0; i < curfd+1; i++)
	{
		CYM_LOG(LV_FATAL, "[%d]:%d ", i, _poll[i].fd);
	}
	CYM_LOG(LV_FATAL, "\n");

	return curfd + 1;
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
		memcpy(sock->send_buf+sock->send_len, (char*)data + ret, len);
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
	if(ret > 0)
	{
		sock->recv_len += ret;
	}
	return ret;
}

sock_t *tcp_accept(sock_t *sock)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	sock_t *new_sock = NULL;
	int fd = accept(sock->fd,(struct sockaddr *)&addr, &addr_len);
	if(fd < 0)
	{
		perror("call to accept");
		return NULL;
	}
	
	new_sock = mem_malloc(sizeof(sock_t));
	if(!new_sock)
	{
		close(fd);
		perror("malloc error");
		return NULL;
	}

	new_sock->fd = fd;
	set_sock_timeout(fd, 300);
	sock_add_hash(new_sock);
	memcpy(&new_sock->addr, &addr, sizeof(new_sock->addr));

	return new_sock;
}

int create_udp(sock_t *sock, __u32 ip, __u16 port)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	if(!sock)
		return -1;

	close_sock(sock);

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
		close(sock->fd);
		sock->fd = INVALID_SOCKET;
		return -1;
	}

	CYM_LOG(LV_FATAL, "udp bind %s:%d, fd [%d] ok\n", IpToStr(ip), ntohs(port), sock->fd);

	sock_add_hash(sock);

	return sock->fd;
}

// timeout 单位 毫秒
int create_tcp(sock_t *sock, __u32 ip, __u16 port, int _listen)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	int opt = 1;
	int try_tm = 5;

	if(!sock)
		return -1;

	// 不管啥情况,先清理
	close_sock(sock);

	sock->fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock->fd < 0)
	{
		perror("socket:");
		return -1;
	}

	if(ip || port)
	{
		set_sockaddr_in(&addr, ip, port);
		memcpy(&sock->addr, &addr, sizeof(sock->addr));
	}
	else if(sock)
	{
		memcpy(&addr, &sock->addr, sizeof(addr));
	}
	else
	{
		printf("%s[%s]:%d ?\n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}

	//端口复用
	setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));
	set_sock_timeout(sock->fd, 500);	// 默认500毫秒延迟

	while(bind(sock->fd, (struct sockaddr *)&addr, addr_len) < 0)
	{
		if(try_tm-- < 0)
		{
			perror("bind:");
			close(sock->fd);
			sock->fd = INVALID_SOCKET;
			return -1;
		}
		printf("try bind tcp port %u\n", ntohs(addr.sin_port));
	}

	if(_listen)
	{
		if(listen(sock->fd, 0x100) < 0)
		{
			perror("call to listen");
			close(sock->fd);
			sock->fd = INVALID_SOCKET;
			return -1;
		}
	}

	CYM_LOG(LV_FATAL, "tcp bind %s:%d, fd [%d] ok\n", IpToStr(addr.sin_addr.s_addr), ntohs(addr.sin_port), sock->fd);

	sock_add_hash(sock);

	return sock->fd;
}

int tcp_just_connect(int fd, unsigned int addr, unsigned short port, int times)
{
	struct sockaddr_in sin;
	int ret = -1;
	int try_tm = times;
	bzero(&sin,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr=addr;
	sin.sin_port=port;

	printf("try connect tcp %s:%d\n", IpToStr(addr), ntohs(port));
	while(connect(fd,(void *)&sin,sizeof(sin)) < 0)
	{
		if(try_tm-- <= 0)
		{
			goto out;
		}
	}

	ret = 0;
out:
	return ret;
}

#define IP_HEADER_LEN sizeof(struct ip)
#define TCP_HEADER_LEN sizeof(struct tcphdr)

void initIPHeader(struct ip* header, int ttl, const char* src, const char* dst) {
	header->ip_v = IPVERSION;
	header->ip_hl = sizeof(struct ip) / 4;
	header->ip_tos = 0;
	header->ip_len = htons(IP_HEADER_LEN + TCP_HEADER_LEN);
	header->ip_id = 0;
	header->ip_off = 0;
	header->ip_ttl = MAXTTL; 
	header->ip_p = IPPROTO_TCP;
	header->ip_sum = 0;
	inet_pton(AF_INET, src, &header->ip_src.s_addr);
	inet_pton(AF_INET, dst, &header->ip_dst.s_addr);
}
 
void initTCPHeader(struct tcphdr* header, unsigned short sport, unsigned short dport) {
	header->source = htons(sport);
	header->dest = htons(dport);
 
	header->doff = sizeof(struct tcphdr) / 4;
	header->syn = 1;
	header->window = htons(4096);
	header->check = 0;
	header->seq = 0;
	header->ack_seq = 0;
}
 
struct psdHeader {
	unsigned int srcIP;
	unsigned int destIP;
	unsigned short zero:8;
	unsigned short proto:8;
	unsigned short totalLen;
};
 
void initPsdHeader(struct psdHeader* header, struct ip* iHeader) {
	header->srcIP = iHeader->ip_src.s_addr;
	header->destIP = iHeader->ip_dst.s_addr;
 
	header->zero = 0;
	header->proto = IPPROTO_TCP;
	header->totalLen = htons(0x0014);
}
 
unsigned short calcTCPCheckSum(const char* buf) {
	size_t size = TCP_HEADER_LEN + sizeof(struct psdHeader);
	unsigned int checkSum = 0;
	int i;
	for (i = 0; i < size; i += 2) {
		unsigned short first = (unsigned short)buf[i] << 8;
		unsigned short second = (unsigned short)buf[i+1] & 0x00ff;
		checkSum += first + second;
	}
	while (1) {
		unsigned short c = (checkSum >> 16);
		if (c > 0) {
			checkSum = (checkSum << 16) >> 16;
			checkSum += c;
		} else {
			break;
		}
	}
	return ~checkSum;
}

int infp_try_connect(const char* src, const char* dst, unsigned short sport, unsigned short dport, int ttl)
{
	int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock < 0) {
		perror("Socket Error");
		return -1;
	}
	const int on = 1;
	setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

	struct tcphdr* tHeader = (struct tcphdr*) malloc(sizeof(struct tcphdr));
	memset(tHeader, 0, TCP_HEADER_LEN);
	initTCPHeader(tHeader, sport, dport);
	struct ip* iHeader = (struct ip*) malloc(sizeof(struct ip));
	memset(iHeader, 0, IP_HEADER_LEN);
	initIPHeader(iHeader, ttl, src, dst);
	struct psdHeader* pHeader = (struct psdHeader*) malloc(sizeof(struct psdHeader));
	initPsdHeader(pHeader, iHeader);
 
	char sumBuf[TCP_HEADER_LEN + sizeof(struct psdHeader)];
	memset(sumBuf, 0, TCP_HEADER_LEN + sizeof(struct psdHeader));
	memcpy(sumBuf, pHeader, sizeof(struct psdHeader));
	memcpy(sumBuf + sizeof(struct psdHeader), tHeader, TCP_HEADER_LEN);
 
	int ni = memcmp(sumBuf, pHeader, sizeof(struct psdHeader));
	if (ni != 0) {
		perror("Compare");
	}
	ni = memcmp(sumBuf + sizeof(struct psdHeader), tHeader, TCP_HEADER_LEN);
	if (ni != 0) {
		perror("Compare 2");
	}
 
	tHeader->check = htons(calcTCPCheckSum(sumBuf));
 
	int totalLen = IP_HEADER_LEN + TCP_HEADER_LEN;
	char buf[totalLen];
 
	memcpy(buf, iHeader, IP_HEADER_LEN);
	memcpy(buf + IP_HEADER_LEN, tHeader, TCP_HEADER_LEN);
 
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, dst, &addr.sin_addr.s_addr);
	addr.sin_port = htons(dport);
 
	socklen_t len = sizeof(struct sockaddr_in);
	int n = sendto(sock, buf, totalLen, 0, (struct sockaddr*)&addr, len);
	if (n < 0) {
		perror("Send Error");
	}

	close(sock);

	return n;
}


void close_sock(sock_t *sock)
{
	if (sock->fd > 0)
	{
		close(sock->fd);
		CYM_LOG(LV_FATAL, "close fd = %d\n", sock->fd);
	}

	if(sock->recv_buf)
	{
		mem_free(sock->recv_buf);
		sock->recv_buf = NULL;
	}

	if(sock->send_buf)
	{
		mem_free(sock->send_buf);
		sock->send_buf = NULL;
	}

	hlist_del_init(&sock->hash_to);

	//memset(sock, 0, sizeof(sock_t));
	sock->fd = INVALID_SOCKET;
}

void free_sock(sock_t *sock)
{
	close_sock(sock);
	mem_free(sock);
}

