#ifndef __SUPPORT_H
#define __SUPPORT_H

#include <stdint.h>
#include <float.h>

#ifdef __SIZEOF_INT128__
typedef          __int128  int128_t;
typedef unsigned __int128 uint128_t;
#else
typedef struct
{
# if BYTE_ORDER == LITTLE_ENDIAN
	uint64_t l, h;
# elif BYTE_ORDER == BIG_ENDIAN
	uint64_t h, l;
# endif
} uint128_t;

typedef struct
{
# if BYTE_ORDER == LITTLE_ENDIAN
	uint64_t l;
	int64_t h;
# elif BYTE_ORDER == BIG_ENDIAN
	int64_t h;
	uint64_t l;
# endif
} int128_t;
#endif

#if FLT_RADIX == 2
# if LDBL_MANT_DIG >= 63 && LDBL_MIN_EXP <= -13681 && LDBL_MAX_EXP >= 16384
#  define _SUPPORT_FLOAT80 1
typedef long double float80_t;
#  if LDBL_MANT_DIG == 64
#   define _FLOAT80_EXACT 1
#  endif
# endif

# if DBL_MANT_DIG >= 53 && DBL_MIN_EXP <= -1021 && DBL_MAX_EXP >= 1024
#  define _SUPPORT_FLOAT64 1
typedef double float64_t;
#  if DBL_MANT_DIG == 53
#   define _FLOAT64_EXACT 1 // casting to float64_t will yield the correct truncated value
#  endif
# elif _SUPPORT_FLOAT80
#  define _SUPPORT_FLOAT64 1
typedef float80_t float64_t;
# endif

# if FLT_MANT_DIG >= 24 && FLT_MIN_EXP <= -125 && FLT_MAX_EXP >= 128
#  define _SUPPORT_FLOAT32 1
typedef float float32_t;
#  if FLT_MANT_DIG == 24
#   define _FLOAT32_EXACT 1 // casting to float32_t will yield the correct truncated value
#  endif
# elif _SUPPORT_FLOAT64
#  define _SUPPORT_FLOAT32 1
typedef float64_t float32_t;
# endif
#endif

#ifdef __SIZEOF_INT128__
# define _div128(y, z, dxax, x) ((y) = (dxax) / (x), (z) = (dxax) % (x))
# define _idiv128(y, z, dxax, x) ((y) = (dxax) / (x), (z) = (dxax) % (x))
# define _mul128(z, x, y) ((z) = (_uint128)(x) * (_uint128)(y))
# define _imul128(z, x, y) ((z) = (_int128)(x) * (_int128)(y))
# define _cons128(x, y) (((_uint128)(x) << 64) | (y))
# define _icons128(x, y) (((_int128)(x) << 64) | (y))
# define _overflow128(x) ((x) != (_uint128)(_uint64)(x))
# define _ioverflow128(x) ((x) != (_int128)(_int64)(x))
# define _low128(x) ((_uint64)(x))
# define _high128(x) ((_uint64)((x) >> 64))
#else
static inline void _div128(uint64_t * _y, uint64_t * _z, uint128_t dxax, uint64_t x)
{
	(void)_y;
	(void)_z;
	(void)dxax;
	(void)x;
	/* TODO */
}

static inline void _idiv128(int64_t * _y, int64_t * _z, int128_t dxax, int64_t x)
{
	(void)_y;
	(void)_z;
	(void)dxax;
	(void)x;
	/* TODO */
}

static inline void _addu(uint128_t * _x, uint64_t y)
{
	uint64_t xl = _x->l;
	uint64_t x0 = xl + y;
	if((int64_t)((xl & y) | (xl & ~x0) | (y & ~x0)) < 0)
		_x->h += 1;
	_x->l = x0;
}

static inline void _mul128(uint128_t * _z, uint64_t x, uint64_t y)
{
	uint128_t z;
	z.l = (x >> 32) * (y >> 32);
	z.h = z.l >> 32;
	z.l <<= 32;
	_addu(&z, (x >> 32) * (uint32_t)y);
	_addu(&z, (uint32_t)x * (y >> 32));
	z.h = (z.h << 32) | (z.l >> 32);
	z.l <<= 32;
	_addu(&z, (uint32_t)x * (uint32_t)y);
	*_z = z;
}

static inline void _adds(int128_t * _x, int64_t y)
{
	uint64_t xl = _x->l;
	uint64_t x0 = xl + y;
	int64_t xh = 0;
	if(y < 0)
		xh -= 1;
	if((int64_t)((xl & y) | (xl & ~x0) | (y & ~x0)) < 0)
		xh += 1;
	_x->l = x0;
	if(xh != 0)
		_x->h += xh;
}

static inline void _imul128(int128_t * _z, int64_t x, int64_t y)
{
	int128_t z;
	z.l = (x >> 32) * (y >> 32);
	z.h = z.l >> 32;
	z.l <<= 32;
	_adds(&z, (x >> 32) * (int64_t)(uint32_t)y);
	_adds(&z, (int64_t)(uint32_t)x * (y >> 32));
	z.h = (z.h << 32) | (z.l >> 32);
	z.l <<= 32;
	_addu((uint128_t *)&z, (uint32_t)x * (uint32_t)y);
	*_z = z;
}
# define _div128(y, z, dxax, x) _div128(&(y), &(z), dxax, x)
# define _idiv128(y, z, dxax, x) _idiv128((int64_t *)&(y), (int64_t *)&(z), dxax, x)
# define _mul128(z, x, y) _mul128(&(z), x, y)
# define _imul128(z, x, y) _imul128(&(z), x, y)
# define _cons128(x, y) ((_uint128) { .h = (x), .l = (y) })
# define _icons128(x, y) ((_int128) { .h = (x), .l = (y) })
# define _overflow128(x) ((x).h != 0)
# define _ioverflow128(x) ((_int64)(x).l < 0 ? (x).h != -1 : (x).h != 0)
# define _low128(x) ((x).l)
# define _high128(x) ((x).h)
#endif

#endif // __SUPPORT_H
