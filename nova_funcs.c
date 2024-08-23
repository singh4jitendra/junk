#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>
#include <time.h>
#include <ctype.h>
#include "filter.h"
#include "linkedlist.h"
#include "sub.h"
#include "common.h"

int filter_debug(void);
int select_debug(void);
void hex_dump(FILE * fp, char * buffer, size_t bufsize);

int c_memcmp(char * name, int num_args, Argument args[], int initial)
{
	return Okay;
}

int nova_range(char * name, int num_args, Argument args[], int initial)
{
	return Okay;
}

int nova_expr(char * name, int num_args, Argument args[], int initial)
{
	char *b;
	char *p;
	regex_t re;

	if (num_args != 3 || Numeric(args[1].a_type)) return Halt;

	p = (char *)calloc(1, a_size(args[1])+5);
	if (p == NULL) return Halt;

	b = (char *)calloc(1, a_size(args[0])+1);
	if (b == NULL) {
		free(p);
		return Halt;
	}

	/* create the regular expression */
	sprintf(p, "^(");
	memcpy(p+2, args[1].a_address, a_size(args[1]));
	strcat(p, ")$");

	/* create a "C"-style string */
	memcpy(b, args[0].a_address, a_size(args[0]));

	regcomp(&re, p, REG_EXTENDED|REG_NOSUB);
	*((int *)(args[2].a_address)) = regexec(&re, b, (size_t)0, NULL, 0);

	free(b);
	free(p);

	return Okay;
}

int nova_find(char * name, int num_args, Argument args[], int initial)
{
	int i = 0;
	int index;
	char * str;
	char * pat;
	regex_t re;
	regmatch_t rm;

	if ((num_args != 3) || (Numeric(args[0].a_type) || Numeric(args[1].a_type) || Numeric(args[2].a_type))) {
		return Halt;
	}

	str = calloc(a_size(args[0])+1, sizeof(char));
	if (str == NULL) return Halt;

	pat = calloc(a_size(args[1])+1, sizeof(char));
	if (pat == NULL) return Halt;

	memcpy(str, args[0].a_address, a_size(args[0]));
	CobolToC(pat, args[1].a_address, (a_size(args[1])+1) * sizeof(char),
		a_size(args[1]));

	regcomp(&re, pat, REG_EXTENDED);
	if (regexec(&re, str, (size_t)1, &rm, 0) == 0) {
		for (index = rm.rm_so; index < rm.rm_eo; index++) {
			if (i >= a_size(args[2])) break;
			((char *)(args[2].a_address))[i++] = str[index];
		}
	}
	else {
		memset(args[2].a_address, ' ', a_size(args[2]));
	}

	regfree(&re);
	free(pat);
	free(str);

	return Okay;
}

/*
 * nova_select
 *
 * This function determines if the current record fits the selection
 * criteria.  It returns a 0 if it does, 1 if it doesn't.
 *
 * arg[0] = Pointer to file filters.
 * arg[1] = Data record to test.
 * arg[2] = Result code.
 *
 */
