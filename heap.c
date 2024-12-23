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

#include "heap.h"


//======
// Heap
//======

void* heap_alloc(heap_t* heap, size_t size)
{
assert(heap!=NULL);
assert(size!=0);
void* buf=heap_alloc_internal(heap, size);
heap_free_cache(heap);
return buf;
}

size_t heap_available(heap_t* heap)
{
if(heap==NULL)
	return 0;
return heap->free;
}

heap_t* heap_create(size_t offset, size_t size)
{
offset=align_up(offset, sizeof(size_t));
size=align_down(size, sizeof(size_t));
assert(size>sizeof(heap_t));
heap_t* heap=(heap_t*)offset;
heap->free=size-sizeof(heap_t);
heap->used=sizeof(heap_t);
heap->size=size;
heap->free_block=0;
block_map_init((block_map_t*)&heap->map_free);
return heap;
}

void heap_free(heap_t* heap, void* buf)
{
assert(heap!=NULL);
if(!buf)
	return;
heap_free_to_map(heap, buf);
heap_free_cache(heap);
}

size_t heap_get_largest_free_block(heap_t* heap)
{
assert(heap!=NULL);
size_t free=heap->size-heap->used;
if(!heap->map_free)
	return free;
size_t largest=block_map_get_last_size((block_map_t*)&heap->map_free);
if(free>largest)
	largest=free;
return largest;
}

void heap_reserve(heap_t* heap, size_t offset, size_t size)
{
assert(heap!=NULL);
assert(size!=0);
offset-=sizeof(size_t);
size+=2*sizeof(size_t);
size_t heap_start=(size_t)heap;
size_t heap_used=heap_start+heap->used;
size_t heap_end=heap_start+heap->size;
assert(offset>heap_used);
assert(offset+size<=heap_end);
heap_block_info_t info;
info.offset=heap_used;
info.size=offset-heap_used;
info.free=true;
heap_block_init(heap, &info);
heap->free+=info.size;
heap->used+=info.size;
block_map_add_block(heap, (block_map_t*)&heap->map_free, &info);
info.offset=offset;
info.size=size;
info.free=false;
heap_block_init(heap, &info);
heap->used+=size;
}


//=====================
// Internal Allocation
//=====================

void* heap_alloc_from_cache(heap_t* heap, size_t size)
{
size_t* free_buf=NULL;
size_t* current_ptr=&heap->free_block;
while(*current_ptr)
	{
	size_t* buf=(size_t*)heap_block_get_pointer(*current_ptr);
	heap_block_info_t info;
	heap_block_get_info(heap, buf, &info);
	if(info.size<size)
		break;
	if(info.size==size)
		{
		*current_ptr=*buf;
		return heap_block_init(heap, &info);
		}
	if(free_buf==NULL)
		free_buf=buf;
	current_ptr=buf;
	}
if(!free_buf)
	return NULL;
heap_block_info_t info;
heap_block_get_info(heap, free_buf, &info);
size_t free_size=info.size-size;
if(free_size<BLOCK_SIZE_MIN)
	return NULL;
info.size-=size;
heap_block_init(heap, &info);
info.offset+=free_size;
info.size=size;
return heap_block_init(heap, &info);
}

void* heap_alloc_from_foot(heap_t* heap, size_t size)
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

void* heap_alloc_from_map(heap_t* heap, size_t size)
{
block_map_t* map=(block_map_t*)&heap->map_free;
if(!map->root)
	return NULL;
heap_block_info_t info;
if(!block_map_get_block(heap, map, size, &info))
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

void* heap_alloc_internal(heap_t* heap, size_t size)
{
size=heap_block_calc_size(size);
void* buf=heap_alloc_from_cache(heap, size);
if(!buf)
	buf=heap_alloc_from_map(heap, size);
if(!buf)
	buf=heap_alloc_from_foot(heap, size);
return buf;
}

void heap_free_cache(heap_t* heap)
{
if(!heap->free_block)
	return;
size_t* buf=(size_t*)heap_block_get_pointer(heap->free_block);
heap->free_block=*buf;
heap_free_to_map(heap, buf);
}

void heap_free_to_cache(heap_t* heap, void* buf)
{
size_t* free_ptr=(size_t*)buf;
heap_block_info_t free_info;
heap_block_get_info(heap, free_ptr, &free_info);
size_t* last_ptr=&heap->free_block;
size_t next=0;
while(*last_ptr)
	{
	size_t* next_ptr=(size_t*)heap_block_get_pointer(*last_ptr);
	heap_block_info_t next_info;
	heap_block_get_info(heap, next_ptr, &next_info);
	if(next_info.size<=free_info.size)
		{
		next=next_info.offset;
		break;
		}
	last_ptr=next_ptr;
	}
*last_ptr=free_info.offset;
*free_ptr=next;
}

void heap_free_to_map(heap_t* heap, void* buf)
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
	block_map_remove_block(heap, (block_map_t*)&heap->map_free, &info.previous);
	offset=info.previous.offset;
	size+=info.previous.size;
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
	block_map_remove_block(heap, (block_map_t*)&heap->map_free, &info.next);
	size+=info.next.size;
	heap->free-=info.next.size;
	}
