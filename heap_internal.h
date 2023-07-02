//=================
// heap_internal.h
//=================

// Heap platform header

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap

#ifndef _HEAP_INTERNAL_H
#define _HEAP_INTERNAL_H


//=======
// Using
//=======

#include <assert.h>
#include <heap.h>
#include <stdbool.h>
#include <stdint.h>


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

inline size_t align_down(size_t value, size_t align)
{
return value&~(align-1);
}

inline size_t align_up(size_t value, size_t align)
{
return (value+align-1)&~(align-1);
}

#endif // _HEAP_INTERNAL_H
