// Marc B. Reynolds, 2013-2024
// Public Domain under http://unlicense.org, see link for details.
//
// Documentation: http://marc-b-reynolds.github.io/shf/2016/04/19/prns.html

// Short description:
// * 

#ifndef PRNS_H
#define PRNS_H

// macro configurations to define (if desired) prior to including
// this header:
// 
// PRNS_SMALLCRUSH: weaker (and cheaper) mixing function which is
// sufficient to pass SmallCrush. If undefined the generator will
// pass Crush.
//
// PRNS_MIX: if defined overrides the mixing functions defined
// in this file.
//
// PRNS_WEYL:
// PRNS_WEYL_D:

#include <stdint.h>

#ifndef PRNS_SMALLCRUSH
#define PRNS_SMALLCRUSH
#endif

typedef struct { uint64_t i;  } prns_t;
typedef struct { uint64_t i,k;} prns_down_t;


//***************************************************************
//*** mixing function portion (start)

// only needed if no user defined version provided
#if !defined(PRNS_MIX)

// choose between mixing functions
#ifndef PRNS_MIX_VERSION
#define PRNS_MIX_VERSION 1
#endif

#define PRNS_MIX(X) prns_mix(X)

#if (PRNS_MIX_VERSION == 1)

#if !defined(PRNS_SMALLCRUSH)

#ifndef PRNS_MIX_S0
#ifdef  PRNS_MIX_13
#define PRNS_MIX_S0 30
#define PRNS_MIX_S1 27
#define PRNS_MIX_S2 31
#define PRNS_MIX_M0 UINT64_C(0xbf58476d1ce4e5b9)
#define PRNS_MIX_M1 UINT64_C(0x94d049bb133111eb)
#else
#define PRNS_MIX_S0 31
#define PRNS_MIX_S1 27
#define PRNS_MIX_S2 33
#define PRNS_MIX_M0 UINT64_C(0x7fb5d329728ea185)
#define PRNS_MIX_M1 UINT64_C(0x81dadef4bc2dd44d)
#endif
#endif

static inline uint64_t prns_mix(uint64_t x)
{
  x ^= (x >> PRNS_MIX_S0);
  x *= PRNS_MIX_M0;
  x ^= (x >> PRNS_MIX_S1);	
  x *= PRNS_MIX_M1;
  
#ifndef PRNS_NO_FINAL_XORSHIFT
  x ^= (x >> PRNS_MIX_S2);
#endif

  return x;
}

#else

static inline uint64_t prns_mix(uint64_t x)
{
  x ^= (x >> 33);
  x *= UINT64_C(0xbf58476d1ce4e5b9);

  return x;
}

#endif

#elif (PRNS_MIX_VERSION == 2)

#if defined(PRNS_MIX_DEFAULT)||defined(PRNS_MIX_D_DEFAULT)
#if defined(__ARM_ARCH)
#if defined(__ARM_FEATURE_CRC32)
static inline uint64_t prns_crc32c_64(uint64_t x, uint32_t k) { return __crc32cd(k,x); }
#endif
#endif
#else
static inline uint64_t prns_crc32c_64(uint64_t x, uint32_t k) { return _mm_crc32_u64(k,x); }
#endif

// all wip: need to recheck some math (sigh)

static inline uint64_t prns_mix(uint64_t x)
{
  uint64_t r = x;

  r = prns_crc32c_64(r,0x9e3d2c1b) ^ (r ^ (r >> 17));
  r ^= (r*r) & UINT64_C(-2);
  r = prns_crc32c_64(r,0x9e3d2c1b) ^ (r ^ (r >> 23));
  r ^= (r*r) & UINT64_C(-2);
  
  return r;
}


