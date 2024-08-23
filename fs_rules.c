#include <sys/types.h>
#include <sys/statvfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "fs_rules.h"
#include "linkedlist.h"

linkedlist_t * list = NULL;

int fs_configure_rules(char * file)
{
	int rv;

	rv = fs_read_configuration_file(file);
	fs_configure_default_rule();

	return rv;
}

int fs_read_configuration_file(char * file)
{
	int rv;
	FILE * fp;
	struct fsrules * tmp;
	char buffer[PIPE_MAX];
	char space_c[256];
	char inode_c[256];
	char * uom;
	char * ptr;

	if (list == NULL) {
		list = ll_create(NULL);
		if (list == NULL)
			return -1;
	}

	fp = fopen(file, "r");
	if (fp == NULL)
		return -1;

	while (!feof(fp)) {
		if (fgets(buffer, sizeof(buffer), fp) != NULL) {
			if (*buffer == '#')
				continue;
	
			tmp = (struct fsrules *)malloc(sizeof(struct fsrules));
			if (tmp == NULL) {
				fclose(fp);
				return -1;
			}
	
			ptr = buffer + (strlen(buffer) - 1);
			if (*ptr == '\n')
				*ptr = 0;
	
			rv = sscanf(buffer, "ID %[^ ] SPACE %[^ ] INODES %[^ ]", tmp->id,
				space_c, inode_c);
			if (rv == 3) { /* number of fields successfully read */
				if (*space_c == '-') {
					print_msg("WARNING: SPACE value must be positive: %s\n",
						space_c);
					free(tmp);
					tmp = NULL;
					continue;
				}

				tmp->space_amount = strtoul(space_c, &uom, 10);
				switch (*uom) {
					case '\0':
						tmp->space_measure = FS_DEFAULT_SPACE_MEASURE;
						break;
					case FS_SPACE_MEASURE_BLOCKS:
					case FS_SPACE_MEASURE_CHARS:
					case FS_SPACE_MEASURE_PERCENT:
						tmp->space_measure = *uom;
						break;
					default:
						print_msg("WARNING: invalid SPACE modifier %c\n", *uom);
						fflush(stdout);
						free(tmp);
						tmp = NULL;
						continue;
				}
	
				if (!fs_validate_disk_space_value(tmp->space_amount, tmp->space_measure)) {
					free(tmp);
					tmp = NULL;
					continue;
				}

				if (*inode_c == '-') {
					print_msg("WARNING: INODES value must be positive: %s\n",
						inode_c);
					free(tmp);
					tmp = NULL;
					continue;
				}

				tmp->inode_amount = strtoul(inode_c, &uom, 10);
				switch (*uom) {
					case '\0':
						tmp->inode_measure = FS_DEFAULT_INODE_MEASURE;
						break;
					case FS_INODE_MEASURE_INODES:
					case FS_INODE_MEASURE_PERCENT:
						tmp->inode_measure = *uom;
						break;
					default:
						print_msg("WARNING: invalid INODES modifier %c\n", *uom);
						fflush(stdout);
						free(tmp);
						tmp = NULL;
						continue;
				}
	
				if (!fs_validate_inode_value(tmp->inode_amount, tmp->inode_measure)) {
					free(tmp);
					tmp = NULL;
					continue;
				}

				if (tmp != NULL) {
					ll_append(list, (void *)tmp);
				}
			}
			else {
				fprintf(stdout, "WARNING: invalid format: %s\n", buffer);
				fflush(stdout);
			}
		}
	}

	fclose(fp);

	return 0;
}

void fs_configure_default_rule(void)
{
	struct fsrules * tmp;

	tmp = fs_find_rule_by_id(FS_DEFAULT_IDENTIFIER);
	if (tmp == NULL) {
		tmp = malloc(sizeof(struct fsrules));
		if (tmp != NULL) {
			strcpy(tmp->id, FS_DEFAULT_IDENTIFIER);
			tmp->space_amount = FS_DEFAULT_SPACE_AMOUNT;
			tmp->space_measure = FS_DEFAULT_SPACE_MEASURE;
			tmp->inode_amount = FS_DEFAULT_INODE_AMOUNT;
			tmp->inode_measure = FS_DEFAULT_INODE_MEASURE;
			ll_append(list, (void *)tmp);
		}
	}
}

