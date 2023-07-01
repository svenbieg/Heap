//========
// heap.c
//========

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap

#include "pch.h"


//=======
// Using
//=======

#include <heap.h>
#include "heap_private.h"


//==================
// Con-/Destructors
//==================

heap_handle_t heap_create(size_t offset, size_t size)
{
size=align_down(size, sizeof(size_t));
assert(size>sizeof(heap_t));
heap_t* heap_ptr=(heap_t*)offset;
heap_ptr->free=size-sizeof(heap_t);
heap_ptr->used=sizeof(heap_t);
heap_ptr->size=size;
heap_ptr->free_block=0;
block_map_init(&heap_ptr->map_free);
return heap_ptr;
}


//============
// Allocation
//============

void* heap_alloc(heap_handle_t heap, size_t size)
{
assert(heap!=NULL);
assert(size!=0);
size=heap_block_calc_size(size);
void* buf=heap_alloc_from_map(heap, size);
if(buf)
	{
	heap_free_cache(heap);
	return buf;
	}
return heap_alloc_from_foot(heap, size);
}

void heap_free(heap_handle_t heap, void* buf)
{
assert(heap!=NULL);
if(!buf)
	return;
heap_free_to_map(heap, buf);
heap_free_cache(heap);
}


//==========
// Internal
//==========

void* heap_alloc_from_cache(heap_handle_t heap, size_t size)
{
heap_t* heap_ptr=(heap_t*)heap;
size_t free_block=heap_ptr->free_block;
size_t* prev_ptr=&heap_ptr->free_block;
while(free_block)
	{
	size_t* buf=heap_block_get_pointer(free_block);
	size_t next_free=*buf;
	heap_block_info_t info;
	heap_block_get_info(heap, buf, &info);
	if(info.size==size)
		{
		*prev_ptr=next_free;
		return buf;
		}
	if(info.size>=size+BLOCK_SIZE_MIN)
		{
		heap_block_info_t free_info;
		free_info.offset=free_block+size;
		free_info.size=info.size-size;
		free_info.free=false;
		size_t* free_body=(size_t*)heap_block_init(heap, &free_info);
		*free_body=next_free;
		*prev_ptr=free_info.offset;
		info.size=size;
		return heap_block_init(heap, &info);
		}
	prev_ptr=buf;
	free_block=next_free;
	}
return NULL;
}

void* heap_alloc_from_foot(heap_handle_t heap, size_t size)
{
heap_t* heap_ptr=(heap_t*)heap;
if(heap_ptr->used+size>heap_ptr->size)
	return NULL;
heap_block_info_t info;
info.offset=(size_t)heap+heap_ptr->used;
info.size=size;
info.free=false;
heap_ptr->free-=size;
heap_ptr->used+=size;
return heap_block_init(heap, &info);
}

void* heap_alloc_from_map(heap_handle_t heap, size_t size)
{
heap_t* heap_ptr=(heap_t*)heap;
heap_block_info_t info;
if(!block_map_get_block(heap, &heap_ptr->map_free, size, &info))
	return NULL;
heap_ptr->free-=info.size;
size_t free_size=info.size-size;
if(free_size>=BLOCK_SIZE_MIN)
	{
	heap_block_info_t free_info;
	free_info.offset=info.offset+size;
	free_info.size=free_size;
	free_info.free=false;
	void* free_buf=heap_block_init(heap, &free_info);
	heap_free_to_cache(heap, free_buf);
	info.size=size;
	}
info.free=false;
return heap_block_init(heap, &info);
}

void* heap_alloc_internal(heap_handle_t heap, size_t size)
{
size=heap_block_calc_size(size);
void* buf=heap_alloc_from_cache(heap, size);
if(buf)
	return buf;
buf=heap_alloc_from_map(heap, size);
if(buf)
	return buf;
return heap_alloc_from_foot(heap, size);
}

void heap_free_cache(heap_handle_t heap)
{
heap_t* heap_ptr=(heap_t*)heap;
size_t free_block=heap_ptr->free_block;
heap_ptr->free_block=0;
while(free_block)
	{
	size_t* buf=(size_t*)heap_block_get_pointer(free_block);
	free_block=*buf;
	heap_free_to_map(heap, buf);
	}
}

void heap_free_to_cache(heap_handle_t heap, void* buf)
{
heap_t* heap_ptr=(heap_t*)heap;
size_t* body_ptr=(size_t*)buf;
*body_ptr=heap_ptr->free_block;
heap_ptr->free_block=heap_block_get_offset(buf);
}

void heap_free_to_map(heap_handle_t heap, void* buf)
{
heap_t* heap_ptr=(heap_t*)heap;
heap_block_chain_t info;
heap_block_get_chain(heap, buf, &info);
size_t heap_end=(size_t)heap+heap_ptr->used;
size_t offset=info.current.offset;
size_t size=info.current.size;
assert(offset>=(size_t)heap);
assert(offset<heap_end);
assert(offset+size<=heap_end);
if(info.previous.free)
	{
	offset=info.previous.offset;
	size+=info.previous.size;
	block_map_remove_block(heap, &heap_ptr->map_free, &info.previous);
	heap_ptr->free-=info.previous.size;
	}
if(!info.next.offset)
	{
	heap_ptr->free+=size;
	heap_ptr->used-=size;
	return;
	}
if(info.next.free)
	{
	size+=info.next.size;
	block_map_remove_block(heap, &heap_ptr->map_free, &info.next);
	heap_ptr->free-=info.next.size;
	}
heap_ptr->free+=size;
info.current.offset=offset;
info.current.size=size;
info.current.free=true;
heap_block_init(heap, &info.current);
block_map_add_block(heap, &heap_ptr->map_free, &info.current);
}
