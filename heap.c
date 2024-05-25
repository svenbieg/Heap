//========
// heap.c
//========

// Memory-manager for real-time C++ applications
// Allocations and deletions are done in constant low time

// Copyright 2024, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/Heap


//=======
// Using
//=======

#include <algorithm>
#include "heap.h"


//======
// Heap
//======

// Con-/Destructors

heap_handle_t heap_create(size_t offset, size_t size)
{
offset=align_up(offset, sizeof(size_t));
size=align_down(size, sizeof(size_t));
assert(size>sizeof(heap_t));
heap_handle_t heap=(heap_t*)offset;
heap->free=size-sizeof(heap_t);
heap->used=sizeof(heap_t);
heap->size=size;
heap->free_block=0;
block_map_init((block_map_t*)&heap->map_free);
return heap;
}


// Allocation

void* heap_alloc(heap_handle_t heap, size_t size)
{
assert(heap!=NULL);
assert(size!=0);
size=heap_block_calc_size(size);
void* buf=heap_alloc_from_map(heap, size);
if(buf)
	{
	heap_free_cache(heap);
	}
else
	{
	buf=heap_alloc_from_foot(heap, size);
	}
return buf;
}

void heap_free(heap_handle_t heap, void* buf)
{
assert(heap!=NULL);
if(!buf)
	return;
heap_free_to_map(heap, buf);
heap_free_cache(heap);
}


// Internal Allocation

void* heap_alloc_from_foot(heap_handle_t heap, size_t size)
{
if(heap->used+size>heap->size)
	return NULL;
heap_block_info_t info;
info.offset=(size_t)heap+heap->used;
info.size=size;
info.free=false;
heap->free-=size;
heap->used+=size;
return heap_block_init(heap, &info);
}

void* heap_alloc_from_map(heap_handle_t heap, size_t size)
{
heap_block_info_t info;
if(!block_map_get_block(heap, (block_map_t*)&heap->map_free, size, &info))
	return NULL;
heap->free-=info.size;
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
size_t* body_ptr=(size_t*)buf;
*body_ptr=heap->free_block;
heap->free_block=heap_block_get_offset(buf);
}

void heap_free_to_map(heap_handle_t heap, void* buf)
{
heap_block_chain_t info;
heap_block_get_chain(heap, buf, &info);
size_t heap_end=(size_t)heap+heap->used;
size_t offset=info.current.offset;
size_t size=info.current.size;
assert(offset>=(size_t)heap);
assert(offset<heap_end);
assert(offset+size<=heap_end);
if(info.previous.free)
	{
	offset=info.previous.offset;
	size+=info.previous.size;
	block_map_remove_block(heap, (block_map_t*)&heap->map_free, &info.previous);
	heap->free-=info.previous.size;
	}
if(!info.next.offset)
	{
	heap->free+=size;
	heap->used-=size;
	return;
	}
if(info.next.free)
	{
	size+=info.next.size;
	block_map_remove_block(heap, (block_map_t*)&heap->map_free, &info.next);
	heap->free-=info.next.size;
	}
info.current.offset=offset;
info.current.size=size;
info.current.free=true;
heap_block_init(heap, &info.current);
bool added=block_map_add_block(heap, (block_map_t*)&heap->map_free, &info.current);
if(!added)
	{
	info.current.free=false;
	heap_block_init(heap, &info.current);
	buf=heap_block_get_pointer(info.current.offset);
	heap_free_to_cache(heap, buf);
	return;
	}
heap->free+=size;
}


// Statistic

size_t heap_available(heap_handle_t heap)
{
return heap->free;
}

size_t heap_get_largest_free_block(heap_handle_t heap)
{
size_t largest=block_map_get_last_size((block_map_t*)&heap->map_free);
size_t free=heap->size-heap->used;
return std::max(largest, free);
}


//============
// Heap-Block
//============

// Con-/Destructors

void* heap_block_init(heap_handle_t heap, heap_block_info_t const* info)
{
heap_t* heap_ptr=(heap_t*)heap;
assert(info->size%sizeof(size_t)==0);
assert(info->offset>=(size_t)heap+sizeof(heap_t));
assert(info->offset+info->size<=(size_t)heap+heap_ptr->size);
size_t* head_ptr=(size_t*)info->offset;
*head_ptr=info->header;
head_ptr++;
size_t* foot_ptr=(size_t*)(info->offset+info->size);
foot_ptr--;
*foot_ptr=info->header;
return head_ptr;
}


// Access

void heap_block_get_chain(heap_handle_t heap, void* ptr, heap_block_chain_t* info)
{
heap_t* heap_ptr=(heap_t*)heap;
size_t heap_offset=(size_t)heap;
size_t heap_start=heap_offset+sizeof(heap_t);
size_t offset=heap_block_get_offset(ptr);
size_t* head_ptr=(size_t*)offset;
info->current.offset=offset;
info->current.header=*head_ptr;
if(offset>heap_start)
	{
	size_t* foot_ptr=(size_t*)offset;
	foot_ptr--;
	info->previous.header=*foot_ptr;
	info->previous.offset=offset-info->previous.size;
	}
else
	{
	info->previous.header=0;
	info->previous.offset=0;
	}
size_t heap_end=heap_offset+heap_ptr->used;
size_t next_offset=offset+info->current.size;
if(next_offset<heap_end)
	{
	head_ptr=(size_t*)next_offset;
	info->next.offset=next_offset;
	info->next.header=*head_ptr;
	}
else
	{
	info->next.header=0;
	info->next.offset=0;
	}
}

void heap_block_get_info(heap_handle_t heap, void* ptr, heap_block_info_t* info)
{
heap_t* heap_ptr=(heap_t*)heap;
info->offset=heap_block_get_offset(ptr);
assert(info->offset>=(size_t)heap+sizeof(heap_t));
assert(info->offset<(size_t)heap+heap_ptr->used);
size_t* head_ptr=(size_t*)info->offset;
info->header=*head_ptr;
assert(info->size>=3*sizeof(size_t));
assert(info->offset+info->size<=(size_t)heap+heap_ptr->used);
assert(*((size_t*)(info->offset+info->size-sizeof(size_t)))==*head_ptr);
}


