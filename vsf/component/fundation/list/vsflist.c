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

#include "vsf_type.h"
#include "vsflist.h"

int vsflist_get_length(struct vsflist_t *head)
{
	int length = 0;
	while (head != NULL)
	{
		length++;
		head = head->next;
	}
	return length;
}

struct vsflist_t * vsflist_get_node(struct vsflist_t *head, int idx)
{
	struct vsflist_t *node = head;
	int pos = 0;
	while (node != NULL)
	{
		if (pos++ == idx)
			return node;
		node = node->next;
	}
	return node;
}

int vsflist_get_idx(struct vsflist_t *head, struct vsflist_t *node)
{
	int index = 0;
	while (head != NULL)
	{
		if (head == node)
		{
			return index;
		}
		head = head->next;
		index++;
	}
	return -1;
}

int vsflist_is_in(struct vsflist_t *head, struct vsflist_t *node)
{
	return vsflist_get_idx(head, node) >= 0;
}

int vsflist_remove(struct vsflist_t **head, struct vsflist_t *node)
{
	if (!vsflist_is_in(*head, node))
	{
		return -1;
	}

	if (*head == node)
	{
		*head = node->next;
		return 0;
	}
	while (*head != NULL)
	{
		if ((*head)->next == node)
		{
			(*head)->next = node->next;
			break;
		}
		*head = (*head)->next;
	}
	return 0;
}

void vsflist_append(struct vsflist_t *head, struct vsflist_t *new_node)
{
	struct vsflist_t *next;

	next = head;
	while (next->next != NULL)
		next = next->next;

	next->next = new_node;
	new_node->next = NULL;
}

void vsflist_delete_next(struct vsflist_t *head)
{
	struct vsflist_t *next;

	next = head->next;
	if (next->next)
		head->next = next->next;
	else
		head->next = NULL;
}
