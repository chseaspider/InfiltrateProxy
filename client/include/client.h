#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "list.h"
#include "timer.h"
#include "sock.h"

#define INFP_DEFAFULT_PORT 45124 // TODO: 配置文件获取

#define INFP_HASH_MAX 0x100
#define INFP_HASH_MASK 0xff

enum CLI_INFP_STATE
{
	CLI_INFP_INIT = 0,		// 初始化
	CLI_INFP_LOGIN,			// 已登陆
	CLI_INFP_CONNECT,		// 已打通NAT
};

typedef struct inf_proxy_s
{
	__u32 ip;			// IP
	__u8  name[32];		// 标识
	__u32 uptime;		// 更新时间 jiffies
}inf_proxy_t;

/* 存放一些全局变量 */
typedef struct cli_infp_s
{
	__u32 server_ip;		// 服务器ip (网络序)
	__u16 svr_m_port;		// 服务器主端口 (网络序)
	__u16 svr_b_port;		// 服务器副端口 (网络序)

	__u16 main_port;		// 与服务器连接的主端口, 主连接用
	__u16 back_port[2];		// 用于检测NAT类型用的端口, 临时开
	__u16 proxy_port;		// 准备的代理端口

	sock_t main_sock;		// 对应main_port
	sock_t back_sock[2];	// 对应back_port
	sock_t proxy_sock;		// 对应proxy_port (代理主连接)

	__u32 nat_type;			// @see C_NAT_TYPE
	__u8  name[32];			// 客户端标识
	__u8  mode[8];			// 客户端模式 host/client

	__u32 state;			// @see CLI_INFP_STATE
	__u32 next_login;		// 下次登陆
	__u32 next_hb;			// 下次心跳

	inf_proxy_t dst;		// 要连接的目标设备信息

	struct timer_list timer;	// 1秒1次的timer (所有timer可以统一扔这里)
}cli_infp_t;

extern cli_infp_t gl_infp;

#endif // __SERVER_H__