info.current.offset=offset;
info.current.size=size;
info.current.free=false;
heap_block_init(heap, &info.current);
bool added=block_map_add_block(heap, (block_map_t*)&heap->map_free, &info.current);
if(added)
	{
	info.current.free=true;
	heap_block_init(heap, &info.current);
	heap->free+=size;
	return;
	}
buf=heap_block_get_pointer(info.current.offset);
heap_free_to_cache(heap, buf);
}


//============
// Heap-Block
//============

void heap_block_get_chain(heap_t* heap, void* ptr, heap_block_chain_t* info)
{
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
size_t heap_end=heap_offset+heap->used;
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

void heap_block_get_info(heap_t* heap, void* ptr, heap_block_info_t* info)
{
info->offset=heap_block_get_offset(ptr);
assert(info->offset>=(size_t)heap+sizeof(heap_t));
assert(info->offset<(size_t)heap+heap->used);
size_t* head_ptr=(size_t*)info->offset;
info->header=*head_ptr;
assert(info->size>=3*sizeof(size_t));
assert(info->offset+info->size<=(size_t)heap+heap->used);
assert(*((size_t*)(info->offset+info->size-sizeof(size_t)))==*head_ptr);
}

void* heap_block_init(heap_t* heap, heap_block_info_t const* info)
{
assert(info->size%sizeof(size_t)==0);
assert(info->offset>=(size_t)heap+sizeof(heap_t));
assert(info->offset+info->size<=(size_t)heap+heap->size);
size_t* head_ptr=(size_t*)info->offset;
*head_ptr=info->header;
head_ptr++;
size_t* foot_ptr=(size_t*)(info->offset+info->size);
foot_ptr--;
*foot_ptr=info->header;
return head_ptr;
}


//======================
// Cluster-Parent-Group
//======================

void cluster_parent_group_append_groups(cluster_parent_group_t* group, cluster_group_t* const* append, uint32_t count)
{
uint32_t child_count=group->header.child_count;
assert(child_count+count<=CLUSTER_GROUP_SIZE);
for(uint32_t u=0; u<count; u++)
	group->children[child_count+u]=append[u];
group->header.child_count+=count;
}

void cluster_parent_group_cleanup(heap_t* heap, cluster_parent_group_t* group)
{
if(!group->header.dirty)
	return;
uint32_t child_count=group->header.child_count;
for(uint32_t pos=0; pos<child_count; )
	{
	uint32_t count=group->children[pos]->child_count;
	if(count==0)
		{
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
		child_count--;
		continue;
		}
	pos++;
	}
group->header.dirty=false;
}

int16_t cluster_parent_group_get_nearest_space(cluster_parent_group_t* group, int16_t pos)
{
int16_t child_count=(int16_t)group->header.child_count;
int16_t before=pos-1;
int16_t after=pos+1;
while(before>=0||after<child_count)
	{
	if(before>=0)
		{
		uint32_t count=group->children[before]->child_count;
		if(count<CLUSTER_GROUP_SIZE)
			return before;
		before--;
		}
	if(after<child_count)
		{
		uint32_t count=group->children[after]->child_count;
		if(count<CLUSTER_GROUP_SIZE)
			return after;
		after++;
		}
	}
return -1;
}

void cluster_parent_group_insert_groups(cluster_parent_group_t* group, uint32_t pos, cluster_group_t* const* insert, uint32_t count)
{
uint32_t child_count=group->header.child_count;
assert(pos<=child_count);
assert(child_count+count<=CLUSTER_GROUP_SIZE);
for(uint32_t u=child_count+count-1; u>=pos+count; u--)
	group->children[u]=group->children[u-count];
for(uint32_t u=0; u<count; u++)
	group->children[pos+u]=insert[u];
group->header.child_count+=count;
}

void cluster_parent_group_remove_group(heap_t* heap, cluster_parent_group_t* group, uint32_t pos)
{
uint32_t child_count=group->header.child_count;
assert(pos<child_count);
cluster_group_t* child=group->children[pos];
assert(child->child_count==0);
for(uint32_t u=pos; u+1<child_count; u++)
	group->children[u]=group->children[u+1];
group->header.child_count--;
heap_free_to_cache(heap, child);
}

void cluster_parent_group_remove_groups(cluster_parent_group_t* group, uint32_t pos, uint32_t count)
{
uint32_t child_count=group->header.child_count;
assert(pos+count<=child_count);
for(uint32_t u=pos; u+count<child_count; u++)
	group->children[u]=group->children[u+count];
group->header.child_count-=count;;
}


//====================
// Offset-Index-Group
//====================

bool offset_index_group_add_offset(heap_t* heap, offset_index_group_t* group, size_t offset, bool again)
{
group->locked=true;
bool added=false;
if(group->level==0)
	{
	added=offset_index_item_group_add_offset((offset_index_item_group_t*)group, offset);
	}
else
	{
	added=offset_index_parent_group_add_offset(heap, (offset_index_parent_group_t*)group, offset, again);
	}
group->locked=false;
return added;
}

size_t offset_index_group_get_first_offset(offset_index_group_t* group)
{
if(group==NULL)
	return 0;
if(group->level==0)
	return offset_index_item_group_get_first_offset((offset_index_item_group_t*)group);
return ((offset_index_parent_group_t*)group)->first_offset;
}

size_t offset_index_group_get_last_offset(offset_index_group_t* group)
{
if(group==NULL)
	return 0;
if(group->level==0)
	return offset_index_item_group_get_last_offset((offset_index_item_group_t*)group);
return ((offset_index_parent_group_t*)group)->last_offset;
}

size_t offset_index_group_remove_last_offset(heap_t* heap, offset_index_group_t* group)
{
if(group->level==0)
	return offset_index_item_group_remove_last_offset((offset_index_item_group_t*)group);
return offset_index_parent_group_remove_last_offset(heap, (offset_index_parent_group_t*)group, group->locked);
}

void offset_index_group_remove_offset(heap_t* heap, offset_index_group_t* group, size_t offset)
{
if(group->level==0)
	{
	offset_index_item_group_remove_offset((offset_index_item_group_t*)group, offset);
	}
else
	{
	offset_index_parent_group_remove_offset(heap, (offset_index_parent_group_t*)group, offset);
	}
}


//=========================
// Offset-Index-Item-Group
//=========================

bool offset_index_item_group_add_offset(offset_index_item_group_t* group, size_t offset)
{
bool exists=false;
uint32_t pos=offset_index_item_group_get_item_pos(group, offset, &exists);
assert(!exists);
uint32_t child_count=group->header.child_count;
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
for(uint32_t u=child_count; u>pos; u--)
	group->items[u]=group->items[u-1];
group->items[pos]=offset;
group->header.child_count++;
return true;
}

void offset_index_item_group_append_items(offset_index_item_group_t* group, size_t const* append, uint32_t count)
{
uint32_t child_count=group->header.child_count;
assert(child_count+count<=CLUSTER_GROUP_SIZE);
for(uint32_t u=0; u<count; u++)
	group->items[child_count+u]=append[u];
group->header.child_count+=count;
}

offset_index_item_group_t* offset_index_item_group_create(heap_t* heap)
{
offset_index_item_group_t* group=(offset_index_item_group_t*)heap_alloc_internal(heap, sizeof(offset_index_item_group_t));
if(group==NULL)
	return NULL;
group->header.value=0;
return group;
}

size_t offset_index_item_group_get_first_offset(offset_index_item_group_t* group)
{
if(group->header.child_count==0)
	return 0;
return group->items[0];
}

uint32_t offset_index_item_group_get_item_pos(offset_index_item_group_t* group, size_t offset, bool* exists_ptr)
{
uint32_t child_count=group->header.child_count;
for(uint32_t pos=0; pos<child_count; pos++)
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
uint32_t child_count=group->header.child_count;
if(child_count==0)
	return 0;
return group->items[child_count-1];
}

void offset_index_item_group_insert_items(offset_index_item_group_t* group, uint32_t pos, size_t const* insert, uint32_t count)
{
uint32_t child_count=group->header.child_count;
for(uint32_t u=child_count+count-1; u>=pos+count; u--)
	group->items[u]=group->items[u-count];
for(uint32_t u=0; u<count; u++)
	group->items[pos+u]=insert[u];
group->header.child_count+=count;
}

size_t offset_index_item_group_remove_item(offset_index_item_group_t* group, uint32_t pos)
{
uint32_t child_count=group->header.child_count;
assert(pos<child_count);
size_t offset=group->items[pos];
for(uint32_t u=pos; u+1<child_count; u++)
	group->items[u]=group->items[u+1];
group->header.child_count--;
return offset;
}

void offset_index_item_group_remove_items(offset_index_item_group_t* group, uint32_t pos, uint32_t count)
{
uint32_t child_count=group->header.child_count;
assert(pos+count<=child_count);
for(uint32_t u=pos; u+count<child_count; u++)
	group->items[u]=group->items[u+count];
group->header.child_count-=count;;
}

size_t offset_index_item_group_remove_last_offset(offset_index_item_group_t* group)
{
uint32_t child_count=group->header.child_count;
assert(child_count>0);
return offset_index_item_group_remove_item(group, child_count-1);
}

void offset_index_item_group_remove_offset(offset_index_item_group_t* group, size_t offset)
{
bool exists=false;
uint32_t pos=offset_index_item_group_get_item_pos(group, offset, &exists);
assert(exists);
offset_index_item_group_remove_item(group, pos);
}


//===========================
// Offset-Index-Parent-Group
//===========================

bool offset_index_parent_group_add_offset(heap_t* heap, offset_index_parent_group_t* group, size_t offset, bool again)
{
bool added=offset_index_parent_group_add_offset_internal(heap, group, offset, again);
cluster_parent_group_cleanup(heap, (cluster_parent_group_t*)group);
if(added)
	offset_index_parent_group_update_bounds(group);
return added;
}

bool offset_index_parent_group_add_offset_internal(heap_t* heap, offset_index_parent_group_t* group, size_t offset, bool again)
{
uint32_t child_count=group->header.child_count;
if(!child_count)
	return false;
uint32_t pos=0;
uint32_t count=offset_index_parent_group_get_item_pos(group, offset, &pos, false);
if(!again)
	{
	for(uint32_t u=0; u<count; u++)
		{
		if(offset_index_group_add_offset(heap, group->children[pos+u], offset, false))
			return true;
		}
	if(offset_index_parent_group_shift_children(group, pos, count))
		{
		count=offset_index_parent_group_get_item_pos(group, offset, &pos, false);
		for(uint32_t u=0; u<count; u++)
			{
			if(offset_index_group_add_offset(heap, group->children[pos+u], offset, false))
				return true;
			}
		}
	}
if(!offset_index_parent_group_split_child(heap, group, pos))
	return false;
count=offset_index_parent_group_get_item_pos(group, offset, &pos, false);
for(uint32_t u=0; u<count; u++)
	{
	if(offset_index_group_add_offset(heap, group->children[pos+u], offset, true))
		return true;
	}
return false;
}

void offset_index_parent_group_append_groups(offset_index_parent_group_t* group, offset_index_group_t* const* append, uint32_t count)
{
cluster_parent_group_append_groups((cluster_parent_group_t*)group, (cluster_group_t* const*)append, count);
offset_index_parent_group_update_bounds(group);
}

bool offset_index_parent_group_combine_child(heap_t* heap, offset_index_parent_group_t* group, uint32_t pos)
{
uint32_t count=group->children[pos]->child_count;
if(count==0)
	{
	cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
	return true;
	}
if(pos>0)
	{
	uint32_t before=group->children[pos-1]->child_count;
	if(count+before<=CLUSTER_GROUP_SIZE)
		{
		offset_index_parent_group_move_children(group, pos, pos-1, count);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
		return true;
		}
	}
uint32_t child_count=group->header.child_count;
if(pos+1<child_count)
	{
	uint32_t after=group->children[pos+1]->child_count;
	if(count+after<=CLUSTER_GROUP_SIZE)
		{
		offset_index_parent_group_move_children(group, pos+1, pos, after);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos+1);
		return true;
		}
	}
return false;
}

