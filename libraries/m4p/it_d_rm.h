#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "it_structs.h"

enum
{
	FORMAT_UNKNOWN = 0,
	FORMAT_IT      = 1,
	FORMAT_S3M     = 2
};

// routines for handling data in RAM as a "FILE" type (IT2 doesn't have these)
typedef struct mem_t
{
	bool _eof;
	uint8_t *_ptr, *_base;
	uint32_t _cnt, _bufsiz;
} MEMFILE;

MEMFILE *mopen(const uint8_t *src, uint32_t length);
void mclose(MEMFILE **buf);
size_t mread(void *buffer, size_t size, size_t count, MEMFILE *buf);
size_t mtell(MEMFILE *buf);
int32_t meof(MEMFILE *buf);
void mseek(MEMFILE *buf, size_t offset, int32_t whence);
bool ReadBytes(MEMFILE *m, void *dst, uint32_t num);
// -------------------------------------------------------

bool Music_LoadFromData(uint8_t *Data, uint32_t DataLen);
void Music_FreeSong(void);