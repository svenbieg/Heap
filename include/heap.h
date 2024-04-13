//========
// heap.h
//========

// Memory-manager mapping free blocks by size and by offset.
// The smallest block top most of the heap is returned.

// Copyright 2024, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap


#ifndef _HEAP_H
#define _HEAP_H

#ifdef __cplusplus
extern "C" {
#endif


//=======
// Using
//=======

#include <stddef.h>


//======
// Heap
//======

typedef void* heap_handle_t;

// Con-/Destructors
heap_handle_t heap_create(size_t offset, size_t size);

// Allocation
void* heap_alloc(heap_handle_t heap, size_t size);
void* heap_alloc_aligned(heap_handle_t heap, size_t size, size_t align);
void heap_free(heap_handle_t heap, void* buffer);
//void* heap_realloc(heap_handle_t heap, void* buffer, size_t size); // Deprecated

// Common
size_t heap_available(heap_handle_t heap);

#ifdef __cplusplus
}
#endif

#endif // _HEAP_H