//===============
// Cluster-Group
//===============

// Access

size_t cluster_group_get_item_count(cluster_group_t* group)
{
if(!group)
	return 0;
cluster_group_t get;
get.value=group->value;
if(get.level==0)
	return get.child_count;
cluster_parent_group_t* cluster_parent_group=(cluster_parent_group_t*)group;
return cluster_parent_group->item_count;
}


//======================
// Cluster-Parent-Group
//======================

// Access

uint16_t cluster_parent_group_get_group(cluster_parent_group_t* group, size_t* pos)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t u=0; u<child_count; u++)
	{
	size_t item_count=cluster_group_get_item_count(group->children[u]);
	if(*pos<item_count)
		return u;
	*pos-=item_count;
	}
return CLUSTER_GROUP_SIZE;
}

int16_t cluster_parent_group_get_nearest_space(cluster_parent_group_t* group, int16_t pos)
{
int16_t child_count=(int16_t)cluster_group_get_child_count((cluster_group_t*)group);
int16_t before=pos-1;
int16_t after=pos+1;
while(before>=0||after<child_count)
	{
	if(before>=0)
		{
		uint16_t count=cluster_group_get_child_count(group->children[before]);
		if(count<CLUSTER_GROUP_SIZE)
			return before;
		before--;
		}
	if(after<child_count)
		{
		uint16_t count=cluster_group_get_child_count(group->children[after]);
		if(count<CLUSTER_GROUP_SIZE)
			return after;
		after++;
		}
	}
return -1;
}


// Modification

void cluster_parent_group_append_groups(cluster_parent_group_t* group, cluster_group_t* const* append, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(child_count+count<=CLUSTER_GROUP_SIZE);
for(uint16_t u=0; u<count; u++)
	{
	group->children[child_count+u]=append[u];
	group->item_count+=cluster_group_get_item_count(append[u]);
	}
cluster_group_set_child_count((cluster_group_t*)group, child_count+count);
}

void cluster_parent_group_cleanup(heap_handle_t heap, cluster_parent_group_t* group)
{
if(!cluster_group_is_dirty((cluster_group_t*)group))
	return;
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t pos=0; pos<child_count; )
	{
	uint16_t count=cluster_group_get_child_count((cluster_group_t*)group->children[pos]);
	if(count==0)
		{
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
		child_count--;
		continue;
		}
	pos++;
	}
cluster_group_set_dirty((cluster_group_t*)group, false);
}

void cluster_parent_group_insert_groups(cluster_parent_group_t* group, uint16_t at, cluster_group_t* const* insert, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(at<=child_count);
assert(child_count+count<=CLUSTER_GROUP_SIZE);
for(uint16_t u=child_count+count-1; u>=at+count; u--)
	group->children[u]=group->children[u-count];
for(uint16_t u=0; u<count; u++)
	{
	group->children[at+u]=insert[u];
	group->item_count+=cluster_group_get_item_count(insert[u]);
	}
cluster_group_set_child_count((cluster_group_t*)group, child_count+count);
}

void cluster_parent_group_remove_group(heap_handle_t heap, cluster_parent_group_t* group, uint16_t at)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(at<child_count);
cluster_group_t* child=group->children[at];
assert(cluster_group_get_child_count(child)==0);
assert(cluster_group_get_item_count(child)==0);
for(uint16_t u=at; u+1<child_count; u++)
	group->children[u]=group->children[u+1];
cluster_group_set_child_count((cluster_group_t*)group, child_count-1);
heap_free_to_cache(heap, child);
}

void cluster_parent_group_remove_groups(cluster_parent_group_t* group, uint16_t at, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(at+count<=child_count);
for(uint16_t u=0; u<count; u++)
	group->item_count-=cluster_group_get_item_count(group->children[at+u]);
for(uint16_t u=at; u+count<child_count; u++)
	group->children[u]=group->children[u+count];
cluster_group_set_child_count((cluster_group_t*)group, child_count-count);
}


//====================
// Offset-Index-Group
//====================

// Access

size_t offset_index_group_get_first_offset(offset_index_group_t* group)
{
if(group==NULL)
	return 0;
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_get_first_offset((offset_index_item_group_t*)group);
return ((offset_index_parent_group_t*)group)->first_offset;
}

size_t offset_index_group_get_last_offset(offset_index_group_t* group)
{
if(group==NULL)
	return 0;
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_get_last_offset((offset_index_item_group_t*)group);
return ((offset_index_parent_group_t*)group)->last_offset;
}


// Modification

bool offset_index_group_add_offset(heap_handle_t heap, offset_index_group_t* group, size_t offset, bool again)
{
cluster_group_set_locked((cluster_group_t*)group, true);
bool added=false;
if(cluster_group_get_level(group)==0)
	{
	added=offset_index_item_group_add_offset((offset_index_item_group_t*)group, offset);
	}
else
	{
	added=offset_index_parent_group_add_offset(heap, (offset_index_parent_group_t*)group, offset, again);
	}
cluster_group_set_locked((cluster_group_t*)group, false);
return added;
}

void offset_index_group_remove_offset(heap_handle_t heap, offset_index_group_t* group, size_t offset)
{
if(cluster_group_get_level(group)==0)
	{
	offset_index_item_group_remove_offset((offset_index_item_group_t*)group, offset);
	}
else
	{
	offset_index_parent_group_remove_offset(heap, (offset_index_parent_group_t*)group, offset);
	}
}

size_t offset_index_group_remove_offset_at(heap_handle_t heap, offset_index_group_t* group, size_t at)
{
bool passive=cluster_group_is_locked((cluster_group_t*)group);
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_remove_offset_at((offset_index_item_group_t*)group, at, passive);
return offset_index_parent_group_remove_offset_at(heap, (offset_index_parent_group_t*)group, at, passive);
}


//=========================
// Offset-Index-Item-Group
//=========================

// Con-/Destructors