offset_index_parent_group_t* offset_index_parent_group_create(heap_t* heap, uint32_t level)
{
offset_index_parent_group_t* group=(offset_index_parent_group_t*)heap_alloc_internal(heap, sizeof(offset_index_parent_group_t));
if(group==NULL)
	return NULL;
group->header.value=0;
group->header.level=level;
group->first_offset=0;
group->last_offset=0;
return group;
}

offset_index_parent_group_t* offset_index_parent_group_create_with_child(heap_t* heap, offset_index_group_t* child)
{
offset_index_parent_group_t* group=(offset_index_parent_group_t*)heap_alloc_internal(heap, sizeof(offset_index_parent_group_t));
if(group==NULL)
	return NULL;
group->header.value=0;
group->header.child_count=1;
group->header.level=child->level+1;
group->first_offset=offset_index_group_get_first_offset(child);
group->last_offset=offset_index_group_get_last_offset(child);
group->children[0]=child;
return group;
}

uint32_t offset_index_parent_group_get_item_pos(offset_index_parent_group_t* group, size_t offset, uint32_t* pos_ptr, bool must_exist)
{
uint32_t child_count=group->header.child_count;
uint32_t pos=0;
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

void offset_index_parent_group_insert_groups(offset_index_parent_group_t* group, uint32_t at, offset_index_group_t* const* insert, uint32_t count)
{
cluster_parent_group_insert_groups((cluster_parent_group_t*)group, at, (cluster_group_t* const*)insert, count);
offset_index_parent_group_update_bounds(group);
}

void offset_index_parent_group_move_children(offset_index_parent_group_t* group, uint32_t from, uint32_t to, uint32_t count)
{
uint32_t level=group->header.level;
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
		uint32_t src_count=src->header.child_count;
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
		uint32_t src_count=src->header.child_count;
		offset_index_item_group_insert_items(dst, 0, &src->items[src_count-count], count);
		offset_index_item_group_remove_items(src, src_count-count, count);
		}
	}
}

