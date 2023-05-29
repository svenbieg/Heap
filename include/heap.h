//========
// heap.h
//========

// Heap manager sorting free blocks by size and offset.
// The smallest block top most of the heap is returned.

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap


#ifndef _HEAP_H
#define _HEAP_H


//======
// Heap
//======

typedef void* heap_handle_t;

// Con-/Destructors
heap_handle_t heap_create(size_t offset, size_t size);

// Allocation
void* heap_alloc(heap_handle_t heap, size_t size);
void heap_free(heap_handle_t heap, void* buffer);

#endif // _HEAP_H
