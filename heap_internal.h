//=================
// heap_internal.h
//=================

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
#define BLOCK_SIZE_MIN (4*sizeof(size_t))

static inline size_t align_down(size_t value, size_t align)
{
return value&~(align-1);
}

static inline size_t align_up(size_t value, size_t align)
{
return value+(align-value%align)%align;
}


//======
// Heap
//======

typedef struct
{
size_t free;
size_t used;
size_t size;
size_t free_block;
size_t map_free;
}heap_t;

void* heap_alloc_from_foot(heap_handle_t heap, size_t size);
void* heap_alloc_from_map(heap_handle_t heap, size_t size);
void* heap_alloc_internal(heap_handle_t heap, size_t size);
void heap_free_cache(heap_handle_t heap);
void heap_free_to_cache(heap_handle_t heap, void* buf);
void heap_free_to_map(heap_handle_t heap, void* buf);
//bool heap_realloc_inplace(heap_handle_t heap, heap_block_info_t* info, size_t size); // Deprecated

#endif // _HEAP_INTERNAL_H