void offset_index_parent_group_move_empty_slot(offset_index_parent_group_t* group, uint32_t from, uint32_t to)
{
if(from<to)
	{
	for(uint32_t u=from; u<to; u++)
		offset_index_parent_group_move_children(group, u+1, u, 1);
	}
else
	{
	for(uint32_t u=from; u>to; u--)
		offset_index_parent_group_move_children(group, u-1, u, 1);
	}
}

void offset_index_parent_group_remove_groups(offset_index_parent_group_t* group, uint32_t at, uint32_t count)
{
cluster_parent_group_remove_groups((cluster_parent_group_t*)group, at, count);
offset_index_parent_group_update_bounds(group);
}

size_t offset_index_parent_group_remove_last_offset(heap_t* heap, offset_index_parent_group_t* group, bool passive)
{
uint32_t child_count=group->header.child_count;
assert(child_count>0);
size_t offset=offset_index_group_remove_last_offset(heap, group->children[child_count-1]);
if(passive)
	{
	group->header.dirty=true;
	}
else
	{
	offset_index_parent_group_combine_child(heap, group, child_count-1);
	}
offset_index_parent_group_update_bounds(group);
return offset;
}

void offset_index_parent_group_remove_offset(heap_t* heap, offset_index_parent_group_t* group, size_t offset)
{
uint32_t pos=0;
uint32_t count=offset_index_parent_group_get_item_pos(group, offset, &pos, true);
assert(count==1);
offset_index_group_remove_offset(heap, group->children[pos], offset);
offset_index_parent_group_combine_child(heap, group, pos);
offset_index_parent_group_update_bounds(group);
}

