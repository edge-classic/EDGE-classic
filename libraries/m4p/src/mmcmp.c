/* MMCMP (Zirconia) decompressor.
** Taken from Mmcmp.cpp (ModPlug Tracker source code) and converted
** from C++ to C. libmodplug is public domain, so this file should be
** able to go under BSD 3-clause.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MMCMP_COMP  0x0001
#define MMCMP_DELTA 0x0002
#define MMCMP_16BIT 0x0004
#define MMCMP_ABS16 0x0200

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct MMCMPFILEHEADER
{
	uint32_t id_ziRC; // "ziRC"
	uint32_t id_ONia; // "ONia"
	uint16_t hdrsize;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
MMCMPFILEHEADER, *LPMMCMPFILEHEADER;

typedef struct MMCMPHEADER
{
	uint16_t version;
	uint16_t nblocks;
	uint32_t filesize;
	uint32_t blktable;
	uint8_t glb_comp;
	uint8_t fmt_comp;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
MMCMPHEADER, *LPMMCMPHEADER;

typedef struct MMCMPBLOCK
{
	uint32_t unpk_size;
	uint32_t pk_size;
	uint32_t xor_chk;
	uint16_t sub_blk;
	uint16_t flags;
	uint16_t tt_entries;
	uint16_t num_bits;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
MMCMPBLOCK, *LPMMCMPBLOCK;

typedef struct MMCMPSUBBLOCK
{
	uint32_t unpk_pos;
	uint32_t unpk_size;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
MMCMPSUBBLOCK, *LPMMCMPSUBBLOCK;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct MMCMPBITBUFFER
{
	uint32_t bitcount;
	uint32_t bitbuffer;
	const uint8_t *pSrc;
	const uint8_t *pEnd;
} MMCMPBITBUFFER;

static uint32_t GetBits(MMCMPBITBUFFER *bb, uint32_t nBits)
{
	if (!nBits)
		return 0;

	while (bb->bitcount < 24)
	{
		bb->bitbuffer |= ((bb->pSrc < bb->pEnd) ? *bb->pSrc++ : 0) << bb->bitcount;
		bb->bitcount += 8;
	}

	uint32_t d = bb->bitbuffer & ((1 << nBits) - 1);

	bb->bitbuffer >>= nBits;
	bb->bitcount -= nBits;

	return d;
}

static const uint8_t MMCMP8BitCommands[8] = { 0x01, 0x03, 0x07, 0x0F, 0x1E, 0x3C, 0x78, 0xF8 };
static const uint8_t MMCMP16BitFetch[16] = { 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t MMCMP8BitFetch[8] = { 3, 3, 3, 3, 2, 1, 0, 0 };
static const uint16_t MMCMP16BitCommands[16] =
{
	 0x01,  0x03,  0x07,  0x0F,  0x1E,    0x3C,   0x78,   0xF0,
	0x1F0, 0x3F0, 0x7F0, 0xFF0, 0x1FF0, 0x3FF0, 0x7FF0, 0xFFF0
};

bool unpackMMCMP(uint8_t **ppMemFile, uint32_t *pdwMemLength)
{
	uint32_t dwMemLength = *pdwMemLength;
	const uint8_t *lpMemFile = *ppMemFile;
	uint8_t *pBuffer;
	LPMMCMPFILEHEADER pmfh = (LPMMCMPFILEHEADER)lpMemFile;
	LPMMCMPHEADER pmmh = (LPMMCMPHEADER)(lpMemFile+10);
	uint32_t *pblk_table;
	uint32_t dwFileSize;

	if ((dwMemLength < 256) || !pmfh || pmfh->id_ziRC != 0x4352697A || pmfh->id_ONia != 0x61694e4f || pmfh->hdrsize < 14
		|| !pmmh->nblocks || pmmh->filesize < 16 || pmmh->filesize > 0x8000000 || pmmh->blktable >= dwMemLength
		|| pmmh->blktable + 4*pmmh->nblocks > dwMemLength)
	{
		return false;
	}

	dwFileSize = pmmh->filesize;
	
	pBuffer = (uint8_t *)malloc((dwFileSize + 31) & ~15);
	if (pBuffer == NULL)
		return false;
	
	pblk_table = (uint32_t *)(lpMemFile+pmmh->blktable);
	for (uint32_t nBlock = 0; nBlock < pmmh->nblocks; nBlock++)
	{
		uint32_t dwMemPos = pblk_table[nBlock];
		LPMMCMPBLOCK pblk = (LPMMCMPBLOCK)(lpMemFile+dwMemPos);
		LPMMCMPSUBBLOCK psubblk = (LPMMCMPSUBBLOCK)(lpMemFile+dwMemPos+20);

		if (dwMemPos+20 >= dwMemLength || dwMemPos+20+pblk->sub_blk*8 >= dwMemLength)
			break;

		dwMemPos += 20+pblk->sub_blk*8;

		// Data is not packed
		if (!(pblk->flags & MMCMP_COMP))
		{
			for (uint32_t i = 0; i<pblk->sub_blk; i++)
			{
				if (psubblk->unpk_pos > dwFileSize || psubblk->unpk_pos+psubblk->unpk_size > dwFileSize)
					break;

				memcpy(pBuffer+psubblk->unpk_pos, lpMemFile+dwMemPos, psubblk->unpk_size);
				dwMemPos += psubblk->unpk_size;
				psubblk++;
			}
		}
		else if (pblk->flags & MMCMP_16BIT) // Data is 16-bit packed
		{
			MMCMPBITBUFFER bb;
			uint16_t *pDest = (uint16_t *)(pBuffer + psubblk->unpk_pos);
			uint32_t dwSize = psubblk->unpk_size >> 1;
			uint32_t dwPos = 0;
			uint32_t numbits = pblk->num_bits;
			uint32_t subblk = 0, oldval = 0;

			bb.bitcount = 0;
			bb.bitbuffer = 0;
			bb.pSrc = lpMemFile+dwMemPos+pblk->tt_entries;
			bb.pEnd = lpMemFile+dwMemPos+pblk->pk_size;
			
			while (subblk < pblk->sub_blk)
			{
				uint32_t newval = 0x10000;
				uint32_t d = GetBits(&bb, numbits+1);

				if (d >= MMCMP16BitCommands[numbits])
				{
					uint32_t nFetch = MMCMP16BitFetch[numbits];
					uint32_t newbits = GetBits(&bb, nFetch) + ((d - MMCMP16BitCommands[numbits]) << nFetch);

					if (newbits != numbits)
					{
						numbits = newbits & 0x0F;
					}
					else
					{
						d = GetBits(&bb, 4);
						if (d == 0x0F)
						{
							if (GetBits(&bb, 1))
								break;

							newval = 0xFFFF;
						}
						else
						{
							newval = 0xFFF0 + d;
						}
					}
				}
				else
				{
					newval = d;
				}

				if (newval < 0x10000)
				{
					newval = (newval & 1) ? (uint32_t)(-(int32_t)((newval+1) >> 1)) : (uint32_t)(newval >> 1);
					if (pblk->flags & MMCMP_DELTA)
					{
						newval += oldval;
						oldval = newval;
					}
					else if (!(pblk->flags & MMCMP_ABS16))
					{
						newval ^= 0x8000;
					}
					
					pDest[dwPos++] = (uint16_t)newval;
				}

				if (dwPos >= dwSize)
				{
					subblk++;
					dwPos = 0;
					dwSize = psubblk[subblk].unpk_size >> 1;
					pDest = (uint16_t *)(pBuffer + psubblk[subblk].unpk_pos);
				}
			}
		}
		else // Data is 8-bit packed
		{
			MMCMPBITBUFFER bb;
			uint8_t *pDest = pBuffer + psubblk->unpk_pos;
			uint32_t dwSize = psubblk->unpk_size;
			uint32_t dwPos = 0;
			uint32_t numbits = pblk->num_bits;
			uint32_t subblk = 0, oldval = 0;
			const uint8_t *ptable = lpMemFile+dwMemPos;

			bb.bitcount = 0;
			bb.bitbuffer = 0;
			bb.pSrc = lpMemFile+dwMemPos+pblk->tt_entries;
			bb.pEnd = lpMemFile+dwMemPos+pblk->pk_size;
			while (subblk < pblk->sub_blk)
			{
				uint32_t newval = 0x100;
				uint32_t d = GetBits(&bb, numbits+1);

				if (d >= MMCMP8BitCommands[numbits])
				{
					uint32_t nFetch = MMCMP8BitFetch[numbits];
					uint32_t newbits = GetBits(&bb, nFetch) + ((d - MMCMP8BitCommands[numbits]) << nFetch);
					if (newbits != numbits)
					{
						numbits = newbits & 0x07;
					}
					else
					{
						d = GetBits(&bb, 3);
						if (d == 7)
						{
							if (GetBits(&bb, 1))
								break;

							newval = 0xFF;
						}
						else
						{
							newval = 0xF8 + d;
						}
					}
				}
				else
				{
					newval = d;
				}

				if (newval < 0x100)
				{
					int32_t n = ptable[newval];
					if (pblk->flags & MMCMP_DELTA)
					{
						n += oldval;
						oldval = n;
					}
					
					pDest[dwPos++] = (uint8_t)n;
				}

				if (dwPos >= dwSize)
				{
					subblk++;
					dwPos = 0;
					dwSize = psubblk[subblk].unpk_size;
					pDest = pBuffer + psubblk[subblk].unpk_pos;
				}
			}
		}
	}
	
	*ppMemFile = pBuffer;
	*pdwMemLength = dwFileSize;

	return true;
}