offset_index_item_group_t* offset_index_item_group_create(heap_handle_t heap)
{
offset_index_item_group_t* group=(offset_index_item_group_t*)heap_alloc_internal(heap, sizeof(offset_index_item_group_t));
if(group==NULL)
	return NULL;
cluster_group_init((cluster_group_t*)group, 0, 0);
return group;
}


// Access

size_t offset_index_item_group_get_first_offset(offset_index_item_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	return 0;
return group->items[0];
}

uint16_t offset_index_item_group_get_item_pos(offset_index_item_group_t* group, size_t offset, bool* exists_ptr)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t pos=0; pos<child_count; pos++)
	{
	size_t item=group->items[pos];
	if(item==offset)
		{
		*exists_ptr=true;
		return pos;
		}
	if(item>offset)
		return pos;
	}
return child_count;
}

size_t offset_index_item_group_get_last_offset(offset_index_item_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	return 0;
return group->items[child_count-1];
}


// Modification

bool offset_index_item_group_add_offset(offset_index_item_group_t* group, size_t offset)
{
bool exists=false;
uint16_t pos=offset_index_item_group_get_item_pos(group, offset, &exists);
assert(!exists);
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
for(uint16_t u=child_count; u>pos; u--)
	group->items[u]=group->items[u-1];
group->items[pos]=offset;
cluster_group_set_child_count((cluster_group_t*)group, child_count+1);
return true;
}

void offset_index_item_group_append_items(offset_index_item_group_t* group, size_t const* append, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(child_count+count<=CLUSTER_GROUP_SIZE);
for(uint16_t u=0; u<count; u++)
	group->items[child_count+u]=append[u];
cluster_group_set_child_count((cluster_group_t*)group, child_count+count);
}

void offset_index_item_group_insert_items(offset_index_item_group_t* group, uint16_t at, size_t const* insert, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t u=child_count+count-1; u>=at+count; u--)
	group->items[u]=group->items[u-count];
for(uint16_t u=0; u<count; u++)
	group->items[at+u]=insert[u];
cluster_group_set_child_count((cluster_group_t*)group, child_count+count);
}

size_t offset_index_item_group_remove_item(offset_index_item_group_t* group, uint16_t at)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
size_t offset=group->items[at];
for(uint16_t u=at; u+1<child_count; u++)
	group->items[u]=group->items[u+1];
cluster_group_set_child_count((cluster_group_t*)group, child_count-1);
return offset;
}

void offset_index_item_group_remove_offset(offset_index_item_group_t* group, size_t offset)
{
bool exists=false;
uint16_t pos=offset_index_item_group_get_item_pos(group, offset, &exists);
assert(exists);
offset_index_item_group_remove_item(group, pos);
}

size_t offset_index_item_group_remove_offset_at(offset_index_item_group_t* group, size_t at, bool passive)
{
assert(!passive);
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(at<child_count);
return offset_index_item_group_remove_item(group, (uint16_t)at);
}

void offset_index_item_group_remove_items(offset_index_item_group_t* group, uint16_t at, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(at+count<=child_count);
for(uint16_t u=at; u+count<child_count; u++)
	group->items[u]=group->items[u+count];
cluster_group_set_child_count((cluster_group_t*)group, child_count-count);
}


//===========================
// Offset-Index-Parent-group
//===========================

// Con-/Destructors

offset_index_parent_group_t* offset_index_parent_group_create(heap_handle_t heap, uint16_t level)
{
offset_index_parent_group_t* group=(offset_index_parent_group_t*)heap_alloc_internal(heap, sizeof(offset_index_parent_group_t));
if(group==NULL)
	return NULL;
cluster_group_init((cluster_group_t*)group, level, 0);
group->first_offset=0;
group->last_offset=0;
group->item_count=0;
return group;
}

offset_index_parent_group_t* offset_index_parent_group_create_with_child(heap_handle_t heap, offset_index_group_t* child)
{
offset_index_parent_group_t* group=(offset_index_parent_group_t*)heap_alloc_internal(heap, sizeof(offset_index_parent_group_t));
if(group==NULL)
	return NULL;
uint16_t child_level=cluster_group_get_level(child);
cluster_group_init((cluster_group_t*)group, child_level+1, 1);
group->first_offset=offset_index_group_get_first_offset(child);
group->last_offset=offset_index_group_get_last_offset(child);
group->item_count=cluster_group_get_item_count((cluster_group_t*)child);
group->children[0]=child;
return group;
}


// Access

uint16_t offset_index_parent_group_get_item_pos(offset_index_parent_group_t* group, size_t offset, uint16_t* pos_ptr, bool must_exist)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
uint16_t pos=0;
for(; pos<child_count; pos++)
	{
	size_t first_offset=offset_index_group_get_first_offset(group->children[pos]);
	assert(offset!=0);
	if(offset<first_offset)
		break;
	size_t last_offset=offset_index_group_get_last_offset(group->children[pos]);
	if(offset>last_offset)
		continue;
	*pos_ptr=pos;
	return 1;
	}
if(must_exist)
	return 0;
if(pos==0)
	{
	*pos_ptr=pos;
	return 1;
	}
if(pos==child_count)
	{
	*pos_ptr=pos-1;
	return 1;
	}
*pos_ptr=pos-1;
return 2;
}


// Modification

bool offset_index_parent_group_add_offset(heap_handle_t heap, offset_index_parent_group_t* group, size_t offset, bool again)
{
if(!offset_index_parent_group_add_offset_internal(heap, group, offset, again))
	return false;
group->item_count++;
offset_index_parent_group_update_bounds(group);
cluster_parent_group_cleanup(heap, (cluster_parent_group_t*)group);
return true;
}

bool offset_index_parent_group_add_offset_internal(heap_handle_t heap, offset_index_parent_group_t* group, size_t offset, bool again)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(!child_count)
	return false;
