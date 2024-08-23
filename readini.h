#ifndef _READINI_H_
#define _READINI_H_

int GetPrivateProfileInt(char *section, char *entry, int defalt, char *filename);
int GetPrivateProfileString(char *section, char *entry, char *buffer, int buffsize, char *defalt, char *filename);

#endif
