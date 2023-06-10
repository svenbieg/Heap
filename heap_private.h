//================
// heap_private.h
//================

// Internal heap functions

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap

#ifndef _HEAP_PRIVATE_H
#define _HEAP_PRIVATE_H


//==========
// Settings
//==========

#define CLUSTER_GROUP_SIZE 10


//===========
// Alignment
//===========

#if(SIZE_MAX==0xFFFFFFFF)
	#define SIZE_BITS 32
	#define SIZE_BYTES 4
#else
	#define SIZE_BITS 64
	#define SIZE_BYTES 8
#endif

#define BLOCK_SIZE_MIN (3*SIZE_BYTES)

inline size_t align_down(size_t value, size_t align)
{
return value&~(align-1);
}

inline size_t align_up(size_t value, size_t align)
{
return (value+align-1)&~(align-1);
}


//======================
// Forward-Declarations
//======================

#include "cluster_group.h"
#include "heap_block.h"
#include "parent_group.h"


//=======
// Using
//=======

#include "block_map.h"
#include "offset_index.h"


//======
// Heap
//======

typedef struct
{
size_t free;
size_t used;
size_t size;
size_t free_block;
block_map_t map_free;
}heap_t;

void* heap_alloc_from_cache(heap_handle_t heap, size_t size);
void* heap_alloc_from_foot(heap_handle_t heap, size_t size);
void* heap_alloc_from_map(heap_handle_t heap, size_t size);
void* heap_alloc_internal(heap_handle_t heap, size_t size);
void heap_free_cache(heap_handle_t heap);
void heap_free_to_cache(heap_handle_t heap, void* buf);
void heap_free_to_map(heap_handle_t heap, void* buf);

#endif // _HEAP_PRIVATE_H
