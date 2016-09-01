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

//<sys/stat.h>
#define fstat64(fildes, stat) (_fstati64(fildes, stat))
#define stat64(path, buffer) (_stati64(path,buffer))

// uio buffers defined, as it was for writev, readv.
struct iovec {
  void * iov_base;
  size_t iov_len;
};

// TODO - implement these separately for file and socket descriptors
// ssize_t readv(int filedes, const struct iovec *vector, int count);
// ssize_t writev(int filedes, const struct iovec *vector, int count);


#ifdef	__cplusplus
}
#endif
#endif /* _WIN32 */
#endif /* _UNISTD_H_ */
