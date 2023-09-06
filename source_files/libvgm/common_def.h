#ifndef __COMMON_DEF_H__
#define __COMMON_DEF_H__

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t	     UINT8;
typedef  int8_t	      INT8;
typedef uint16_t	UINT16;
typedef  int16_t	 INT16;
typedef uint32_t	UINT32;
typedef  int32_t	 INT32;
typedef uint64_t	UINT64;
typedef  int64_t	 INT64;

#ifndef INLINE
#if defined(_MSC_VER)
#define INLINE	static __inline	// __forceinline
#elif defined(__GNUC__)
#define INLINE	static __inline__
#else
#define INLINE	static inline
#endif
#endif	// INLINE

#endif	// __COMMON_DEF_H__
