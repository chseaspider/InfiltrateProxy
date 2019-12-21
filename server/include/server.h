#ifndef __SERVER_H__
#define __SERVER_H__

#include "list.h"
#include "sock.h"

#define INFP_HASH_MAX 0x100
#define INFP_HASH_MASK 0xff

/* 存放一些全局变量 */
typedef struct infp_s
{
	__u16 port;				// 监听的主端口, 主连接用
	__u16 port_list[11];	// 监听的STUN测试端口(暂开11个)

	sock_t main_sock;		// 对应port
	sock_t sock_list[11];	// 对应port_list

	struct list_head dev_list;
	struct hlist_head dev_hash[INFP_HASH_MAX];
}infp_t;

typedef struct infp_cli_s
{
	__u32 ip;			// 终端的内网IP地址
	char name[32];		// 终端标识(可以为空,IP地址冲突时需搞一个,防止连到别人家的设备去)

	__u8 nat_type;		// nat类型 @see C_NAT_TYPE
	__u8 mode;			// 0: 客户端 1: PC端
	__u16 guess_port;	// 猜测的可用端口(两边都是对称NAT的情况暂未处理)

	__u32 dst_ip;		// 访问的目标IP
	char dst_name[32];	// 访问的目标标识(可以为空,IP地址冲突时需搞一个,防止连到别人家的设备去)

	struct list_head list_to;	// 关联infp_t.dev_list
	struct hlist_node hash_to;	// 关联infp_t.dev_hash ip+name作为hash值
}infp_cli_t;

#endif // __SERVER_H__
