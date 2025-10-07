#ifndef _SYSTEM_H
#define _SYSTEM_H

#define noreturn _Noreturn

typedef _Bool bool;
#define false ((bool)0)
#define true  ((bool)1)

#if __ia16__
# define far __far
#else
# define far
#endif

typedef   signed char   int8_t;
typedef unsigned char  uint8_t;
typedef   signed short  int16_t;
typedef unsigned short uint16_t;

#if __i386__ || __amd64__
typedef   signed int        int32_t;
typedef unsigned int       uint32_t;
#else
typedef   signed long       int32_t;
typedef unsigned long      uint32_t;
#endif

#if __amd64__
typedef   signed long       int64_t;
typedef unsigned long      uint64_t;
typedef   signed long       ssize_t;
typedef unsigned long       size_t;
#else
typedef   signed long long  int64_t;
typedef unsigned long long uint64_t;
typedef   signed int       ssize_t;
typedef unsigned int        size_t;
#endif

noreturn void exit(int status);
ssize_t write(int fd, const void * buf, size_t count);
ssize_t read(int fd, void * buf, size_t count);
size_t strlen(const char * s);
void putstr(const char * s);
void putint(unsigned int value);

#endif /* _SYSTEM_H */
