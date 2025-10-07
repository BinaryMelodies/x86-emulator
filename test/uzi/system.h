#ifndef _SYSTEM_H
#define _SYSTEM_H

#define noreturn _Noreturn

typedef _Bool bool;
#define false ((bool)0)
#define true  ((bool)1)

#define far

typedef   signed char   int8_t;
typedef unsigned char  uint8_t;
typedef   signed short  int16_t;
typedef unsigned short uint16_t;

typedef   signed long       int32_t;
typedef unsigned long      uint32_t;

typedef   signed long long  int64_t;
typedef unsigned long long uint64_t;
typedef   signed int       ssize_t;
typedef unsigned int        size_t;

noreturn void exit(int status) __smallc;
ssize_t write(int fd, const void * buf, size_t count) __smallc;
size_t strlen(const char * s);
void putstr(const char * s);
void putint(unsigned int value);

#endif /* _SYSTEM_H */
