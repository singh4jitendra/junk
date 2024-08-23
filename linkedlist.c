#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "linkedlist.h"

void ll_debug(char * fmt, ...);

linkedlist_t * ll_create(ll_compare_func_t compare)
{
	linkedlist_t * list;

	list = (linkedlist_t *)malloc(sizeof(linkedlist_t));
	if (list != NULL) {
		list->first = list->current = list->last = NULL;
		list->length = 0l;
		list->flags = LL_FREELISTMEM|LL_FREELISTNODES;
		list->compare = compare;
		ll_debug("ll_create: malloc(%d) = 0x%08x\n", sizeof(linkedlist_t), list);
	}
	else {
		ll_debug("ll_create: %s\n", strerror(errno));
	}

	return list;
}

void ll_set_free_data(linkedlist_t * list, int free_data)
{
	ll_debug("ll_set_free_data: set free_data = %d\n", free_data);

	if (free_data)
		list->flags |= LL_FREELISTDATA;
}

void ll_destroy(linkedlist_t * list)
{
	ll_debug("ll_destroy: list = 0x%08x\n", list);

	if (list->flags & LL_FREELISTNODES) {
		list->current = list->first;
		while (list->current != NULL) {
			list->first = list->first->next;
			if (list->flags & LL_FREELISTDATA) {
				if (list->current->data != NULL) {
					ll_debug("ll_destroy: destroy data 0x%08x\n",
						list->current->data);
					free(list->current->data);
				}
			}

			ll_debug("ll_destroy: destroy node 0x%08x\n", list->current);
			free(list->current);
			list->current = list->first;
		}
	}

	if (list->flags & LL_FREELISTMEM) {
		ll_debug("ll_destroy: destroy list 0x%08x\n", list);
		free(list);
	}

	return;
}

int ll_prepend(linkedlist_t * list, void * data)
{
	int index;

	ll_debug("ll_prepend: list = 0x%08x, data = 0x%08x\n", list, data);

	index = (list->length == 0) ? -1 : 0;
	return ll_insertat(list, index, data);
}

int ll_append(linkedlist_t * list, void * data)
{
	ll_debug("ll_append: list = 0x%08x, data = 0x%08x\n", list, data);

	return ll_insertat(list, -1, data);
}

int ll_insertat(linkedlist_t * list, int index, void * data)
{
	int count;
	listnode_t * node;

	ll_debug("ll_insertat: list = 0x%08x, index = %d, data = 0x%08x\n",
		list, index, data);

	if ((index < -1) || (index >= list->length)) {
		ll_debug("ll_insertat: invalid index out of bounds %d\n", index);
		return -1;
	}

	if (list->flags & LL_READONLY) {
		ll_debug("ll_insertat: ERROR: list at 0x%08x is read-only!\n", list);
		return -1;
	}

	node = (listnode_t *)malloc(sizeof(listnode_t));
	ll_debug("ll_insertat: malloc(%d) = 0x%08x\n", sizeof(listnode_t), node);
	if (node == NULL) {
		ll_debug("             %s\n", strerror(errno));
		return -1;
	}

	node->data = data;
	node->next = node->prev = NULL;

	if (list->first == NULL) {
		ll_debug("ll_insertat: null list, first element\n");
		list->first = list->current = list->last = node;
	}
	else if (index == 0) {
		ll_debug("ll_insertat: first element\n");
		node->next = list->first;
		list->first->prev = node;
		list->first = list->current = node;
	}
	else if (index == -1) {
		ll_debug("ll_insertat: last element\n");
		node->prev = list->last;
		list->last->next = node;
		list->last = list->current = node;
	}
	else {
		for (count = 0, list->current = list->first;
			count < index && list->current != NULL;
			count++) list->current = list->current->next;

		ll_debug("ll_insertat: insert at %d\n", index);

		node->next = list->current;
		node->prev = list->current->prev;

		if (list->current == list->first) {
			list->first = node;
		}
		else {
			list->current->prev->next = node;
		}

		list->current->prev = node;
		list->current = node;
	}

	list->length++;

	return index;
}

