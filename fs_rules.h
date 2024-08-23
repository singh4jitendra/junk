/*
   Changed: 12/22/04 mjb - Added a preconditional statement to define the
                           PIPE_MAX symbol if it is not already defined.
*/

#if !defined(_FS_RULES_H)
#define _FS_RULES_H

#include <sys/statvfs.h>

#if !defined(PIPE_MAX)
#define PIPE_MAX 5120
#endif

#define FS_SPACE_MEASURE_BLOCKS    'b'
#define FS_SPACE_MEASURE_CHARS     'c'
#define FS_SPACE_MEASURE_PERCENT   '%'

#define FS_INODE_MEASURE_INODES    'i'
#define FS_INODE_MEASURE_PERCENT   '%'

#define FS_DEFAULT_IDENTIFIER      "DEFAULT"
#define FS_DEFAULT_SPACE_AMOUNT    15
#define FS_DEFAULT_SPACE_MEASURE   FS_SPACE_MEASURE_PERCENT
#define FS_DEFAULT_INODE_AMOUNT    15
#define FS_DEFAULT_INODE_MEASURE   FS_INODE_MEASURE_PERCENT

/* This structure holds the data specified in the free space configuration
	files.  The configuration file should have one record per line.  If
	the first character of the line is #, the line is considered to be a
	comment.  A free space record must conform to the following format:
		ID <identifier> SPACE amount[b|c|%] INODES amount[i|%]
	
	The rules are processed in the order they are read from the configuration
	file.
*/
struct fsrules {
	char id[256];
	u_int32_t space_amount;
	char space_measure; /* b - blocks, c - characters, % - percentage */
	u_int32_t inode_amount;
	char inode_measure; /* i - number, % - percentage */
};

int fs_configure_rules(char * file);
int fs_read_configuration_file(char * file);
void fs_configure_default_rule(void);
struct fsrules * fs_find_rule_by_id(char * id);
int fs_validate_free_space(struct fsrules * fsr, struct statvfs * vfs, u_int32_t offset);
int fs_validate_free_inodes(struct fsrules * fsr, struct statvfs * vfs);
void fs_purge_list(void);
void fs_print_list(void);
int fs_validate_disk_space_value(u_int32_t value, char modifier);
int fs_validate_inode_value(u_int32_t value, char modifier);

#endif
