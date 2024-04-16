//=================
// heap_internal.h
//=================

// Heap platform header

// Copyright 2024, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/Heap

#ifndef _HEAP_INTERNAL_H
#define _HEAP_INTERNAL_H


//=======
// Using
//=======

#include <assert.h>
#include <heap.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


//==========
// Settings
//==========

#define CLUSTER_GROUP_SIZE 10


//===========
// Alignment
//===========

#define SIZE_BITS (sizeof(size_t)*8)
#define SIZE_BYTES sizeof(size_t)

#define BLOCK_SIZE_MIN (3*SIZE_BYTES)

static inline size_t align_down(size_t value, size_t align)
{
return value&~(align-1);
}

static inline size_t align_up(size_t value, size_t align)
{
return value+(align-value%align)%align;
}

#endif // _HEAP_INTERNAL_H
