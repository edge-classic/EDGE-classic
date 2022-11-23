/*Copyright (c) 2015, Wojciech Adam Koszek <wojciech@koszek.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.*/

#ifndef _MINI_GZIP_H_
#define _MINI_GZIP_H_

#define MAX_PATH_LEN		1024
#define	MINI_GZ_MIN(a, b)	((a) < (b) ? (a) : (b))

struct mini_gzip {
	size_t		total_len;
	size_t		data_len;
	size_t		chunk_size;

	uint32_t	magic;
#define	MINI_GZIP_MAGIC	0xbeebb00b

	uint16_t	fcrc;
	uint16_t	fextra_len;

	uint8_t		*hdr_ptr;
	uint8_t		*fextra_ptr;
	uint8_t		*fname_ptr;
	uint8_t		*fcomment_ptr;

	uint8_t		*data_ptr;
	uint8_t		pad[3];
};

#ifdef __cplusplus
extern "C" {
#endif
/* mini_gzip.c */
extern int	mini_gz_start(struct mini_gzip *gz_ptr, void *mem, size_t mem_len);
extern void	mini_gz_chunksize_set(struct mini_gzip *gz_ptr, int chunk_size);
extern void	mini_gz_init(struct mini_gzip *gz_ptr);
extern int	mini_gz_unpack(struct mini_gzip *gz_ptr, void *mem_out, size_t mem_out_len);
#ifdef __cplusplus
}
#endif

#define	func_fprintf	fprintf
#define	func_fflush	fflush

#define	MINI_GZ_STREAM	stderr

#ifdef MINI_GZ_DEBUG
#define	GZAS(comp, ...)	do {						\
	if (!((comp))) {						\
		func_fprintf(MINI_GZ_STREAM, "Error: ");				\
		func_fprintf(MINI_GZ_STREAM, __VA_ARGS__);			\
		func_fprintf(MINI_GZ_STREAM, ", %s:%d\n", __func__, __LINE__);	\
		func_fflush(MINI_GZ_STREAM);					\
		exit(1);						\
	}								\
} while (0)

#define	GZDBG(...) do {					\
	func_fprintf(MINI_GZ_STREAM, "%s:%d ", __func__, __LINE__);	\
	func_fprintf(MINI_GZ_STREAM, __VA_ARGS__);			\
	func_fprintf(MINI_GZ_STREAM, "\n");				\
} while (0)
#else	/* MINI_GZ_DEBUG */
#define	GZAS(comp, ...)	
#define	GZDBG(...)
#endif

#endif
