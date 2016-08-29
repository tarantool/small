#ifndef _UNISTD_H_
#define _UNISTD_H_
#ifdef _WIN32
#pragma once

#include <crtdefs.h>
#include <io.h>
#include <direct.h>
#include <process.h>
//#pragma comment( lib, "ws2_32" )
//#include <winsock2.h>

#include <process.h>
#include <time.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef int	pid_t;			/* process id type	*/

#ifndef _SSIZE_T_DEFINED
typedef ptrdiff_t ssize_t;
#define _SSIZE_T_DEFINED
#endif

#ifndef getpid
#define  getpid() _getpid()
extern pid_t __cdecl _getpid(void);
#endif

//<sys/stat.h>
#define fstat64(fildes, stat) (_fstati64(fildes, stat))
#define stat64(path, buffer) (_stati64(path,buffer))

#ifdef	__cplusplus
}
#endif
#endif /* _WIN32 */
#endif /* _UNISTD_H_ */