uint16_t pos=0;
uint16_t count=offset_index_parent_group_get_item_pos(group, offset, &pos, false);
if(!again)
	{
	for(uint16_t u=0; u<count; u++)
		{
		if(offset_index_group_add_offset(heap, group->children[pos+u], offset, false))
			return true;
		}
	if(offset_index_parent_group_shift_children(group, pos, count))
		{
		count=offset_index_parent_group_get_item_pos(group, offset, &pos, false);
		for(uint16_t u=0; u<count; u++)
			{
			if(offset_index_group_add_offset(heap, group->children[pos+u], offset, false))
				return true;
			}
		}
	}
if(!offset_index_parent_group_split_child(heap, group, pos))
	return false;
count=offset_index_parent_group_get_item_pos(group, offset, &pos, false);
for(uint16_t u=0; u<count; u++)
	{
	if(offset_index_group_add_offset(heap, group->children[pos+u], offset, true))
		return true;
	}
return false;
}

void offset_index_parent_group_append_groups(offset_index_parent_group_t* group, offset_index_group_t* const* append, uint16_t count)
{
cluster_parent_group_append_groups((cluster_parent_group_t*)group, (cluster_group_t* const*)append, count);
offset_index_parent_group_update_bounds(group);
}

bool offset_index_parent_group_combine_child(heap_handle_t heap, offset_index_parent_group_t* group, uint16_t at)
{
uint16_t count=cluster_group_get_child_count((cluster_group_t*)group->children[at]);
if(count==0)
	{
	cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, at);
	return true;
	}
if(at>0)
	{
	uint16_t before=cluster_group_get_child_count((cluster_group_t*)group->children[at-1]);
	if(count+before<=CLUSTER_GROUP_SIZE)
		{
		offset_index_parent_group_move_children(group, at, at-1, count);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, at);
		return true;
		}
	}
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(at+1<child_count)
	{
	uint16_t after=cluster_group_get_child_count((cluster_group_t*)group->children[at+1]);
	if(count+after<=CLUSTER_GROUP_SIZE)
		{
		offset_index_parent_group_move_children(group, at+1, at, after);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, at+1);
		return true;
		}
	}
return false;
}

void offset_index_parent_group_insert_groups(offset_index_parent_group_t* group, uint16_t at, offset_index_group_t* const* insert, uint16_t count)
{
cluster_parent_group_insert_groups((cluster_parent_group_t*)group, at, (cluster_group_t* const*)insert, count);
offset_index_parent_group_update_bounds(group);
}

void offset_index_parent_group_move_children(offset_index_parent_group_t* group, uint16_t from, uint16_t to, uint16_t count)
{
uint16_t level=cluster_group_get_level((cluster_group_t*)group);
if(level>1)
	{
	offset_index_parent_group_t* src=(offset_index_parent_group_t*)group->children[from];
	offset_index_parent_group_t* dst=(offset_index_parent_group_t*)group->children[to];
	if(from>to)
		{
		offset_index_parent_group_append_groups(dst, src->children, count);
		offset_index_parent_group_remove_groups(src, 0, count);
		}
	else
		{
		uint16_t src_count=cluster_group_get_child_count((cluster_group_t*)src);
		offset_index_parent_group_insert_groups(dst, 0, &src->children[src_count-count], count);
		offset_index_parent_group_remove_groups(src, src_count-count, count);
		}
	}
else
	{
	offset_index_item_group_t* src=(offset_index_item_group_t*)group->children[from];
	offset_index_item_group_t* dst=(offset_index_item_group_t*)group->children[to];
	if(from>to)
		{
		offset_index_item_group_append_items(dst, src->items, count);
		offset_index_item_group_remove_items(src, 0, count);
		}
	else
		{
		uint16_t src_count=cluster_group_get_child_count((cluster_group_t*)src);
		offset_index_item_group_insert_items(dst, 0, &src->items[src_count-count], count);
		offset_index_item_group_remove_items(src, src_count-count, count);
		}
	}
}

void offset_index_parent_group_move_empty_slot(offset_index_parent_group_t* group, uint16_t from, uint16_t to)
{
if(from<to)
	{
	for(uint16_t u=from; u<to; u++)
		offset_index_parent_group_move_children(group, u+1, u, 1);
	}
else
	{
	for(uint16_t u=from; u>to; u--)
		offset_index_parent_group_move_children(group, u-1, u, 1);
	}
}

void offset_index_parent_group_remove_groups(offset_index_parent_group_t* group, uint16_t at, uint16_t count)
{
cluster_parent_group_remove_groups((cluster_parent_group_t*)group, at, count);
offset_index_parent_group_update_bounds(group);
}

void offset_index_parent_group_remove_offset(heap_handle_t heap, offset_index_parent_group_t* group, size_t offset)
{
uint16_t pos=0;
uint16_t count=offset_index_parent_group_get_item_pos(group, offset, &pos, true);
assert(count==1);
offset_index_group_remove_offset(heap, group->children[pos], offset);
offset_index_parent_group_combine_child(heap, group, pos);
group->item_count--;
offset_index_parent_group_update_bounds(group);
}

size_t offset_index_parent_group_remove_offset_at(heap_handle_t heap, offset_index_parent_group_t* group, size_t at, bool passive)
{
uint16_t pos=cluster_parent_group_get_group((cluster_parent_group_t*)group, &at);
assert(pos<CLUSTER_GROUP_SIZE);
size_t offset=offset_index_group_remove_offset_at(heap, group->children[pos], at);
if(passive)
	{
	cluster_group_set_dirty((cluster_group_t*)group, true);
	}
else
	{
	offset_index_parent_group_combine_child(heap, group, pos);
	cluster_parent_group_cleanup(heap, (cluster_parent_group_t*)group);
	}
group->item_count--;
offset_index_parent_group_update_bounds(group);
return offset;
}

bool offset_index_parent_group_shift_children(offset_index_parent_group_t* group, uint16_t at, uint16_t count)
{
int16_t space=cluster_parent_group_get_nearest_space((cluster_parent_group_t*)group, at);
if(space<0)
	return false;
if(count>1&&space>at)
	at++;
offset_index_parent_group_move_empty_slot(group, space, at);
return true;
}