int nova_select(char * name, int num_args, Argument args[], int initial)
{
	char * p;
	int result;
	filter_t * flt;
	linkedlist_t * ll;
	linkedlist_t * it;
	FILE * dbg;
	int flt_dbg = select_debug();

	if (num_args != 3 && !Numeric(args[2].a_type))
		return Halt;

	ll = (linkedlist_t *)(*((unsigned long *)args[0].a_address));

	if (flt_dbg != 0) {
		dbg = fopen("select.txt", "a+");
		if (dbg == NULL) {
			flt_dbg = 0;
		}
		else {
			fprintf(dbg, "Filter Address: 0x%08x\n", (long)ll);
			fwrite(args[1].a_address, a_size(args[1]), sizeof(char), dbg);
			fprintf(dbg, "\n");
		}
	}

	if (ll != NULL) {
		it = ll_create_iterator(ll);
		if (it == NULL) return Halt;

		flt = (filter_t *)ll_move_first(it);
		if (flt_dbg != 0) {
			fprintf(dbg, "N$SELECT\n");
		}

		while (flt != NULL) {
			*((unsigned short *)args[2].a_address) = 0;
			p = (char *)args[1].a_address + flt->flt_column;
			result = memcmp(p, flt->flt_value, flt->flt_size);

			if (flt_dbg != 0) {
				fprintf(dbg, "\t\"");
				hex_dump(dbg, p, flt->flt_size);
				fprintf(dbg, "\" %s \"", filter_type[flt->flt_type]);
				hex_dump(dbg, flt->flt_value, flt->flt_size);
				fprintf(dbg, "\"");
			}

			switch (flt->flt_type) {
				case FLT_EQ:
					if (result != 0) {
						*((unsigned short *)args[2].a_address) = 1;
						ll_destroy_iterator(it);
						if (flt_dbg != 0) {
							fprintf(dbg, "...FAILED!\n\n");
							fclose(dbg);
						}

						return Okay;
					}
					else if (flt_dbg != 0) {
						fprintf(dbg, "...OK!\n");
					}

					break;
				case FLT_GE:
					if (result < 0) {
						*((unsigned short *)args[2].a_address) = 1;
						ll_destroy_iterator(it);
						if (flt_dbg != 0) {
							fprintf(dbg, "...FAILED!\n\n");
							fclose(dbg);
						}

						return Okay;
					}
					else if (flt_dbg != 0) {
						fprintf(dbg, "...OK!\n");
					}

					break;
				case FLT_GT:
					if (result <= 0) {
						*((unsigned short *)args[2].a_address) = 1;
						ll_destroy_iterator(it);
						if (flt_dbg != 0) {
							fprintf(dbg, "...FAILED!\n\n");
							fclose(dbg);
						}

						return Okay;
					}
					else if (flt_dbg != 0) {
						fprintf(dbg, "...OK!\n");
					}

					break;
				case FLT_LE:
					if (result > 0) {
						*((unsigned short *)args[2].a_address) = 1;
						ll_destroy_iterator(it);
						if (flt_dbg != 0) {
							fprintf(dbg, "...FAILED!\n\n");
							fclose(dbg);
						}

						return Okay;
					}
					else if (flt_dbg != 0) {
						fprintf(dbg, "...OK!\n");
					}

					break;
				case FLT_LT:
					if (result >= 0) {
						*((unsigned short *)args[2].a_address) = 1;
						ll_destroy_iterator(it);
						if (flt_dbg != 0) {
							fprintf(dbg, "...FAILED!\n\n");
							fclose(dbg);
						}

						return Okay;
					}
					else if (flt_dbg != 0) {
						fprintf(dbg, "...OK!\n");
					}

					break;
				case FLT_NE:
					if (result == 0) {
						*((unsigned short *)args[2].a_address) = 1;
						ll_destroy_iterator(it);
						if (flt_dbg != 0) {
							fprintf(dbg, "...FAILED!\n\n");
							fclose(dbg);
						}

						return Okay;
					}
					else if (flt_dbg != 0) {
						fprintf(dbg, "...OK!\n");
					}

					break;
				case FLT_START:
					if (result != 0) {
						*((unsigned short *)args[2].a_address) = 1;
						ll_destroy_iterator(it);
						if (flt_dbg != 0) {
							fprintf(dbg, "...FAILED!\n\n");
							fclose(dbg);
						}

						return Okay;
					}
					else if (flt_dbg != 0) {
						fprintf(dbg, "...OK!\n");
					}

					break;
			}

			flt = (filter_t *)ll_move_next(it);
		}

		ll_destroy_iterator(it);
	}
	else {
		*((unsigned short *)args[2].a_address) = 0;
	}

	if (flt_dbg != 0) {
		fprintf(dbg, "\n");
		fclose(dbg);
	}

	return Okay;
}

/*
 * nova_addfilter
 *
 * This function adds criteria to the current filter.
 *
 * arg[0] = Column number.
 * arg[1] = Field size.
 * arg[2] = Filter type.
 * arg[3] = Filter value.
 * arg[4] = Pointer to file filters.
 *
 */
