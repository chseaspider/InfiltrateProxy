#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdio.h>
#include "list.h"

struct timer_list {
	struct list_head	list;
	unsigned long		expires;
	void			(*function)(unsigned long);
	unsigned long		data;
};

unsigned long jiffies_get(void);
unsigned long init_jiffies(void);
void init_timervecs(void);

#define jiffies jiffies_get()

static inline void init_timer(struct timer_list * timer)
{
	timer->list.next = timer->list.prev = NULL;
	timer->data = 0;
	timer->function = NULL;
	timer->expires = 0;
}

#ifdef HZ
#undef HZ
#endif

#define HZ 1000

#endif