void ll_delete_current(linkedlist_t * list)
{
	listnode_t * tmp;

	ll_debug("ll_delete_current: list = 0x%08x\n", list);

	if (list == NULL) return;

	if (list->current == NULL) return;

	if (list->current == list->first) {
		ll_debug("ll_delete_current: delete first element\n");
		list->first = list->first->next;
		if (list->first != NULL)
			list->first->prev = NULL;

		free(list->current);
		list->current = list->first;
	}
	else if (list->current->next == NULL) {
		ll_debug("ll_delete_current: delete last element\n");
		tmp = list->current->prev;
		tmp->next = NULL;
		free(list->current);
		list->current = tmp;
	}
	else {
		ll_debug("ll_delete_current: delete interior element\n");
		tmp = list->current->prev;
		tmp->next = list->current->next;
		tmp->next->prev = tmp;
		free(list->current);
		list->current = tmp;
	}
}

int ll_compare_long(listnode_t * node, void * value)
{
	return ((int32_t)(node->data) == (int32_t)value);
}

int ll_compare_short(listnode_t * node, void * value)
{
	return ((short)(node->data) == (short)value);
}

int ll_compare_str(listnode_t * node, void * value)
{
	return strcmp((char *)(node->data), (char *)value);
}

void * ll_move_first(linkedlist_t * list)
{
	ll_debug("ll_move_first: list = 0x%08x\n", list);

	list->current = list->first;
	return (list->current == NULL) ? NULL : list->current->data;
}

void * ll_move_next(linkedlist_t * list)
{
	ll_debug("ll_move_next: list = 0x%08x\n", list);

	if (list->current == NULL) {
		ll_debug("ll_move_next: list->current = 0x%08x\n", list->current);
		return NULL;
	}
	else {
		list->current = list->current->next;
		ll_debug("ll_move_next: list->current = 0x%08x\n", list->current);
		return (list->current == NULL) ? NULL : list->current->data;
	}
}

void * ll_move_prev(linkedlist_t * list)
{
	ll_debug("ll_move_prev: list = 0x%08x\n", list);

	if (list->current == NULL) {
		ll_debug("ll_move_prev: list->current = 0x%08x\n", list->current);
		return NULL;
	}
	else {
		list->current = list->current->prev;
		ll_debug("ll_move_prev: list->current = 0x%08x\n", list->current);
		return (list->current == NULL) ? NULL : list->current->data;
	}
}

void * ll_move_last(linkedlist_t * list)
{
	ll_debug("ll_move_first: list = 0x%08x\n", list);

	list->current = list->last;
	return (list->current == NULL) ? NULL : list->current->data;
}

linkedlist_t * ll_create_iterator(linkedlist_t * list)
{
	linkedlist_t * iterator;

	iterator = (linkedlist_t *)malloc(sizeof(linkedlist_t));
	if (iterator != NULL) {
		ll_debug("ll_create_iterator: malloc(%d) = 0x%08x\n",
			sizeof(linkedlist_t), iterator);
		memcpy(iterator, list, sizeof(linkedlist_t));
		iterator->flags = LL_READONLY|LL_FREELISTMEM;
	}
	else {
		ll_debug("ll_create_iterator: malloc(%d) = 0x00000000\n",
			sizeof(linkedlist_t));
	}

	return iterator;
}

void ll_destroy_iterator(linkedlist_t * iterator)
{
	ll_destroy(iterator);
}

void ll_debug(char * fmt, ...)
{
	FILE * fp;
	va_list ap;
	char * p = getenv("LL_DEBUG");

	va_start(ap, fmt);

	if (p != NULL) {
		if (toupper(*p) == 'Y') {
			p = getenv("LL_DEBUG_LOG");
			if (p == NULL)
				p = "ll.dbg";

			fp = fopen(p, "a+");
			if (fp) {
				vfprintf(fp, fmt, ap);
				fclose(fp);
			}
		}
	}

	va_end(ap);
}
