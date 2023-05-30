//================
// offset_index.c
//================

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap

#include "pch.h"


//=======
// Using
//=======

#include <heap.h>
#include "heap_private.h"


//=======
// Group
//=======


// Access

size_t* offset_index_group_get_first_offset(offset_index_group_t* group)
{
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_get_first_offset((offset_index_item_group_t*)group);
return ((offset_index_parent_group_t*)group)->first;
}

size_t* offset_index_group_get_last_offset(offset_index_group_t* group)
{
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_get_last_offset((offset_index_item_group_t*)group);
return ((offset_index_parent_group_t*)group)->last;
}


// Modification

bool offset_index_group_add_offset(heap_handle_t heap, offset_index_group_t* group, size_t offset, bool again)
{
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_add_offset((offset_index_item_group_t*)group, offset);
return offset_index_parent_group_add_offset(heap, (offset_index_parent_group_t*)group, offset, again);
}

bool offset_index_group_remove_offset(heap_handle_t heap, offset_index_group_t* group, size_t offset)
{
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_remove_offset((offset_index_item_group_t*)group, offset);
return offset_index_parent_group_remove_offset(heap, (offset_index_parent_group_t*)group, offset);
}

size_t offset_index_group_remove_offset_at(heap_handle_t heap, offset_index_group_t* group, size_t at)
{
if(cluster_group_get_level(group)==0)
	return offset_index_item_group_remove_offset_at((offset_index_item_group_t*)group, at);
return offset_index_parent_group_remove_offset_at(heap, (offset_index_parent_group_t*)group, at);
}


//============
// Item-Group
//============


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

size_t* offset_index_item_group_get_first_offset(offset_index_item_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	return NULL;
return group->items;
}

uint16_t offset_index_item_group_get_item_pos(offset_index_item_group_t* group, size_t offset, bool* exists_ptr)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
uint16_t start=0;
uint16_t end=child_count;
while(start<end)
	{
	uint16_t pos=start+(end-start)/2;
	size_t item=group->items[pos];
	if(item>offset)
		{
		end=pos;
		continue;
		}
	if(item<offset)
		{
		start=pos+1;
		continue;
		}
	*exists_ptr=true;
	return pos;
	}
return start;
}

size_t* offset_index_item_group_get_last_offset(offset_index_item_group_t* group)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
if(child_count==0)
	return NULL;
return &group->items[child_count-1];
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

bool offset_index_item_group_remove_offset(offset_index_item_group_t* group, size_t offset)
{
bool exists=false;
uint16_t pos=offset_index_item_group_get_item_pos(group, offset, &exists);
assert(exists);
if(!exists)
	return false;
offset_index_item_group_remove_item(group, pos);
return true;
}

size_t offset_index_item_group_remove_offset_at(offset_index_item_group_t* group, size_t at)
{
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


//==============
// Parent-group
//==============


// Con-/Destructors

offset_index_parent_group_t* offset_index_parent_group_create(heap_handle_t heap, uint16_t level)
{
offset_index_parent_group_t* group=(offset_index_parent_group_t*)heap_alloc_internal(heap, sizeof(offset_index_parent_group_t));
if(group==NULL)
	return NULL;
cluster_group_init((cluster_group_t*)group, level, 0);
group->first=NULL;
group->last=NULL;
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
group->first=offset_index_group_get_first_offset(child);
group->last=offset_index_group_get_last_offset(child);
group->item_count=cluster_group_get_item_count((cluster_group_t*)child);
group->children[0]=child;
return group;
}


// Access

uint16_t offset_index_parent_group_get_item_pos(offset_index_parent_group_t* group, size_t offset, uint16_t* pos_ptr, bool must_exist)
{
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)group);
uint16_t start=0;
uint16_t end=child_count;
size_t* first_ptr=NULL;
size_t* last_ptr=NULL;
int16_t empty=0;
while(start<end)
	{
	uint16_t pos=start+(end-start)/2+empty;
	first_ptr=offset_index_group_get_first_offset(group->children[pos]);
	if(first_ptr==NULL)
		{
		if(empty<0)
			{
			empty--;
			if((end-start)/2+empty<start)
				break;
			continue;
			}
		empty++;
		if((end-start)/2+empty>=end)
			{
			empty=-1;
			if((end-start)/2+empty<start)
				break;
			}
		continue;
		}
	empty=0;
	if(*first_ptr>offset)
		{
		end=pos;
		continue;
		}
	last_ptr=offset_index_group_get_last_offset(group->children[pos]);
	if(*last_ptr<offset)
		{
		start=pos+1;
		continue;
		}
	*pos_ptr=pos;
	return 1;
	}
if(must_exist)
	return 0;
if(child_count==0)
	{
	*pos_ptr=0;
	return 1;
	}
if(start>=child_count)
	start=(uint16_t)(child_count-1);
*pos_ptr=start;
if(start>0)
	{
	first_ptr=offset_index_group_get_first_offset(group->children[start]);
	if(first_ptr==NULL||*first_ptr>offset)
		{
		*pos_ptr=(uint16_t)(start-1);
		return 2;
		}
	}