bool offset_index_parent_group_shift_children(offset_index_parent_group_t* group, uint32_t at, uint32_t count)
{
int16_t space=cluster_parent_group_get_nearest_space((cluster_parent_group_t*)group, at);
if(space<0)
	return false;
if(count>1&&space>at)
	at++;
offset_index_parent_group_move_empty_slot(group, space, at);
return true;
}

bool offset_index_parent_group_split_child(heap_t* heap, offset_index_parent_group_t* group, uint32_t at)
{
uint32_t child_count=group->header.child_count;
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
offset_index_group_t* child=NULL;
uint32_t level=group->header.level;
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
for(uint32_t u=child_count; u>at+1; u--)
	group->children[u]=group->children[u-1];
group->children[at+1]=child;
group->header.child_count++;
offset_index_parent_group_move_children(group, at, at+1, 1);
return true;
}

void offset_index_parent_group_update_bounds(offset_index_parent_group_t* group)
{
uint32_t child_count=group->header.child_count;
if(child_count==0)
	{
	group->first_offset=0;
	group->last_offset=0;
	return;
	}
for(uint32_t pos=0; pos<child_count; pos++)
	{
	group->first_offset=offset_index_group_get_first_offset(group->children[pos]);
	if(group->first_offset!=0)
		break;
	}
for(uint32_t pos=child_count; pos>0; pos--)
	{
	group->last_offset=offset_index_group_get_last_offset(group->children[pos-1]);
	if(group->last_offset!=0)
		break;
	}
}


//==============
// Offset-Index
//==============

bool offset_index_add_offset(heap_t* heap, offset_index_t* index, size_t offset)
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

size_t offset_index_drop_root(heap_t* heap, offset_index_t* index)
{
offset_index_group_t* root=index->root;
uint32_t child_count=root->child_count;
uint32_t level=root->level;
if(level==0)
	{
	size_t offset=0;
	if(child_count==1)
		{
		offset=offset_index_item_group_get_first_offset((offset_index_item_group_t*)root);
		child_count=0;
		}
	if(child_count==0)
		{
		index->root=NULL;
		heap_free_to_cache(heap, root);
		}
	return offset;
	}
if(child_count>1)
	return 0;
if(root->locked)
	return 0;
offset_index_parent_group_t* parent_group=(offset_index_parent_group_t*)root;
index->root=parent_group->children[0];
heap_free_to_cache(heap, root);
return 0;
}

bool offset_index_lift_root(heap_t* heap, offset_index_t* index)
{
offset_index_parent_group_t* root=offset_index_parent_group_create_with_child(heap, index->root);
if(!root)
	return false;
index->root=(offset_index_group_t*)root;
return true;
}


//=================
// Block-Map-Group
//=================

int16_t block_map_group_add_block(heap_t* heap, block_map_group_t* group, heap_block_info_t const* info, bool again)
{
group->locked=true;
int16_t added=0;
if(group->level==0)
	{
	added=block_map_item_group_add_block(heap, (block_map_item_group_t*)group, info);
	}
else
	{
	added=block_map_parent_group_add_block(heap, (block_map_parent_group_t*)group, info, again);
	}
group->locked=false;
return added;
}

bool block_map_group_get_block(heap_t* heap, block_map_group_t* group, size_t min_size, heap_block_info_t* info)
{
bool passive=group->locked;
if(group->level==0)
	return block_map_item_group_get_block(heap, (block_map_item_group_t*)group, min_size, info, passive);
return block_map_parent_group_get_block(heap, (block_map_parent_group_t*)group, min_size, info, passive);
}

size_t block_map_group_get_first_size(block_map_group_t* group)
{
if(group->level==0)
	return block_map_item_group_get_first_size((block_map_item_group_t*)group);
return ((block_map_parent_group_t*)group)->first_size;
}

size_t block_map_group_get_last_size(block_map_group_t* group)
{
if(group->level==0)
	return block_map_item_group_get_last_size((block_map_item_group_t*)group);
return ((block_map_parent_group_t*)group)->last_size;
}

void block_map_group_remove_block(heap_t* heap, block_map_group_t* group, heap_block_info_t const* info)
{
if(group->level==0)
	{
	block_map_item_group_remove_block(heap, (block_map_item_group_t*)group, info);
	}
else
	{
	block_map_parent_group_remove_block(heap, (block_map_parent_group_t*)group, info);
	}
}


//======================
// Block-Map-Item-Group
//======================

int16_t block_map_item_group_add_block(heap_t* heap, block_map_item_group_t* group, heap_block_info_t const* info)
{
bool exists=false;
uint32_t pos=block_map_item_group_get_item_pos(group, info->size, &exists);
if(!exists)
	{
	if(block_map_item_group_add_item(group, info, pos))
		return 1;
	return 0;
	}
block_map_item_t* item=&group->items[pos];
bool added=false;
if(item->single)
	{
	offset_index_t index={ NULL };
	bool added=offset_index_add_offset(heap, &index, info->offset);
	if(!added)
		return -1;
	if(item->offset)
		offset_index_add_offset(heap, &index, item->offset);
	item->index=index;
	item->single=false;
	}
else
	{
	if(!offset_index_add_offset(heap, &item->index, info->offset))
		return -1;
	}
block_map_item_group_cleanup(heap, group, info->size);
return 1;
}

