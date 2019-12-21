#ifndef __MEM_H__
#define __MEM_H__

#include "stdlib.h"

inline void* mem_malloc(__u32 size)
{
	void* temp = malloc(size);
	if(temp)
	{
		memset(temp, 0, size);
	}

	return temp;
}

inline void mem_free(void* ptr)
{
	if(ptr)
		free(ptr);
}

#endif
