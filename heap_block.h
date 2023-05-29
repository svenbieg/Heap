//==============
// heap_block.h
//==============

// Block of continuous memory on the heap.
// Size and flags are stored twice at the head and the foot.

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap


#ifndef _HEAP_BLOCK_H
#define _HEAP_BLOCK_H


//======
// Info
//======

typedef struct heap_block_info_t
{
size_t offset;
union
	{
	struct
		{
		size_t size: SIZE_BITS-1;
		size_t free: 1;
		};
	size_t header;
	};
}heap_block_info_t;

typedef struct
{
heap_block_info_t previous;
heap_block_info_t current;
heap_block_info_t next;
}heap_block_chain_t;


//==================
// Con-/Destructors
//==================

void* heap_block_init(heap_handle_t heap, heap_block_info_t const* info);


//========
// Common
//========

size_t heap_block_calc_size(size_t size);
size_t heap_block_get_offset(void* ptr);
void* heap_block_get_pointer(size_t offset);


//========
// Access
//========

void heap_block_get_chain(heap_handle_t heap, void* ptr, heap_block_chain_t* info);
void heap_block_get_info(heap_handle_t heap, void* ptr, heap_block_info_t* info);

#endif // _HEAP_BLOCK_H