bool block_map_item_group_add_item(block_map_item_group_t* group, heap_block_info_t const* info, uint32_t pos)
{
uint32_t child_count=group->header.child_count;
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
for(uint32_t u=child_count; u>pos; u--)
	group->items[u]=group->items[u-1];
group->items[pos].size=info->size;
group->items[pos].offset=info->offset;
group->items[pos].single=true;
group->header.child_count++;
return true;
}

void block_map_item_group_append_items(block_map_item_group_t* group, block_map_item_t const* items, uint32_t count)
{
uint32_t child_count=group->header.child_count;
for(uint32_t u=0; u<count; u++)
	group->items[child_count+u]=items[u];
group->header.child_count+=count;
}

void block_map_item_group_cleanup(heap_t* heap, block_map_item_group_t* group, size_t ignore)
{
if(!group->header.dirty)
	return;
uint32_t child_count=group->header.child_count;
for(uint32_t pos=0; pos<child_count; )
	{
	block_map_item_t* item=&group->items[pos];
	if(item->size==ignore)
		{
		pos++;
		continue;
		}
	if(item->offset&&!item->single)
		{
		size_t offset=offset_index_drop_root(heap, &item->index);
		if(offset)
			{
			item->offset=offset;
			item->single=true;
			}
		}
	if(!item->offset)
		{
		for(uint32_t u=pos; u+1<child_count; u++)
			group->items[u]=group->items[u+1];
		child_count--;
		continue;
		}
	pos++;
	}
group->header.child_count=child_count;
group->header.dirty=false;
}

block_map_item_group_t* block_map_item_group_create(heap_t* heap)
{
block_map_item_group_t* group=(block_map_item_group_t*)heap_alloc_internal(heap, sizeof(block_map_item_group_t));
if(group==NULL)
	return NULL;
group->header.value=0;
return group;
}

bool block_map_item_group_get_block(heap_t* heap, block_map_item_group_t* group, size_t min_size, heap_block_info_t* info, bool passive)
{
uint32_t child_count=group->header.child_count;
bool exists=false;
uint32_t pos=block_map_item_group_get_item_pos(group, min_size, &exists);
if(pos>=child_count)
	return false;
block_map_item_t* item=&group->items[pos];
assert(item->offset!=0);
info->size=item->size;
if(item->single)
	{
	info->offset=item->offset;
	block_map_item_group_remove_item_at(group, pos, passive);
	return true;
	}
info->offset=offset_index_group_remove_last_offset(heap, item->index.root);
size_t offset=offset_index_drop_root(heap, &item->index);
if(offset)
	{
	item->offset=offset;
	item->single=true;
	}
if(passive)
	{
	group->header.dirty=true;
	}
else
	{
	if(!item->offset)
		block_map_item_group_remove_item_at(group, pos, false);
	}
return true;
}

size_t block_map_item_group_get_first_size(block_map_item_group_t* group)
{
uint32_t child_count=group->header.child_count;
if(child_count==0)
	return 0;
return group->items[0].size;
}

