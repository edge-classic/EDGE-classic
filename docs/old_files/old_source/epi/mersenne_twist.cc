//------------------------------------------------------------------------
// EPI MersenneTwister.h
//------------------------------------------------------------------------
// Mersenne Twister random number generator -- a C++ class MTRand
// Based on code by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus
// Richard J. Wagner  v1.0  15 May 2003  rjwagner@writeme.com
//------------------------------------------------------------------------
// Simplified for use in the EDGE engine : Andrew Apted, April 2005.
//------------------------------------------------------------------------

/* Please see the extensive comments in mersenne_twist.h */

#include "epi.h"
#include "mersenne_twist.h"

namespace epi
{

void MTRand::seed(u32_t oneSeed)
{
	// Seed the generator with a simple u32_t
	initialize(oneSeed);
	reload();
}

u32_t MTRand::rand(u32_t n)
{
	// Find which bits are used in n
	// Optimized by Magnus Jonsson (magnus@smartelectronix.com)
	u32_t used = n;

	used |= used >> 1;
	used |= used >> 2;
	used |= used >> 4;
	used |= used >> 8;
	used |= used >> 16;
	
	// Draw numbers until one is found in [0,n]
	u32_t i;

	do
		i = rand() & used;  // toss unused bits to shorten search
	while (i > n);

	return i;
}

void MTRand::seed(const u32_t *bigSeed, int seedLength)
{
	// Seed the generator with an array of uint32's
	// There are 2^19937-1 possible initial states.  This function allows
	// all of those to be accessed by providing at least 19937 bits (with a
	// default seed length of N = 624 uint32's).  Any bits above the lower 32
	// in each element are discarded.
	initialize(19650218UL);

	int i = 1;
	int j = 0;
	int k = ( N > seedLength ? N : seedLength );

	for (; k; k--)
	{
		state[i] = state[i] ^ ((state[i-1] ^ (state[i-1] >> 30)) * 1664525UL);
		state[i] += (bigSeed[j] & 0xffffffffUL) + (u32_t) j;
		state[i] &= 0xffffffffUL;

		i++;
		if (i >= N)
		{
			state[0] = state[N-1];
			i = 1;
		}

		j++;
		if (j >= seedLength)
			j = 0;
	}

	for (k = N - 1; k; k--)
	{
		state[i] = state[i] ^ ((state[i-1] ^ (state[i-1] >> 30)) * 1566083941UL);
		state[i] -= i;
		state[i] &= 0xffffffffUL;

		i++;
		if (i >= N)
		{
			state[0] = state[N-1];
			i = 1;
		}
	}

	state[0] = 0x80000000UL;  // MSB is 1, assuring non-zero initial array
	reload();
}

void MTRand::initialize(const u32_t seed)
{
	// Initialize generator state with seed
	// See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
	// In previous versions, most significant bits (MSBs) of the seed affect
	// only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto.

	u32_t *s = state;
	u32_t *r = state;

	*s++ = seed & 0xffffffffUL;

	for (int i = 1; i < N; i++, r++)
	{
		*s++ = (1812433253UL * (*r ^ (*r >> 30)) + i) & 0xffffffffUL;
	}
}

inline void MTRand::reload()
{
	// Generate N new values in state
	// Made clearer and faster by Matthew Bellew (matthew.bellew@home.com)
	u32_t *p = state;

	for (int i = N - M; i--; p++)
		*p = twist(p[M], p[0], p[1]);

	for (int j = M; --j; p++)
		*p = twist(p[M-N], p[0], p[1]);

	*p = twist(p[M-N], p[0], state[0]);

	left  = N;
	pNext = state;
}

void MTRand::save(u32_t *dest) const
{
	for (int i = 0; i < N; i++)
		*dest++ = state[i];

	*dest = left;
}

void MTRand::load(const u32_t *source)
{
	for (int i = 0; i < N; i++)
		state[i] = *source++;

	left = *source;
	pNext = &state[N-left];
}

}  // namespace epi


//------------------------------------------------------------------------
//
//  TEST ROUTINES
//

#ifdef TEST_MTRAND_CLASS

#include <iostream>

#define VERIFY_LEN  80

static u32_t mtrand_verify_data[VERIFY_LEN] =
{
	1067595299U,  955945823U,  477289528U, 4107218783U, 4228976476U,
	3344332714U, 3355579695U,  227628506U,  810200273U, 2591290167U,
	2560260675U, 3242736208U,  646746669U, 1479517882U, 4245472273U,
	1143372638U, 3863670494U, 3221021970U, 1773610557U, 1138697238U,
	1421897700U, 1269916527U, 2859934041U, 1764463362U, 3874892047U,
	3965319921U,   72549643U, 2383988930U, 2600218693U, 3237492380U,
	2792901476U,  725331109U,  605841842U,  271258942U,  715137098U,
	3297999536U, 1322965544U, 4229579109U, 1395091102U, 3735697720U,
	2101727825U, 3730287744U, 2950434330U, 1661921839U, 2895579582U,
	2370511479U, 1004092106U, 2247096681U, 2111242379U, 3237345263U,
	4082424759U,  219785033U, 2454039889U, 3709582971U,  835606218U,
	2411949883U, 2735205030U,  756421180U, 2175209704U, 1873865952U,
	2762534237U, 4161807854U, 3351099340U,  181129879U, 3269891896U,
	 776029799U, 2218161979U, 3001745796U, 1866825872U, 2133627728U,
	  34862734U, 1191934573U, 3102311354U, 2916517763U, 1012402762U,
	2184831317U, 4257399449U, 2899497138U, 3818095062U, 3030756734U
};

void Test_MTRand(void)
{
	std::cout << "\n---TEST-MERSENNE_TWISTER---------------------\n";

	u32_t init_data[4] = { 0x123, 0x234, 0x345, 0x456 };

	epi::MTRand mt_gen(0);  // dummy seed

	mt_gen.seed(init_data, 4);

	for (int i=0; i < VERIFY_LEN; i++)
	{
		u32_t val = mt_gen.rand();

		const char *result = (val == mtrand_verify_data[i]) ? "OK" : "ERROR";

		std::cout << "Generated: " << val << " Should be: ";
		std::cout << mtrand_verify_data[i] << " ..... " << result << "\n";
	}

	std::cout << "-------------------------------------------\n\n";
}

#endif  /* TEST_MTRAND_CLASS */


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