static inline uint64_t prns_down_mix(uint64_t x, uint64_t k)
{
  uint64_t r = x ^ k;

  r = prns_crc32c_64(r,0x9e3d2c1b) ^ (r);
  r ^= (r*r) & UINT64_C(-2);
  r = prns_crc32c_64(r,0x9e3d2c1b) ^ (r);
  r ^= (r*r) & UINT64_C(-2);

  return r;
}

#endif



#ifndef PRNS_MIX
#define PRNS_MIX_DEFAULT
#ifndef PRNS_SMALLCRUSH
#define PRNS_MIX(X) prns_mix(X)
#else
#define PRNS_MIX(X) prns_min_mix(X)
#endif
#endif 


#endif


#if !defined(PRNS_MIX_D)
#define PRNS_MIX_D(X,K) prns_mix((X)^(K))
#endif


//*** mixing function portion (end)
//***************************************************************

// Weyl constant choices
#ifndef PRNS_WEYL
#define PRNS_WEYL   UINT64_C(0x61c8864680b583eb)
#define PRNS_WEYL_I UINT64_C(0x0e217c1e66c88cc3)
#endif

#ifndef PRNS_WEYL_D
#define PRNS_WEYL_D   UINT64_C(0x4f1bbcdcbfa54001)
#define PRNS_WEYL_D_I UINT64_C(0x4f1bbcdcbfa54001) // NO! compute it and place
#endif


// return the position in the stream
static inline uint64_t prns_tell(prns_t* gen)
{
  return gen->i * PRNS_WEYL_I;
}

// sets the position in the stream
static inline void prns_set(prns_t* gen, uint64_t pos)
{
  gen->i = PRNS_WEYL*pos;
}

// moves the stream position by 'offset'
static inline void prns_seek(prns_t* gen, int64_t offset)
{
  gen->i += PRNS_WEYL*((uint64_t)offset);
}

// returns the random number at position 'n'
static inline uint64_t prns_at(uint64_t n)
{
  return PRNS_MIX(PRNS_WEYL*n);
}

// returns the current random number without advancing the position
static inline uint64_t prns_peek(prns_t* gen)
{
  return PRNS_MIX(gen->i);
}

// return the current random number and advances the position by one
static inline uint64_t prns_next(prns_t* gen)
{
  uint64_t i = gen->i;
  uint64_t r = PRNS_MIX(i);
  gen->i = i + PRNS_WEYL;
  return r;
}

// return the current random number and moves the position by backward by one
static inline uint64_t prns_prev(prns_t* gen)
{
  uint64_t i = gen->i;
  uint64_t r = PRNS_MIX(i);
  gen->i = i - PRNS_WEYL;
  return r;
}

//**** "down" functions

static inline void prns_down_init(prns_down_t* d, prns_t* s)
{
  d->i = 0;
  d->k = s->i;
}

static inline uint64_t prns_down_tell(prns_down_t* gen)
{
  return gen->i * PRNS_WEYL_D_I;
}

static inline void prns_down_set(prns_down_t* gen, uint64_t pos)
{
  gen->i = PRNS_WEYL_D*pos;
}

static inline void prns_down_seek(prns_down_t* gen, int64_t offset)
{
  gen->i += PRNS_WEYL_D*((uint64_t)offset);
}

static inline uint64_t prns_down_at(prns_down_t* gen, uint64_t n)
{
  return PRNS_MIX_D(PRNS_WEYL_D*n, gen->k);
}

static inline uint64_t prns_down_peek(prns_down_t* gen)
{
  return PRNS_MIX_D(gen->i, gen->k);
}

static inline uint64_t prns_down_next(prns_down_t* gen)
{
  uint64_t i = gen->i;
  uint64_t r = PRNS_MIX_D(i, gen->k);
  gen->i = i + PRNS_WEYL_D;
  return r;
}

static inline uint64_t prns_down_prev(prns_down_t* gen)
{
  uint64_t i = gen->i;
  uint64_t r = PRNS_MIX_D(i, gen->k);
  gen->i = i - PRNS_WEYL_D;
  return r;
}

#endif