uint32_t block_map_item_group_get_item_pos(block_map_item_group_t* group, size_t size, bool* exists_ptr)
{
uint32_t child_count=group->header.child_count;
for(uint32_t pos=0; pos<child_count; pos++)
	{
	block_map_item_t* item=&group->items[pos];
	if(item->offset==0)
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
uint32_t child_count=group->header.child_count;
if(child_count==0)
	return 0;
return group->items[child_count-1].size;
}

void block_map_item_group_insert_items(block_map_item_group_t* group, uint32_t pos, block_map_item_t const* items, uint32_t count)
{
uint32_t child_count=group->header.child_count;
for(uint32_t u=child_count+count-1; u>=pos+count; u--)
	group->items[u]=group->items[u-count];
for(uint32_t u=0; u<count; u++)
	group->items[pos+u]=items[u];
group->header.child_count+=count;
}

void block_map_item_group_remove_block(heap_t* heap, block_map_item_group_t* group, heap_block_info_t const* info)
{
bool exists=false;
uint32_t pos=block_map_item_group_get_item_pos(group, info->size, &exists);
assert(exists);
block_map_item_t* item=&group->items[pos];
if(item->single)
	{
	assert(item->offset==info->offset);
	block_map_item_group_remove_item_at(group, pos, false);
	return;
	}
assert(item->offset);
offset_index_group_remove_offset(heap, item->index.root, info->offset);
size_t offset=offset_index_drop_root(heap, &item->index);
if(offset)
	{
	item->offset=offset;
	item->single=true;
	}
if(!item->offset)
	block_map_item_group_remove_item_at(group, pos, false);
}

size_t block_map_item_group_remove_item_at(block_map_item_group_t* group, uint32_t pos, bool passive)
{
uint32_t child_count=group->header.child_count;
assert(pos<child_count);
block_map_item_t* item=&group->items[pos];
size_t offset=item->offset;
if(passive)
	{
	item->offset=0;
	item->single=true;
	group->header.dirty=true;
	}
else
	{
	for(uint32_t u=pos; u+1<child_count; u++)
		group->items[u]=group->items[u+1];
	group->header.child_count--;
	}
return offset;
}

void block_map_item_group_remove_items(block_map_item_group_t* group, uint32_t pos, uint32_t count)
{
uint32_t child_count=group->header.child_count;
for(uint32_t u=pos; u+count<child_count; u++)
	group->items[u]=group->items[u+count];
group->header.child_count-=count;;
}


//========================
// Block-Map-Parent-Group
//========================

int16_t block_map_parent_group_add_block(heap_t* heap, block_map_parent_group_t* group, heap_block_info_t const* info, bool again)
{
int16_t added=block_map_parent_group_add_block_internal(heap, group, info, again);
cluster_parent_group_cleanup(heap, (cluster_parent_group_t*)group);
if(added==1)
	block_map_parent_group_update_bounds(group);
return added;
}

int16_t block_map_parent_group_add_block_internal(heap_t* heap, block_map_parent_group_t* group, heap_block_info_t const* info, bool again)
{
uint32_t pos=0;
uint32_t count=block_map_parent_group_get_item_pos(group, info->size, &pos, false);
if(!again)
	{
	for(uint32_t u=0; u<count; u++)
		{
		int16_t added=block_map_group_add_block(heap, group->children[pos+u], info, false);
		if(added!=0)
			return added;
		}
	if(block_map_parent_group_shift_children(group, pos, count))
		{
		count=block_map_parent_group_get_item_pos(group, info->size, &pos, false);
		for(uint32_t u=0; u<count; u++)
			{
			int16_t added=block_map_group_add_block(heap, group->children[pos+u], info, false);
			if(added!=0)
				return added;
			}
		}
	}
if(!block_map_parent_group_split_child(heap, group, pos))
	return 0;
count=block_map_parent_group_get_item_pos(group, info->size, &pos, false);
for(uint32_t u=0; u<count; u++)
	{
	int16_t added=block_map_group_add_block(heap, group->children[pos+u], info, true);
	if(added!=0)
		return added;
	}
return -1;
}

void block_map_parent_group_append_groups(block_map_parent_group_t* group, block_map_group_t* const* append, uint32_t count)
{
cluster_parent_group_append_groups((cluster_parent_group_t*)group, (cluster_group_t* const*)append, count);
block_map_parent_group_update_bounds(group);
}

bool block_map_parent_group_combine_child(heap_t* heap, block_map_parent_group_t* group, uint32_t pos)
{
uint32_t count=group->children[pos]->child_count;
if(count==0)
	{
	cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
	return true;
	}
if(pos>0)
	{
	uint32_t before=group->children[pos-1]->child_count;
	if(count+before<=CLUSTER_GROUP_SIZE)
		{
		block_map_parent_group_move_children(group, pos, pos-1, count);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos);
		return true;
		}
	}
uint32_t child_count=group->header.child_count;
if(pos+1<child_count)
	{
	uint32_t after=group->children[pos+1]->child_count;
	if(count+after<=CLUSTER_GROUP_SIZE)
		{
		block_map_parent_group_move_children(group, pos+1, pos, after);
		cluster_parent_group_remove_group(heap, (cluster_parent_group_t*)group, pos+1);
		return true;
		}
	}
return false;
}

block_map_parent_group_t* block_map_parent_group_create(heap_t* heap, uint32_t level)
{
block_map_parent_group_t* group=(block_map_parent_group_t*)heap_alloc_internal(heap, sizeof(block_map_parent_group_t));
if(group==NULL)
	return NULL;
group->header.value=0;
group->header.level=level;
group->first_size=0;
group->last_size=0;
return group;
}

block_map_parent_group_t* block_map_parent_group_create_with_child(heap_t* heap, block_map_group_t* child)
{
block_map_parent_group_t* group=(block_map_parent_group_t*)heap_alloc_internal(heap, sizeof(block_map_parent_group_t));
if(group==NULL)
	return NULL;
group->header.value=0;
group->header.child_count=1;
group->header.level=child->level+1;
group->first_size=block_map_group_get_first_size(child);
group->last_size=block_map_group_get_last_size(child);
group->children[0]=child;
return group;
}

bool block_map_parent_group_get_block(heap_t* heap, block_map_parent_group_t* group, size_t min_size, heap_block_info_t* info, bool passive)
{
uint32_t pos=0;
uint32_t count=block_map_parent_group_get_item_pos(group, min_size, &pos, false);
assert(count>0);
if(count==2)
	pos++;
if(!block_map_group_get_block(heap, group->children[pos], min_size, info))
	return false;
if(passive)
	{
	group->header.dirty=true;
	}
else
	{
	block_map_parent_group_combine_child(heap, group, pos);
	}
block_map_parent_group_update_bounds(group);
return true;
}