bool offset_index_parent_group_split_child(heap_handle_t heap, offset_index_parent_group_t* group, uint16_t at)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
offset_index_group_t* child=NULL;
uint16_t level=cluster_group_get_level((cluster_group_t*)group);
if(level>1)
	{
	child=(offset_index_group_t*)offset_index_parent_group_create(heap, level-1);
	}
else
	{
	child=(offset_index_group_t*)offset_index_item_group_create(heap);
	}
if(!child)
	return false;
for(uint16_t u=child_count; u>at+1; u--)
	group->children[u]=group->children[u-1];
group->children[at+1]=child;
cluster_group_set_child_count((cluster_group_t*)group, child_count+1);
offset_index_parent_group_move_children(group, at, at+1, 1);
return true;
}

void offset_index_parent_group_update_bounds(offset_index_parent_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	{
	group->first_offset=0;
	group->last_offset=0;
	return;
	}
for(uint16_t pos=0; pos<child_count; pos++)
	{
	group->first_offset=offset_index_group_get_first_offset(group->children[pos]);
	if(group->first_offset!=0)
		break;
	}
for(uint16_t pos=child_count; pos>0; pos--)
	{
	group->last_offset=offset_index_group_get_last_offset(group->children[pos-1]);
	if(group->last_offset!=0)
		break;
	}
}


//==============
// Offset-Index
//==============

// Con-/Destructors

void offset_index_init(offset_index_t* index)
{
index->root=NULL;
}


// Modification

bool offset_index_add_offset(heap_handle_t heap, offset_index_t* index, size_t offset)
{
if(!index->root)
	{
	index->root=(offset_index_group_t*)offset_index_item_group_create(heap);
	if(!index->root)
		return false;
	}
if(offset_index_group_add_offset(heap, index->root, offset, false))
	return true;
if(!offset_index_lift_root(heap, index))
	return false;
return offset_index_group_add_offset(heap, index->root, offset, true);
}

void offset_index_drop_root(heap_handle_t heap, offset_index_t* index)
{
offset_index_group_t* root=index->root;
if(cluster_group_is_locked((cluster_group_t*)root))
	return;
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)root);
uint16_t level=cluster_group_get_level((cluster_group_t*)root);
if(level==0)
	{
	if(child_count==0)
		{
		index->root=NULL;
		heap_free_to_cache(heap, root);
		}
	return;
	}
if(child_count>1)
	return;
offset_index_parent_group_t* parent_group=(offset_index_parent_group_t*)root;
index->root=parent_group->children[0];
heap_free_to_cache(heap, root);
}

bool offset_index_lift_root(heap_handle_t heap, offset_index_t* index)
{
offset_index_parent_group_t* root=offset_index_parent_group_create_with_child(heap, index->root);
if(!root)
	return false;
index->root=(offset_index_group_t*)root;
return true;
}

void offset_index_remove_offset(heap_handle_t heap, offset_index_t* index, size_t offset)
{
offset_index_group_remove_offset(heap, index->root, offset);
offset_index_drop_root(heap, index);
}

size_t offset_index_remove_offset_at(heap_handle_t heap, offset_index_t* index, size_t at)
{
assert(index->root);
size_t offset=offset_index_group_remove_offset_at(heap, index->root, at);
offset_index_drop_root(heap, index);
return offset;
}


//=================
// Block-Map-Group
//=================

// Access

size_t block_map_group_get_first_size(block_map_group_t* group)
{
if(cluster_group_get_level(group)==0)
	return block_map_item_group_get_first_size((block_map_item_group_t*)group);
return ((block_map_parent_group_t*)group)->first_size;
}

block_map_item_t* block_map_group_get_item(block_map_group_t* group, size_t size)
{
if(cluster_group_get_level(group)==0)
	return block_map_item_group_get_item((block_map_item_group_t*)group, size);
return block_map_parent_group_get_item((block_map_parent_group_t*)group, size);
}

size_t block_map_group_get_last_size(block_map_group_t* group)
{
if(cluster_group_get_level(group)==0)
	return block_map_item_group_get_last_size((block_map_item_group_t*)group);
return ((block_map_parent_group_t*)group)->last_size;
}


// Modification

int16_t block_map_group_add_block(heap_handle_t heap, block_map_group_t* group, heap_block_info_t const* info, bool again)
{
cluster_group_set_locked((cluster_group_t*)group, true);
int16_t added=0;
if(cluster_group_get_level(group)==0)
	{
	added=block_map_item_group_add_block(heap, (block_map_item_group_t*)group, info);
	}
else
	{
	added=block_map_parent_group_add_block(heap, (block_map_parent_group_t*)group, info, again);
	}
cluster_group_set_locked((cluster_group_t*)group, false);
return added;
}

int16_t block_map_group_get_block(heap_handle_t heap, block_map_group_t* group, size_t min_size, heap_block_info_t* info)
{
bool passive=cluster_group_is_locked((cluster_group_t*)group);
if(cluster_group_get_level(group)==0)
	return block_map_item_group_get_block(heap, (block_map_item_group_t*)group, min_size, info, passive);
return block_map_parent_group_get_block(heap, (block_map_parent_group_t*)group, min_size, info, passive);
}

uint16_t block_map_group_remove_block(heap_handle_t heap, block_map_group_t* group, heap_block_info_t const* info)
{
if(cluster_group_get_level(group)==0)
	return block_map_item_group_remove_block(heap, (block_map_item_group_t*)group, info);
return block_map_parent_group_remove_block(heap, (block_map_parent_group_t*)group, info);
}


//======================
// Block-Map-Item-Group
//======================

// Con-/Destructors

block_map_item_group_t* block_map_item_group_create(heap_handle_t heap)
{
block_map_item_group_t* group=(block_map_item_group_t*)heap_alloc_internal(heap, sizeof(block_map_item_group_t));
if(group==NULL)
	return NULL;
cluster_group_init((cluster_group_t*)group, 0, 0);
return group;
}


// Access

size_t block_map_item_group_get_first_size(block_map_item_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	return 0;
return group->items[0].size;
}

