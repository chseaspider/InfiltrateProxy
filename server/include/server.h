#ifndef __SERVER_H__
#define __SERVER_H__

#include "list.h"
#include "sock.h"

#define INFP_HASH_MAX 0x100
#define INFP_HASH_MASK 0xff

/* ���һЩȫ�ֱ��� */
typedef struct infp_s
{
	__u16 port;				// ���������˿�, ��������
	__u16 port_list[11];	// ������STUN���Զ˿�(�ݿ�11��)

	sock_t main_sock;		// ��Ӧport
	sock_t sock_list[11];	// ��Ӧport_list

	struct list_head dev_list;
	struct hlist_head dev_hash[INFP_HASH_MAX];
}infp_t;

typedef struct infp_cli_s
{
	__u32 ip;			// �ն˵�����IP��ַ
	char name[32];		// �ն˱�ʶ(����Ϊ��,IP��ַ��ͻʱ���һ��,��ֹ�������˼ҵ��豸ȥ)

	__u8 nat_type;		// nat���� @see C_NAT_TYPE
	__u8 mode;			// 0: �ͻ��� 1: PC��
	__u16 guess_port;	// �²�Ŀ��ö˿�(���߶��ǶԳ�NAT�������δ����)

	__u32 dst_ip;		// ���ʵ�Ŀ��IP
	char dst_name[32];	// ���ʵ�Ŀ���ʶ(����Ϊ��,IP��ַ��ͻʱ���һ��,��ֹ�������˼ҵ��豸ȥ)

	struct list_head list_to;	// ����infp_t.dev_list
	struct hlist_node hash_to;	// ����infp_t.dev_hash ip+name��Ϊhashֵ
}infp_cli_t;

#endif // __SERVER_H__