if(start+1<child_count)
	{
	last_ptr=offset_index_group_get_last_offset(group->children[start]);
	if(last_ptr==NULL||*last_ptr<offset)
		return 2;
	}
return 1;
}


// Modification

bool offset_index_parent_group_add_offset(heap_handle_t heap, offset_index_parent_group_t* group, size_t offset, bool again)
{
bool added=offset_index_parent_group_add_offset_internal(heap, group, offset, again);
if(added)
	{
	group->item_count++;
	offset_index_parent_group_update_bounds(group);
	}
return added;
}

bool offset_index_parent_group_add_offset_internal(heap_handle_t heap, offset_index_parent_group_t* group, size_t offset, bool again)
{
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
parent_group_append_groups((parent_group_t*)group, (cluster_group_t* const*)append, count);
offset_index_parent_group_update_bounds(group);
}

bool offset_index_parent_group_combine_child(heap_handle_t heap, offset_index_parent_group_t* group, uint16_t at)
{
uint16_t count=cluster_group_get_child_count((cluster_group_t*)group->children[at]);
if(count==0)
	{
	parent_group_remove_group(heap, (parent_group_t*)group, at);
	return true;
	}
if(at>0)
	{
	uint16_t before=cluster_group_get_child_count((cluster_group_t*)group->children[at-1]);
	if(count+before<=CLUSTER_GROUP_SIZE)
		{
		offset_index_parent_group_move_children(group, at, at-1, count);
		parent_group_remove_group(heap, (parent_group_t*)group, at);
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
		parent_group_remove_group(heap, (parent_group_t*)group, at+1);
		return true;
		}
	}
return false;
}

void offset_index_parent_group_insert_groups(offset_index_parent_group_t* group, uint16_t at, offset_index_group_t* const* insert, uint16_t count)
{
parent_group_insert_groups((parent_group_t*)group, at, (cluster_group_t* const*)insert, count);
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
parent_group_remove_groups((parent_group_t*)group, at, count);
offset_index_parent_group_update_bounds(group);
}

bool offset_index_parent_group_remove_offset(heap_handle_t heap, offset_index_parent_group_t* group, size_t offset)
{
uint16_t pos=0;
uint16_t count=offset_index_parent_group_get_item_pos(group, offset, &pos, true);
assert(count==1);
if(count!=1)
	return false;
if(offset_index_group_remove_offset(heap, group->children[pos], offset))
	{
	group->item_count--;
	offset_index_parent_group_combine_child(heap, group, pos);
	offset_index_parent_group_update_bounds(group);
	return true;
	}
return false;
}

size_t offset_index_parent_group_remove_offset_at(heap_handle_t heap, offset_index_parent_group_t* group, size_t at)
{
uint16_t pos=parent_group_get_group((parent_group_t*)group, &at);
assert(pos<CLUSTER_GROUP_SIZE);
size_t offset=offset_index_group_remove_offset_at(heap, group->children[pos], at);
group->item_count--;
offset_index_parent_group_combine_child(heap, group, pos);
offset_index_parent_group_update_bounds(group);
return offset;
}

bool offset_index_parent_group_shift_children(offset_index_parent_group_t* group, uint16_t at, uint16_t count)
{
int16_t space=parent_group_get_nearest_space((parent_group_t*)group, at);
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
	group->first=NULL;
	group->last=NULL;
	return;
	}
for(uint16_t pos=0; pos<child_count; pos++)
	{
	group->first=offset_index_group_get_first_offset(group->children[pos]);
	if(group->first!=NULL)
		break;
	}
for(uint16_t pos=child_count; pos>0; pos--)
	{
	group->last=offset_index_group_get_last_offset(group->children[pos-1]);
	if(group->last!=NULL)
		break;
	}
}


//=======
// Index
//=======

// Con-/Destructors

void offset_index_init(offset_index_t* index)
{
index->root=NULL;
}

void offset_index_open(offset_index_t* index, size_t offset)
{
index->root=(cluster_group_t*)offset;
}


// Access

size_t offset_index_get_offset_count(offset_index_t* index)
{
if(!index->root)
	return 0;
return cluster_group_get_item_count((cluster_group_t*)index->root);
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
return offset_index_parent_group_add_offset(heap, (offset_index_parent_group_t*)index->root, offset, true);
}

void offset_index_drop_root(heap_handle_t heap, offset_index_t* index)
{
offset_index_group_t* root=index->root;
uint16_t child_count=cluster_group_get_child_count((cluster_group_t*)root);
uint16_t level=cluster_group_get_level((cluster_group_t*)root);
if(level==0)
	{
	if(child_count==0)
		{
		index->root=NULL;
		heap_free_internal(heap, root);
		}
	return;
	}
if(child_count>1)
	return;
offset_index_parent_group_t* parent_group=(offset_index_parent_group_t*)root;
index->root=parent_group->children[0];
heap_free_internal(heap, root);
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
if(offset_index_group_remove_offset(heap, index->root, offset))
	offset_index_drop_root(heap, index);
}

size_t offset_index_remove_offset_at(heap_handle_t heap, offset_index_t* index, size_t at)
{
size_t offset=offset_index_group_remove_offset_at(heap, index->root, at);
offset_index_drop_root(heap, index);
return offset;
}