struct fsrules * fs_find_rule_by_id(char * id)
{
	linkedlist_t * i;
	struct fsrules * fsr;

	i = ll_create_iterator(list);
	if (i != NULL) {
		fsr = (struct fsrules *)ll_move_first(i);
		while (fsr != NULL) {
			if (strcmp(id, fsr->id) == 0) {
				ll_destroy_iterator(i);
				return fsr;
			}

			fsr = (struct fsrules *)ll_move_next(i);
		}
	}

	ll_destroy_iterator(i);

	return NULL;
}

int fs_validate_free_space(struct fsrules * fsr, struct statvfs * vfs, u_int32_t offset)
{
	int rv;
	double pct;

	switch (fsr->space_measure) {
		case FS_SPACE_MEASURE_BLOCKS:
			rv = ((vfs->f_bavail - (((double)offset)/vfs->f_frsize)) >= fsr->space_amount);
			break;
		case FS_SPACE_MEASURE_CHARS:
			rv = (((vfs->f_bavail * vfs->f_frsize) - offset) >= fsr->space_amount);
			break;
		case FS_SPACE_MEASURE_PERCENT:
		default:
			pct = fsr->space_amount / 100.0;
			rv = ((vfs->f_bavail - (((double)offset)/vfs->f_frsize)) >= (vfs->f_blocks * pct));
			break;
	}

	return rv;
}

int fs_validate_free_inodes(struct fsrules * fsr, struct statvfs * vfs)
{
	int rv;
	double pct;

	switch (fsr->inode_measure) {
		case FS_INODE_MEASURE_INODES:
			rv = (vfs->f_favail >= fsr->inode_amount);
			break;
		case FS_INODE_MEASURE_PERCENT:
		default:
			pct = fsr->inode_amount / 100.0;
			rv = (vfs->f_bavail >= (vfs->f_blocks * pct));
			break;
	}

	return rv;
}

void fs_purge_list(void)
{
	ll_destroy(list);
}

void fs_print_list(void)
{
	linkedlist_t * i;
	struct fsrules * f;

	i = ll_create_iterator(list);
	if (i != NULL) {
		printf("**** list ****\n");
		f = (struct fsrules *)ll_move_first(i);
		while (f != NULL) {
			printf("DEBUG: fs_print_list: ID %s SPACE %d%c INODES %d%c\n",
				f->id, f->space_amount,
				f->space_measure, f->inode_amount, f->inode_measure);
			f = (struct fsrules *)ll_move_next(i);
		}
	}

	ll_destroy_iterator(i);
}

int fs_validate_disk_space_value(u_int32_t value, char modifier)
{
	int rv;

	switch (modifier) {
		case FS_SPACE_MEASURE_BLOCKS:
		case FS_SPACE_MEASURE_CHARS:
			rv = (value <= UINT_MAX);
			break;
		case FS_SPACE_MEASURE_PERCENT:
			rv = (value <= 100);
			if (rv == 0) {
				print_msg("WARNING: invalid free space value %d%%\n", value);
				fflush(stdout);
			}

			break;
		default:
			rv = 0;
	}

	return rv;
}

int fs_validate_inode_value(u_int32_t value, char modifier)
{
	int rv;

	switch (modifier) {
		case FS_INODE_MEASURE_INODES:
			rv = (value <= UINT_MAX);
			break;
		case FS_INODE_MEASURE_PERCENT:
			rv = (value <= 100);
			if (rv == 0) {
				print_msg("WARNING: invalid inodes value %d%%\n", value);
				fflush(stdout);
			}

			break;
		default:
			rv = 0;
	}

	return rv;
}
