//========
// heap.c
//========

// Copyright 2024, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/Heap


//=======
// Using
//=======

#include "block_map.h"
#include "heap_block.h"


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
block_map_init((block_map_t*)&heap_ptr->map_free);
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

void* heap_alloc_aligned(heap_handle_t heap, size_t size, size_t align)
{
assert(heap!=NULL);
assert(size!=0);
assert(align!=0);
assert(align%sizeof(size_t)==0);
size=heap_block_calc_size(size+align-sizeof(size_t));
void* buf=heap_alloc_from_map(heap, size);
if(buf)
	{
	heap_free_cache(heap);
	}
else
	{
	buf=heap_alloc_from_foot(heap, size);
	}
size_t addr=(size_t)buf;
addr=align_up(addr, align);
return (void*)addr;
}

void heap_free(heap_handle_t heap, void* buf)
{
assert(heap!=NULL);
if(!buf)
	return;
heap_free_to_map(heap, buf);
heap_free_cache(heap);
}

//void* heap_realloc(heap_handle_t heap, void* buf, size_t size) // Deprecated
//{
//assert(heap!=NULL);
//assert(size!=0);
//size_t block_size=heap_block_calc_size(size);
//heap_block_info_t info;
//heap_block_get_info(heap, buf, &info);
//if(!heap_realloc_inplace(heap, &info, block_size))
//	{
//	void* moved=heap_alloc(heap, size);
//	memcpy(moved, buf, info.size);
//	heap_free_to_map(heap, buf);
//	buf=moved;
//	}
//heap_free_cache(heap);
//return buf;
//}


//=====================
// Internal Allocation
//=====================

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
if(!block_map_get_block(heap, (block_map_t*)&heap_ptr->map_free, size, &info))
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
void* buf=heap_alloc_from_map(heap, size);
if(buf)
	return buf;
return heap_alloc_from_foot(heap, size);
}

void heap_free_cache(heap_handle_t heap)
{
heap_t* heap_ptr=(heap_t*)heap;
size_t free_block=heap_ptr->free_block;
if(!free_block)
	return;
size_t* buf=(size_t*)heap_block_get_pointer(free_block);
heap_ptr->free_block=*buf;
heap_free_to_map(heap, buf);
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
	block_map_remove_block(heap, (block_map_t*)&heap_ptr->map_free, &info.previous);
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
	block_map_remove_block(heap, (block_map_t*)&heap_ptr->map_free, &info.next);
	heap_ptr->free-=info.next.size;
	}
info.current.offset=offset;
info.current.size=size;
info.current.free=true;
heap_block_init(heap, &info.current);
if(!block_map_add_block(heap, (block_map_t*)&heap_ptr->map_free, &info.current))
	{
	info.current.free=false;
	heap_block_init(heap, &info.current);
	buf=heap_block_get_pointer(info.current.offset);
	heap_free_to_cache(heap, buf);
	return;
	}
heap_ptr->free+=size;
}

//bool heap_realloc_inplace(heap_handle_t heap, heap_block_info_t* info, size_t size) // Deprecated
//{
//heap_t* heap_ptr=(heap_t*)heap;
//if(info->size<size)
//	{
//	size_t next_offset=info->offset+info->size;
//	void* next_buf=heap_block_get_pointer(next_offset);
//	heap_block_info_t next_info;
//	heap_block_get_info(heap, next_buf, &next_info);
//	if(!next_info.free)
//		return false;
//	if(info->size+next_info.size<size)
//		return false;
//	block_map_remove_block(heap, &heap_ptr->map_free, &next_info);
//	heap_ptr->free-=next_info.size;
//	info->size+=next_info.size;
//	heap_block_init(heap, info);
//	}
//if(info->size>=size+BLOCK_SIZE_MIN)
//	{
//	heap_block_info_t free_info;
//	free_info.offset=info->offset+size;
//	free_info.size=info->size-size;
//	free_info.free=false;
//	void* free_buf=heap_block_init(heap, &free_info);
//	heap_free_to_cache(heap, free_buf);
//	info->size=size;
//	heap_block_init(heap, info);
//	}
//return true;
//}


//========
// Common
//========

size_t heap_available(heap_handle_t heap)
{
heap_t* heap_ptr=(heap_t*)heap;
return heap_ptr->free+(heap_ptr->size-heap_ptr->used);
}

size_t heap_get_largest_free_block(heap_handle_t heap)
{
heap_t* heap_ptr=(heap_t*)heap;
size_t largest=0;
largest=block_map_get_last_size((block_map_t*)&heap_ptr->map_free);
largest=max(largest, heap_ptr->size-heap_ptr->used);
return largest;
}
