/***************************************************************************
 *   Copyright (C) 2009 - 2010 by Simon Qian <SimonQian@SimonQian.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "vsf.h"

static struct vsfsm_state_t *
vsftimer_init_handler(struct vsfsm_t *sm, vsfsm_evt_t evt);

struct vsftimer_info_t
{
	struct vsfsm_t sm;
	struct vsfq_t timerlist;

	struct vsftimer_mem_op_t *mem_op;
} static vsftimer =
{
	.sm.init_state.evt_handler = vsftimer_init_handler,
};

// vsftimer_callback_int is called in interrupt,
// simply send event to vsftimer SM
void vsftimer_callback_int(void)
{
	vsfsm_post_evt_pending(&vsftimer.sm, VSFSM_EVT_TIMER);
}

vsf_err_t vsftimer_init(struct vsftimer_mem_op_t *mem_op)
{
	vsftimer.mem_op = mem_op;

	vsfq_init(&vsftimer.timerlist);
	return vsfsm_init(&vsftimer.sm);
}

static struct vsftimer_t *vsftimer_allocate(void)
{
	return vsftimer.mem_op->alloc();
}

void vsftimer_enqueue(struct vsftimer_t *timer)
{
	timer->node.addr = timer->interval + vsfhal_tickclk_get_ms();
	vsftimer_dequeue(timer);

	uint8_t origlevel = vsfsm_sched_lock();
	vsfq_enqueue(&vsftimer.timerlist, &timer->node);
	vsfsm_sched_unlock(origlevel);
}

void vsftimer_dequeue(struct vsftimer_t *timer)
{
	uint8_t origlevel = vsfsm_sched_lock();
	vsfq_remove(&vsftimer.timerlist, &timer->node);
	vsfsm_sched_unlock(origlevel);
}

static struct vsfsm_state_t *
vsftimer_init_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	uint32_t cur_tick = vsfhal_tickclk_get_ms();
	struct vsfsm_notifier_t notifier;
	struct vsftimer_t *timer;

	switch (evt)
	{
	case VSFSM_EVT_TIMER:
		timer = (struct vsftimer_t *)vsftimer.timerlist.head;
		while (timer != NULL)
		{
			if (cur_tick >= timer->node.addr)
			{
				if (timer->trigger_cnt > 0)
				{
					timer->trigger_cnt--;
				}
				notifier = timer->notifier;
				if (timer->trigger_cnt != 0)
				{
					vsftimer_enqueue(timer);
				}
				else
				{
					vsftimer_free(timer);
				}
				vsfsm_notifier_notify(&notifier);

				timer = (struct vsftimer_t *)vsftimer.timerlist.head;
			}
			else
			{
				break;
			}
		}
		break;
	}
	return NULL;
}

struct vsftimer_t *vsftimer_create_cb(uint32_t interval, int16_t trigger_cnt,
									void (*cb)(void *), void *param)
{
	uint8_t origlevel = vsfsm_sched_lock();
	struct vsftimer_t *timer = vsftimer_allocate();
	vsfsm_sched_unlock(origlevel);

	if (NULL == timer)
	{
		return NULL;
	}

	vsfsm_notifier_set_cb(&timer->notifier, cb, param);
	timer->interval = interval;
	timer->trigger_cnt = trigger_cnt;
	vsftimer_enqueue(timer);
	return timer;
}

struct vsftimer_t *vsftimer_create(struct vsfsm_t *sm, uint32_t interval,
									int16_t trigger_cnt, vsfsm_evt_t evt)
{
	uint8_t origlevel = vsfsm_sched_lock();
	struct vsftimer_t *timer = vsftimer_allocate();
	vsfsm_sched_unlock(origlevel);

	if (NULL == timer)
	{
		return NULL;
	}

	vsfsm_notifier_set_evt(&timer->notifier, sm, evt);
	timer->interval = interval;
	timer->trigger_cnt = trigger_cnt;
	vsftimer_enqueue(timer);
	return timer;
}

void vsftimer_free(struct vsftimer_t *timer)
{
	vsftimer_dequeue(timer);

	uint8_t origlevel = vsfsm_sched_lock();
	vsftimer.mem_op->free(timer);
	vsfsm_sched_unlock(origlevel);
}

extern void vsfsm_notifier_notify_evt(struct vsfsm_notifier_t *notifier);
void vsftimer_clean_sm(struct vsfsm_t *sm)
{
	uint8_t origlevel = vsfsm_sched_lock();
	struct vsftimer_t *timer = (struct vsftimer_t *)vsftimer.timerlist.head;
	struct vsftimer_t *timer_next;
	
	while (timer != NULL)
	{
		timer_next = container_of(timer->node.next, struct vsftimer_t, node);
		if (sm && (timer->notifier.notify == vsfsm_notifier_notify_evt) &&
			(timer->notifier.evt.sm == sm))
		{
			vsftimer_free(timer);
		}
		timer = timer_next;
	}
	vsfsm_sched_unlock(origlevel);
}