uint32_t block_map_parent_group_get_item_pos(block_map_parent_group_t* group, size_t size, uint32_t* pos_ptr, bool must_exist)
{
uint32_t child_count=group->header.child_count;
uint32_t pos=0;
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

void block_map_parent_group_insert_groups(block_map_parent_group_t* group, uint32_t at, block_map_group_t* const* insert, uint32_t count)
{
cluster_parent_group_insert_groups((cluster_parent_group_t*)group, at, (cluster_group_t* const*)insert, count);
block_map_parent_group_update_bounds(group);
}

void block_map_parent_group_move_children(block_map_parent_group_t* group, uint32_t from, uint32_t to, uint32_t count)
{
uint32_t level=group->header.level;
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
		uint32_t src_count=src->header.child_count;
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
		uint32_t src_count=src->header.child_count;
		block_map_item_group_insert_items(dst, 0, &src->items[src_count-count], count);
		block_map_item_group_remove_items(src, src_count-count, count);
		}
	}
}

void block_map_parent_group_move_empty_slot(block_map_parent_group_t* group, uint32_t from, uint32_t to)
{
if(from<to)
	{
	for(uint32_t u=from; u<to; u++)
		block_map_parent_group_move_children(group, u+1, u, 1);
	}
else
	{
	for(uint32_t u=from; u>to; u--)
		block_map_parent_group_move_children(group, u-1, u, 1);
	}
}

void block_map_parent_group_remove_block(heap_t* heap, block_map_parent_group_t* group, heap_block_info_t const* info)
{
uint32_t pos=0;
uint32_t count=block_map_parent_group_get_item_pos(group, info->size, &pos, true);
assert(count==1);
block_map_group_remove_block(heap, group->children[pos], info);
block_map_parent_group_combine_child(heap, group, pos);
block_map_parent_group_update_bounds(group);
}

void block_map_parent_group_remove_groups(block_map_parent_group_t* group, uint32_t at, uint32_t count)
{
cluster_parent_group_remove_groups((cluster_parent_group_t*)group, at, count);
block_map_parent_group_update_bounds(group);
}

bool block_map_parent_group_shift_children(block_map_parent_group_t* group, uint32_t at, uint32_t count)
{
int16_t space=cluster_parent_group_get_nearest_space((cluster_parent_group_t*)group, at);
if(space<0)
	return false;
if(count>1&&space>at)
	at++;
block_map_parent_group_move_empty_slot(group, space, at);
return true;
}

bool block_map_parent_group_split_child(heap_t* heap, block_map_parent_group_t* group, uint32_t at)
{
uint32_t child_count=group->header.child_count;
if(child_count==CLUSTER_GROUP_SIZE)
	return false;
block_map_group_t* child=NULL;
uint32_t level=group->header.level;
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
for(uint32_t u=child_count; u>at+1; u--)
	group->children[u]=group->children[u-1];
group->children[at+1]=child;
group->header.child_count++;
block_map_parent_group_move_children(group, at, at+1, 1);
return true;
}

void block_map_parent_group_update_bounds(block_map_parent_group_t* group)
{
uint32_t child_count=group->header.child_count;
if(child_count==0)
	{
	group->first_size=0;
	group->last_size=0;
	return;
	}
for(uint32_t pos=0; pos<child_count; pos++)
	{
	group->first_size=block_map_group_get_first_size(group->children[pos]);
	if(group->first_size!=0)
		break;
	}
for(uint32_t pos=child_count; pos>0; pos--)
	{
	group->last_size=block_map_group_get_last_size(group->children[pos-1]);
	if(group->last_size!=0)
		break;
	}
}


//===========
// Block-Map
//===========

bool block_map_add_block(heap_t* heap, block_map_t* map, heap_block_info_t const* info)
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
int16_t added=block_map_group_add_block(heap, map->root, info, false);
if(added!=0)
	{
	block_map_drop_root(heap, map);
	return (added==1);
	}
if(!block_map_lift_root(heap, map))
	return false;
added=block_map_group_add_block(heap, map->root, info, true);
block_map_drop_root(heap, map);
return (added==1);
}

bool block_map_drop_root(heap_t* heap, block_map_t* map)
{
block_map_group_t* root=map->root;
if(root->locked)
	return false;
uint32_t level=root->level;
if(level==0)
	return false;
uint32_t child_count=root->child_count;
if(child_count>1)
	return false;
block_map_parent_group_t* parent_group=(block_map_parent_group_t*)root;
map->root=parent_group->children[0];
heap_free_to_cache(heap, root);
return true;
}

bool block_map_get_block(heap_t* heap, block_map_t* map, size_t min_size, heap_block_info_t* info)
{
block_map_group_t* root=map->root;
if(!root)
	return false;
if(!block_map_group_get_block(heap, root, min_size, info))
	return false;
if(!root->locked)
	block_map_drop_root(heap, map);
return true;
}

bool block_map_lift_root(heap_t* heap, block_map_t* map)
{
block_map_parent_group_t* root=block_map_parent_group_create_with_child(heap, map->root);
if(!root)
	return false;
map->root=(block_map_group_t*)root;
return true;
}

void block_map_remove_block(heap_t* heap, block_map_t* map, heap_block_info_t const* info)
{
block_map_group_remove_block(heap, map->root, info);
block_map_drop_root(heap, map);
}
