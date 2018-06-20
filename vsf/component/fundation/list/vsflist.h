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

#ifndef __VSFLIST_H_INCLUDED__
#define __VSFLIST_H_INCLUDED__

struct vsflist_t
{
	struct vsflist_t *next;
};

#define vsflist_init_node(node)			((node).next = NULL)
#define vsflist_insert(node, new)		((node).next = &(new))
#define vsflist_get_container(p, t, m)	container_of(p, t, m)

#define vsflist_foreach(var, p, t, m) \
	for (t *var = vsflist_get_container(p, t, m); var != NULL; \
			var = vsflist_get_container(var->m.next, t, m))

#define vsflist_foreach_next(var, n, p, t, m) \
	for (t *var = vsflist_get_container(p, t, m), \
			*n = var ? vsflist_get_container(var->m.next, t, m) : NULL; var != NULL; \
			var = n, n = var ? vsflist_get_container(var->m.next, t, m) : NULL)

int vsflist_get_length(struct vsflist_t *head);
struct vsflist_t * vsflist_get_node(struct vsflist_t *head, int idx);
int vsflist_get_idx(struct vsflist_t *head, struct vsflist_t *node);
int vsflist_is_in(struct vsflist_t *head, struct vsflist_t *node);
int vsflist_remove(struct vsflist_t **head, struct vsflist_t *node);
void vsflist_append(struct vsflist_t *head, struct vsflist_t *new_node);
void vsflist_delete_next(struct vsflist_t *head);

#endif // __VSFLIST_H_INCLUDED__

