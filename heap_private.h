//================
// heap_private.h
//================

// Internal heap functions

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap

#ifndef _HEAP_PRIVATE_H
#define _HEAP_PRIVATE_H


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