int nova_addfilter(char * name, int num_args, Argument args[], int initial)
{
	int i, j;
	char * v;
	filter_t * flt;
	linkedlist_t * ll;
	unsigned long l;
	FILE * dbg;
	int flt_dbg = filter_debug();

	if (num_args != 5 || !Numeric(args[0].a_type) || !Numeric(args[1].a_type) || !Numeric(args[2].a_type))
		return Halt;

	if (flt_dbg != 0) {
		dbg = fopen("filter.txt", "a+");
		if (dbg == NULL) {
			flt_dbg = 0;
		}
		else {
			fprintf(dbg, "Filter Address: 0x%08x\n",
				*((unsigned long *)args[4].a_address));
		}
	}

	ll = (linkedlist_t *)(*((unsigned long *)args[4].a_address));
	if (ll == NULL) {
		ll = ll_create(NULL);
		if (ll == NULL) return Halt;

		ll_set_free_data(ll, 1);

		l = (unsigned long)ll;
		*((unsigned long *)args[4].a_address) = l;

		if (flt_dbg != 0) {
			fprintf(dbg, "Created filter @ 0x%08x.\n", (long)ll);
		}
	}

	flt = malloc(sizeof(filter_t));
	if (flt == NULL) return Halt;

	/*
	   subtract 1 from supplied column number to account for C's 0-based
		indexes
	*/
	flt->flt_column = *((unsigned long *)args[0].a_address) - 1;
	flt->flt_size = *((size_t *)args[1].a_address);
	flt->flt_type = *((short *)args[2].a_address);

	/* allocate storage for the value */
	v = malloc((size_t)flt->flt_size);
	if (v == NULL) {
		free(flt);
		return Halt;
	}

	memcpy(v, args[3].a_address, flt->flt_size);
	flt->flt_value = v;

	if (flt_dbg != 0) {
		fprintf(dbg, "N$ADDFILTER:\n");
		fprintf(dbg, "\tColumn: %d\n", flt->flt_column);
		fprintf(dbg, "\tLength: %d\n", flt->flt_size);
		fprintf(dbg, "\tType:   %s\n", filter_type[flt->flt_type]);
		fprintf(dbg, "\tValue:  ");
		hex_dump(dbg, flt->flt_value, flt->flt_size);
/*		fwrite(flt->flt_value, flt->flt_size, 1, dbg); */
		fprintf(dbg, "\n");
	}

	ll_append(ll, flt);

	if (flt_dbg != 0) fclose(dbg);

	return Okay;
}

/*
 * nova_removefilter
 *
 * This function clears a specific criteria for a filter.
 *
 * arg[0] = Column number.
 * arg[1] = Pointer to file filters.
 *
 */
int nova_removefilter(char * name, int num_args, Argument args[], int initial)
{
	int col;
	filter_t * flt;
	linkedlist_t * ll;
	linkedlist_t * it;
	FILE * dbg;
	int flt_dbg = filter_debug();

	if (num_args != 2 || !Numeric(args[0].a_type))
		return Halt;

	col = *((unsigned long *)args[0].a_address) - 1;
	ll = (linkedlist_t *)(*((unsigned long *)args[1].a_address));

	if (flt_dbg != 0) {
		dbg = fopen("filter.txt", "a+");
		if (dbg == NULL) {
			flt_dbg = 0;
		}
		else {
			fprintf(dbg, "N$REMOVEFILTER:\n");
			fprintf(dbg, "\tAddress: 0x%08x\n", ll);
			fprintf(dbg, "\tColumn:  %d\n", col);
		}
	}

	if (ll != NULL) {
		it = ll_create_iterator(ll);
		if (it == NULL) return Halt;

		flt = (filter_t *)ll_move_first(it);
		while (flt != NULL) {
			if (flt->flt_column == col)
				if (flt_dbg != 0) {
					fprintf(dbg, "\tValue:   ");
					hex_dump(dbg, flt->flt_value, flt->flt_size);
/*					fwrite(flt->flt_value, flt->flt_size, 1, dbg); */
					fprintf(dbg, "\n");
				}

				free(flt->flt_value);
				ll_delete_current(it);

			flt = (filter_t *)ll_move_next(it);
		}
	}

	if (flt_dbg != 0) fclose(dbg);

	return 0;
}

/*
 * nova_srcclear
 *
 * This function clears all criteria for a filter.
 *
 * arg[0] = Pointer to file filters.
 *
 */
int nova_srcclear(char * name, int num_args, Argument args[], int initial)
{
	linkedlist_t * ll;
	FILE * dbg;
	int flt_dbg = filter_debug();

	if (num_args != 1) return Halt;

	ll = (linkedlist_t *)(*((unsigned long *)args[0].a_address));
	if (flt_dbg != 0) {
		dbg = fopen("filter.txt", "a+");
		if (dbg == NULL) {
			flt_dbg = 0;
		}
		else {
			fprintf(dbg, "N$SRCCLEAR:\n");
			fprintf(dbg, "\tAddress: 0x%08x\n", ll);
		}
	}

	if (ll != NULL) {
		ll_destroy(ll);

		*((unsigned long *)args[0].a_address) = (unsigned long)NULL;
	}

	if (flt_dbg != 0) fclose(dbg);

	return 0;
}

int filter_debug(void)
{
	int rv = 0;
	char * p = getenv("FILTER_DEBUG");

	if (p != NULL) {
		if (toupper(*p) == 'Y') rv = 1;
	}

	return rv;
}

int select_debug(void)
{
	int rv = 0;
	char * p = getenv("SELECT_DEBUG");

	if (p != NULL) {
		if (toupper(*p) == 'Y') rv = 1;
	}

	return rv;
}

void hex_dump(FILE * fp, char * buffer, size_t bufsize)
{
	char c;
	int i = 0;

	while (i < bufsize) {
		c = buffer[i++];
		fprintf(fp, "%02x", c);
	}
}