block_map_item_t* block_map_item_group_get_item(block_map_item_group_t* group, size_t size)
{
bool exists=false;
int16_t pos=block_map_item_group_get_item_pos(group, size, &exists);
assert(exists);
return &group->items[pos];
}

block_map_item_t* block_map_item_group_get_item_at(block_map_item_group_t* group, uint16_t at)
{
assert(at<cluster_group_get_child_count((cluster_group_t*)group));
return &group->items[at];
}

uint16_t block_map_item_group_get_item_pos(block_map_item_group_t* group, size_t size, bool* exists_ptr)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t pos=0; pos<child_count; pos++)
	{
	block_map_item_t* item=&group->items[pos];
	if(item->entry==0)
		continue;
	if(item->size==size)
		{
		*exists_ptr=true;
		return pos;
		}
	if(item->size>size)
		return pos;
	}
return child_count;
}

size_t block_map_item_group_get_last_size(block_map_item_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	return 0;
return group->items[child_count-1].size;
}


// Modification

int16_t block_map_item_group_add_block(heap_handle_t heap, block_map_item_group_t* group, heap_block_info_t const* info)
{
bool exists=false;
uint16_t pos=block_map_item_group_get_item_pos(group, info->size, &exists);
if(!exists)
	{
	if(!block_map_item_group_add_item(group, info, pos))
		return -1;
	return 1;
	}
block_map_item_t* item=block_map_item_group_get_item_at(group, pos);
bool added=false;
if(item->single)
	{
	offset_index_t index;
	offset_index_init(&index);
	added=offset_index_add_offset(heap, &index, info->offset);
	if(!added)
		return -1;
	if(item->offset)
		offset_index_add_offset(heap, &index, item->offset);
	item->index=index;
	item->single=false;
	}
else
	{
	added=offset_index_add_offset(heap, &item->index, info->offset);
	}
block_map_item_group_cleanup(group);
return 0;
}

bool block_map_item_group_add_item(block_map_item_group_t* group, heap_block_info_t const* info, uint16_t at)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
for(uint16_t u=child_count; u>at; u--)
	group->items[u]=group->items[u-1];
group->items[at].size=info->size;
group->items[at].offset=info->offset;
group->items[at].single=true;
cluster_group_set_child_count((cluster_group_t*)group, child_count+1);
return true;
}

void block_map_item_group_append_items(block_map_item_group_t* group, block_map_item_t const* items, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t u=0; u<count; u++)
	group->items[child_count+u]=items[u];
cluster_group_set_child_count((cluster_group_t*)group, child_count+count);
}

void block_map_item_group_cleanup(block_map_item_group_t* group)
{
if(!cluster_group_is_dirty((cluster_group_t*)group))
	return;
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t pos=0; pos<child_count; )
	{
	if(group->items[pos].offset==0)
		{
		for(uint16_t u=pos; u+1<child_count; u++)
			group->items[u]=group->items[u+1];
		child_count--;
		}
	else
		{
		pos++;
		}
	}
cluster_group_set_child_count((cluster_group_t*)group, child_count);
cluster_group_set_dirty((cluster_group_t*)group, false);
}

int16_t block_map_item_group_get_block(heap_handle_t heap, block_map_item_group_t* group, size_t min_size, heap_block_info_t* info, bool passive)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
bool exists=false;
uint16_t pos=block_map_item_group_get_item_pos(group, min_size, &exists);
if(pos==child_count)
	return -1;
block_map_item_t* item=block_map_item_group_get_item_at(group, pos);
assert(item->offset!=0);
info->size=item->size;
if(item->single)
	{
	info->offset=item->offset;
	block_map_item_group_remove_item_at(group, pos, passive);
	return 1;
	}
info->offset=offset_index_remove_offset_at(heap, &item->index, 0);
if(!item->offset)
	{
	block_map_item_group_remove_item_at(group, pos, passive);
	return 1;
	}
return 0;
}

void block_map_item_group_insert_items(block_map_item_group_t* group, uint16_t pos, block_map_item_t const* items, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t u=child_count+count-1; u>=pos+count; u--)
	group->items[u]=group->items[u-count];
for(uint16_t u=0; u<count; u++)
	group->items[pos+u]=items[u];
cluster_group_set_child_count((cluster_group_t*)group, child_count+count);
}

uint16_t block_map_item_group_remove_block(heap_handle_t heap, block_map_item_group_t* group, heap_block_info_t const* info)
{
bool exists=false;
uint16_t pos=block_map_item_group_get_item_pos(group, info->size, &exists);
assert(exists);
block_map_item_t* item=block_map_item_group_get_item_at(group, pos);
if(item->single)
	{
	assert(item->offset==info->offset);
	block_map_item_group_remove_item_at(group, pos, false);
	return 1;
	}
offset_index_remove_offset(heap, &item->index, info->offset);
if(!item->offset)
	{
	block_map_item_group_remove_item_at(group, pos, false);
	return 1;
	}
return 0;
}

size_t block_map_item_group_remove_item_at(block_map_item_group_t* group, size_t at, bool passive)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
assert(at<child_count);
uint16_t pos=(uint16_t)at;
size_t offset=group->items[pos].offset;
if(passive)
	{
	group->items[pos].entry=0;
	cluster_group_set_dirty((cluster_group_t*)group, true);
	}
else
	{
	for(uint16_t u=pos; u+1<child_count; u++)
		group->items[u]=group->items[u+1];
	cluster_group_set_child_count((cluster_group_t*)group, child_count-1);
	}
return offset;
}

void block_map_item_group_remove_items(block_map_item_group_t* group, uint16_t pos, uint16_t count)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
for(uint16_t u=pos; u+count<child_count; u++)
	group->items[u]=group->items[u+count];
cluster_group_set_child_count((cluster_group_t*)group, child_count-count);
}


//========================
// Block-Map-Parent-Group
//========================

// Con-/Destructors

block_map_parent_group_t* block_map_parent_group_create(heap_handle_t heap, uint16_t level)
{
block_map_parent_group_t* group=(block_map_parent_group_t*)heap_alloc_internal(heap, sizeof(block_map_parent_group_t));
if(group==NULL)
	return NULL;
cluster_group_init((cluster_group_t*)group, level, 0);
group->first_size=0;
group->last_size=0;
group->item_count=0;
return group;
}

