//========
// heap.h
//========

// Memory-manager for real-time C++ applications
// Allocations and deletions are done in constant low time

// Copyright 2024, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/Heap


#ifndef _HEAP_H
#define _HEAP_H


//=======
// Using
//=======

#include <stddef.h>


//==========
// Creation
//==========

typedef void* heap_handle_t;

heap_handle_t heap_create(size_t offset, size_t size);


//============
// Allocation
//============

void* heap_alloc(heap_handle_t heap, size_t size);
void heap_free(heap_handle_t heap, void* buffer);
void* heap_realloc(heap_handle_t heap, void* buffer, size_t size);


//============
// Statistics
//============

size_t heap_available(heap_handle_t heap);
size_t heap_get_largest_free_block(heap_handle_t heap);


#endif // _HEAP_H
