#ifndef WS_COMPAT_H
#define WS_COMPAT_H

/* This file defines some types used by MS Windows.  They are used to make
	porting the code a little easier. */

typedef void *wesco_void_ptr_t;

#if defined(_WIN32)
   typedef SOCKET wesco_socket_t;

   typedef char  int8_t;
   typedef short int16_t;
   typedef int  int32_t;

   typedef unsigned char  u_int8_t;
   typedef unsigned short u_int16_t;
   typedef unsigned int  u_int32_t;

   typedef BOOL   bool_t;
   typedef HANDLE wesco_handle_t;

#   define wesco_closesocket(x)	       closesocket(x)
#   define wesco_CloseHandle(x)         CloseHandle(x)
#   define wesco_SetCurrentDirectory(x) SetCurrentDirectory(x)
#   define wesco_lstrlen(x)             lstrlen(x)
#   define wesco_lstrcpyn(s1,s2,sz)     lstrncpy(s1, s2, sz)
#   define wesco_lstrcpy(s1,s2)         lstrcpy(s1, s2)
#   define wesco_lstrcmp(s1,s2)         lstrcmp(s1, s2)
#   define wesco_lstrcat(s1,s2)         lstrcat(s1, s2)
#   define wesco_wsprintf               wsprintf

#   define SET_FILE_CREATION_MASK(x)    0
#   define FCM_ALLOW_ALL                0
#else
   typedef int wesco_socket_t;
   typedef int wesco_handle_t;
	typedef int bool_t;

#   define FAR

#   ifndef FALSE
#      define FALSE 0
#   endif      /* ifndef FALSE */

#   ifndef TRUE
#      define TRUE 1
#   endif      /* ifndef TRUE */

#   define INVALID_SOCKET	-1
#   define SOCKET_ERROR		-1
#   define INVALID_HANDLE_VALUE -1

#   define WINAPI

#   define wesco_closesocket(x)	       close(x)
#   define wesco_CloseHandle(x)         close(x)
#   define wesco_SetCurrentDirectory(x) chdir(x)
#   define wesco_lstrlen(x)             strlen(x)
#   define wesco_lstrcpyn(s1,s2,sz)     strncpy(s1, s2, sz)
#   define wesco_lstrcpy(s1,s2)         strcpy(s1, s2)
#   define wesco_lstrcmp(s1,s2)         strcmp(s1, s2)
#   define wesco_lstrcat(s1,s2)         strcat(s1, s2)
#   define wesco_wsprintf               sprintf

#   if !defined(S_IRWXU)
#      include <fcntl.h>
#   endif

#   define SET_FILE_CREATION_MASK(x)    umask(x)
#   define FCM_ALLOW_ALL                (~(S_IRWXU|S_IRWXG|S_IRWXO))
#endif

#endif
