#ifndef __INFILTRATE_H__
#define __INFILTRATE_H__

#define DEFULAT_MAGIC 0x74a9c6f8

typedef struct infp_head_s
{
	__u32 magic;	// ͷ��ʶ(�����ļ���ȡ)

	__u32 data_len;	// �������ݳ���
	__u8 data[0];	// (����)��������->��json
}infp_head_t;

#endif
