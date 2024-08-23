#if !defined(_LINKEDLIST_H)
#define _LINKEDLIST_H

#define LL_FREELISTMEM   0x01
#define LL_FREELISTNODES 0x02
#define LL_READONLY      0x04
#define LL_FREELISTDATA  0X08

struct listnode {
	void * data;
	struct listnode * next;
	struct listnode * prev;
};

typedef int (* ll_compare_func_t)(struct listnode *, void *);

struct linkedlist {
	struct listnode * first;
	struct listnode * current;
	struct listnode * last;
	ll_compare_func_t compare;
	int32_t length;
	int32_t flags;
};

typedef struct linkedlist linkedlist_t;
typedef struct listnode listnode_t;
typedef struct listiterator ll_iterator_t;

linkedlist_t * ll_create(ll_compare_func_t);
void ll_set_free_data(linkedlist_t *, int);
void ll_destroy(linkedlist_t *);
int ll_prepend(linkedlist_t *, void *);
int ll_append(linkedlist_t *, void *);
int ll_insertat(linkedlist_t *, int, void *);
void ll_delete_current(linkedlist_t *);

int ll_compare_long(listnode_t *, void *);
int ll_compare_short(listnode_t *, void *);
int ll_compare_str(listnode_t *, void *);

void * ll_move_first(linkedlist_t *);
void * ll_move_next(linkedlist_t *);
void * ll_move_prev(linkedlist_t *);
void * ll_move_last(linkedlist_t *);

linkedlist_t * ll_create_iterator(linkedlist_t *);
void ll_destroy_iterator(linkedlist_t *);

#endif
