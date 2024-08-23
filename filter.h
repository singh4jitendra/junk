#ifndef _FILTER_H
#define _FILTER_H

char * filter_type[] = {
#define FLT_EQ     0
	"==",
#define FLT_GE     1
	">=",
#define FLT_GT     2
	">",
#define FLT_LE     3
	"<=",
#define FLT_LT     4
	"<",
#define FLT_NE     5
	"!=",
#define FLT_START  6
	"BEGINS"
};

typedef struct _FILTER_T {
	unsigned long flt_column;
	size_t flt_size;
	short flt_type;
	void * flt_value;
} filter_t;

#endif