block_map_parent_group_t* block_map_parent_group_create_with_child(heap_handle_t heap, block_map_group_t* child)
{
block_map_parent_group_t* group=(block_map_parent_group_t*)heap_alloc_internal(heap, sizeof(block_map_parent_group_t));
if(group==NULL)
	return NULL;
uint16_t child_level=cluster_group_get_level((cluster_group_t*)child);
cluster_group_init((cluster_group_t*)group, child_level+1, 1);
group->first_size=block_map_group_get_first_size(child);
group->last_size=block_map_group_get_last_size(child);
group->item_count=cluster_group_get_item_count((cluster_group_t*)child);
group->children[0]=child;
return group;
}


// Access

block_map_item_t* block_map_parent_group_get_item(block_map_parent_group_t* group, size_t size)
{
uint16_t pos=0;
uint16_t count=block_map_parent_group_get_item_pos(group, size, &pos, true);
assert(count>0);
return block_map_group_get_item(group->children[pos], size);
}

uint16_t block_map_parent_group_get_item_pos(block_map_parent_group_t* group, size_t size, uint16_t* pos_ptr, bool must_exist)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
uint16_t pos=0;
for(; pos<child_count; pos++)
	{
	size_t first_size=block_map_group_get_first_size(group->children[pos]);
	if(size<first_size)
		break;
	size_t last_size=block_map_group_get_last_size(group->children[pos]);
	if(size>last_size)
		continue;
	*pos_ptr=pos;
	return 1;
	}
if(must_exist)
	return 0;
if(child_count==1)
	pos=0;
if(pos==0)
	{
	*pos_ptr=pos;
	return 1;
	}
if(pos==child_count)
	{
	pos--;
	*pos_ptr=pos;
	return 1;
	}
pos--;
*pos_ptr=pos;
return 2;
}


// Modification

int16_t block_map_parent_group_add_block(heap_handle_t heap, block_map_parent_group_t* group, heap_block_info_t const* info, bool again)
{
int16_t added=block_map_parent_group_add_block_internal(heap, group, info, again);
if(added<0)
	return -1;
group->item_count+=added;
block_map_parent_group_update_bounds(group);
cluster_parent_group_cleanup(heap, (cluster_parent_group_t*)group);
return added;
}

int16_t block_map_parent_group_add_block_internal(heap_handle_t heap, block_map_parent_group_t* group, heap_block_info_t const* info, bool again)
{
uint16_t pos=0;
uint16_t count=block_map_parent_group_get_item_pos(group, info->size, &pos, false);
if(!again)
	{
	for(uint16_t u=0; u<count; u++)
		{
		int16_t added=block_map_group_add_block(heap, group->children[pos+u], info, false);
		if(added>=0)
			return added;
		}
	if(block_map_parent_group_shift_children(group, pos, count))
		{
		count=block_map_parent_group_get_item_pos(group, info->size, &pos, false);
		for(uint16_t u=0; u<count; u++)
			{
			int16_t added=block_map_group_add_block(heap, group->children[pos+u], info, false);
			if(added>=0)
				return added;
			}
		}
	}
if(!block_map_parent_group_split_child(heap, group, pos))
	return -1;
count=block_map_parent_group_get_item_pos(group, info->size, &pos, false);
for(uint16_t u=0; u<count; u++)
	{
	int16_t added=block_map_group_add_block(heap, group->children[pos+u], info, true);
	if(added>=0)
		return added;
	}
return -1;
}

void block_map_parent_group_append_groups(block_map_parent_group_t* group, block_map_group_t* const* append, uint16_t count)
{
cluster_parent_group_append_groups((cluster_parent_group_t*)group, (cluster_group_t* const*)append, count);
block_map_parent_group_update_bounds(group);
}

bool block_map_parent_group_combine_child(heap_handle_t heap, block_map_parent_group_t* group, uint16_t pos)
{
uint16_t count=cluster_group_get_child_count(group->children[pos]);
if(count==0)
	{
	cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
	return true;
	}
if(pos>0)
	{
	uint16_t before=cluster_group_get_child_count(group->children[pos-1]);
	if(count+before<=CLUSTER_GROUP_SIZE)
		{
		block_map_parent_group_move_children(group, pos, pos-1, count);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
		return true;
		}
	}
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(pos+1<child_count)
	{
	uint16_t after=cluster_group_get_child_count(group->children[pos+1]);
	if(count+after<=CLUSTER_GROUP_SIZE)
		{
		block_map_parent_group_move_children(group, pos+1, pos, after);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos+1);
		return true;
		}
	}
return false;
}

int16_t block_map_parent_group_get_block(heap_handle_t heap, block_map_parent_group_t* group, size_t min_size, heap_block_info_t* info, bool passive)
{
uint16_t pos=0;
uint16_t count=block_map_parent_group_get_item_pos(group, min_size, &pos, false);
assert(count>0);
if(count==2)
	pos++;
int16_t removed=block_map_group_get_block(heap, group->children[pos], min_size, info);
if(removed<0)
	return -1;
if(passive)
	{
	cluster_group_set_dirty((cluster_group_t*)group, true);
	}
else
	{
	block_map_parent_group_combine_child(heap, group, pos);
	}
group->item_count-=removed;
block_map_parent_group_update_bounds(group);
return removed;
}

void block_map_parent_group_insert_groups(block_map_parent_group_t* group, uint16_t at, block_map_group_t* const* insert, uint16_t count)
{
cluster_parent_group_insert_groups((cluster_parent_group_t*)group, at, (cluster_group_t* const*)insert, count);
block_map_parent_group_update_bounds(group);
}

void block_map_parent_group_move_children(block_map_parent_group_t* group, uint16_t from, uint16_t to, uint16_t count)
{
uint16_t level=cluster_group_get_level((cluster_group_t*)group);
if(level>1)
	{
	block_map_parent_group_t* src=(block_map_parent_group_t*)group->children[from];
	block_map_parent_group_t* dst=(block_map_parent_group_t*)group->children[to];
	if(from>to)
		{
		block_map_parent_group_append_groups(dst, src->children, count);
		block_map_parent_group_remove_groups(src, 0, count);
		}
	else
		{
		uint16_t src_count=cluster_group_get_child_count((cluster_group_t*)src);
		block_map_parent_group_insert_groups(dst, 0, &src->children[src_count-count], count);
		block_map_parent_group_remove_groups(src, src_count-count, count);
		}
	}
else
	{
	block_map_item_group_t* src=(block_map_item_group_t*)group->children[from];
	block_map_item_group_t* dst=(block_map_item_group_t*)group->children[to];
	if(from>to)
		{
		block_map_item_group_append_items(dst, src->items, count);
		block_map_item_group_remove_items(src, 0, count);
		}
	else
		{
		uint16_t src_count=cluster_group_get_child_count((cluster_group_t*)src);
		block_map_item_group_insert_items(dst, 0, &src->items[src_count-count], count);
		block_map_item_group_remove_items(src, src_count-count, count);
		}
	}
}

void block_map_parent_group_move_empty_slot(block_map_parent_group_t* group, uint16_t from, uint16_t to)
{
if(from<to)
	{
	for(uint16_t u=from; u<to; u++)
		block_map_parent_group_move_children(group, u+1, u, 1);
	}
else
	{
	for(uint16_t u=from; u>to; u--)
		block_map_parent_group_move_children(group, u-1, u, 1);
	}
}

uint16_t block_map_parent_group_remove_block(heap_handle_t heap, block_map_parent_group_t* group, heap_block_info_t const* info)
{
uint16_t pos=0;
uint16_t count=block_map_parent_group_get_item_pos(group, info->size, &pos, true);
assert(count==1);
uint16_t removed=block_map_group_remove_block(heap, group->children[pos], info);
group->item_count-=removed;
block_map_parent_group_combine_child(heap, group, pos);
block_map_parent_group_update_bounds(group);
return removed;
}

void block_map_parent_group_remove_groups(block_map_parent_group_t* group, uint16_t at, uint16_t count)
{
cluster_parent_group_remove_groups((cluster_parent_group_t*)group, at, count);
block_map_parent_group_update_bounds(group);
}

bool block_map_parent_group_shift_children(block_map_parent_group_t* group, uint16_t at, uint16_t count)
{
int16_t space=cluster_parent_group_get_nearest_space((cluster_parent_group_t*)group, at);
if(space<0)
	return false;
if(count>1&&space>at)
	at++;
block_map_parent_group_move_empty_slot(group, space, at);
return true;
}

bool block_map_parent_group_split_child(heap_handle_t heap, block_map_parent_group_t* group, uint16_t at)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
block_map_group_t* child=NULL;
uint16_t level=cluster_group_get_level((cluster_group_t*)group);
if(level>1)
	{
	child=(block_map_group_t*)block_map_parent_group_create(heap, level-1);
	}
else
	{
	child=(block_map_group_t*)block_map_item_group_create(heap);
	}
if(!child)
	return false;
for(uint16_t u=child_count; u>at+1; u--)
	group->children[u]=group->children[u-1];
group->children[at+1]=child;
cluster_group_set_child_count((cluster_group_t*)group, child_count+1);
block_map_parent_group_move_children(group, at, at+1, 1);
return true;
}

void block_map_parent_group_update_bounds(block_map_parent_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	{
	group->first_size=0;
	group->last_size=0;
	return;
	}
for(uint16_t pos=0; pos<child_count; pos++)
	{
	group->first_size=block_map_group_get_first_size(group->children[pos]);
	if(group->first_size!=0)
		break;
	}
for(uint16_t pos=child_count; pos>0; pos--)
	{
	group->last_size=block_map_group_get_last_size(group->children[pos-1]);
	if(group->last_size!=0)
		break;
	}
}


//===========
// Block-Map
//===========

// Modification

bool block_map_add_block(heap_handle_t heap, block_map_t* map, heap_block_info_t const* info)
{
heap_t* heap_ptr=(heap_t*)heap;
assert(info->offset>=(size_t)heap+sizeof(heap_t));
assert(info->offset<(size_t)heap+heap_ptr->used);
if(!map->root)
	{
	map->root=(block_map_group_t*)block_map_item_group_create(heap);
	if(!map->root)
		return false;
	}
if(block_map_group_add_block(heap, map->root, info, false)>=0)
	{
	block_map_drop_root(heap, map);
	return true;
	}
if(!block_map_lift_root(heap, map))
	return false;
if(block_map_group_add_block(heap, map->root, info, true)<0)
	return false;
block_map_drop_root(heap, map);
return true;
}

bool block_map_drop_root(heap_handle_t heap, block_map_t* map)
{
block_map_group_t* root=map->root;
if(cluster_group_is_locked((cluster_group_t*)root))
	return false;
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)root);
uint16_t level=cluster_group_get_level((cluster_group_t*)root);
if(level==0)
	return false;
if(child_count>1)
	return false;
block_map_parent_group_t* parent_group=(block_map_parent_group_t*)root;
map->root=parent_group->children[0];
heap_free_to_cache(heap, root);
return true;
}

bool block_map_get_block(heap_handle_t heap, block_map_t* map, size_t min_size, heap_block_info_t* info)
{
if(!map->root)
	return false;
if(block_map_group_get_block(heap, map->root, min_size, info)>=0)
	{
	block_map_drop_root(heap, map);
	return true;
	}
return false;
}

bool block_map_lift_root(heap_handle_t heap, block_map_t* map)
{
block_map_parent_group_t* root=block_map_parent_group_create_with_child(heap, map->root);
if(!root)
	return false;
map->root=(block_map_group_t*)root;
return true;
}

void block_map_remove_block(heap_handle_t heap, block_map_t* map, heap_block_info_t const* info)
{
if(block_map_group_remove_block(heap, map->root, info))
	block_map_drop_root(heap, map);
}
