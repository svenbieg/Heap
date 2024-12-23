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

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


//==========
// Settings
//==========

#define CLUSTER_GROUP_SIZE 10


//===========
// Alignment
//===========

#define BLOCK_SIZE_MIN (4*sizeof(size_t))
#define SIZE_BITS (sizeof(size_t)*8)

static inline size_t align_down(size_t value, size_t align)
{
return value&~(align-1);
}

static inline size_t align_up(size_t value, size_t align)
{
return value+(align-value%align)%align;
}


//======
// Heap
//======

typedef struct
{
size_t free;
size_t used;
size_t size;
size_t free_block;
size_t map_free;
}heap_t;

void* heap_alloc(heap_t* heap, size_t size);
size_t heap_available(heap_t* heap);
heap_t* heap_create(size_t offset, size_t size);
void heap_free(heap_t* heap, void* buffer);
size_t heap_get_largest_free_block(heap_t* heap);
void heap_reserve(heap_t* handle, size_t offset, size_t size);


//===============
// Heap Internal
//===============

void* heap_alloc_from_cache(heap_t* heap, size_t size);
void* heap_alloc_from_foot(heap_t* heap, size_t size);
void* heap_alloc_from_map(heap_t* heap, size_t size);
void* heap_alloc_internal(heap_t* heap, size_t size);
void heap_free_cache(heap_t* heap);
void heap_free_to_cache(heap_t* heap, void* buf);
void heap_free_to_map(heap_t* heap, void* buf);


//============
// Heap-Block
//============

typedef struct
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

static inline size_t heap_block_calc_size(size_t size)
{
return align_up(size, sizeof(size_t))+2*sizeof(size_t);
}

static inline size_t heap_block_get_offset(void* ptr)
{
return (size_t)ptr-sizeof(size_t);
}

static inline void* heap_block_get_pointer(size_t offset)
{
return (void*)(offset+sizeof(size_t));
}

void heap_block_get_chain(heap_t* heap, void* ptr, heap_block_chain_t* info);
void heap_block_get_info(heap_t* heap, void* ptr, heap_block_info_t* info);
void* heap_block_init(heap_t* heap, heap_block_info_t const* info);


//===============
// Cluster-Group
//===============

typedef struct
{
union
	{
	struct
		{
		uint32_t dirty: 1;
		uint32_t locked: 1;
		uint32_t level: 14;
		uint32_t child_count: 16;
		};
	uint32_t value;
	};
}cluster_group_t;


//======================
// Cluster-Parent-Group
//======================

typedef struct
{
cluster_group_t header;
size_t first;
size_t last;
cluster_group_t* children[CLUSTER_GROUP_SIZE];
}cluster_parent_group_t;

void cluster_parent_group_append_groups(cluster_parent_group_t* group, cluster_group_t* const* append, uint32_t count);
void cluster_parent_group_cleanup(heap_t* heap, cluster_parent_group_t* group);
int16_t cluster_parent_group_get_nearest_space(cluster_parent_group_t* group, int16_t pos);
void cluster_parent_group_insert_groups(cluster_parent_group_t* group, uint32_t at, cluster_group_t* const* insert, uint32_t count);
void cluster_parent_group_remove_group(heap_t* heap, cluster_parent_group_t* group, uint32_t at);
void cluster_parent_group_remove_groups(cluster_parent_group_t* group, uint32_t at, uint32_t count);


//====================
// Offset-Index-Group
//====================

typedef cluster_group_t offset_index_group_t;

bool offset_index_group_add_offset(heap_t* heap, offset_index_group_t* group, size_t offset, bool again);
size_t offset_index_group_get_first_offset(offset_index_group_t* group);
size_t offset_index_group_get_last_offset(offset_index_group_t* group);
size_t offset_index_group_remove_last_offset(heap_t* heap, offset_index_group_t* group);
void offset_index_group_remove_offset(heap_t* heap, offset_index_group_t* group, size_t offset);


//=========================
// Offset-Index-Item-Group
//=========================

typedef struct
{
cluster_group_t header;
size_t items[CLUSTER_GROUP_SIZE];
}offset_index_item_group_t;

bool offset_index_item_group_add_offset(offset_index_item_group_t* group, size_t offset);
void offset_index_item_group_append_items(offset_index_item_group_t* group, size_t const* append, uint32_t count);
offset_index_item_group_t* offset_index_item_group_create(heap_t* heap);
size_t offset_index_item_group_get_first_offset(offset_index_item_group_t* group);
uint32_t offset_index_item_group_get_item_pos(offset_index_item_group_t* group, size_t offset, bool* exists_ptr);
size_t offset_index_item_group_get_last_offset(offset_index_item_group_t* group);
void offset_index_item_group_insert_items(offset_index_item_group_t* group, uint32_t pos, size_t const* insert, uint32_t count);
size_t offset_index_item_group_remove_item(offset_index_item_group_t* group, uint32_t pos);
void offset_index_item_group_remove_items(offset_index_item_group_t* group, uint32_t pos, uint32_t count);
size_t offset_index_item_group_remove_last_offset(offset_index_item_group_t* group);
void offset_index_item_group_remove_offset(offset_index_item_group_t* group, size_t offset);


//===========================
// Offset-Index-Parent-Group
//===========================

typedef struct
{
cluster_group_t header;
size_t first_offset;
size_t last_offset;
offset_index_group_t* children[CLUSTER_GROUP_SIZE];
}offset_index_parent_group_t;

bool offset_index_parent_group_add_offset(heap_t* heap, offset_index_parent_group_t* group, size_t offset, bool again);
bool offset_index_parent_group_add_offset_internal(heap_t* heap, offset_index_parent_group_t* group, size_t offset, bool again);
void offset_index_parent_group_append_groups(offset_index_parent_group_t* group, offset_index_group_t* const* append, uint32_t count);
bool offset_index_parent_group_combine_child(heap_t* heap, offset_index_parent_group_t* group, uint32_t pos);
offset_index_parent_group_t* offset_index_parent_group_create(heap_t* heap, uint32_t level);
offset_index_parent_group_t* offset_index_parent_group_create_with_child(heap_t* heap, offset_index_group_t* child);
uint32_t offset_index_parent_group_get_item_pos(offset_index_parent_group_t* group, size_t offset, uint32_t* pos_ptr, bool must_exist);
void offset_index_parent_group_insert_groups(offset_index_parent_group_t* group, uint32_t pos, offset_index_group_t* const* insert, uint32_t count);
void offset_index_parent_group_move_children(offset_index_parent_group_t* group, uint32_t from, uint32_t to, uint32_t count);
void offset_index_parent_group_move_empty_slot(offset_index_parent_group_t* group, uint32_t from, uint32_t to);
void offset_index_parent_group_remove_groups(offset_index_parent_group_t* group, uint32_t pos, uint32_t count);
size_t offset_index_parent_group_remove_last_offset(heap_t* heap, offset_index_parent_group_t* group, bool passive);
void offset_index_parent_group_remove_offset(heap_t* heap, offset_index_parent_group_t* group, size_t offset);
bool offset_index_parent_group_shift_children(offset_index_parent_group_t* group, uint32_t pos, uint32_t count);
bool offset_index_parent_group_split_child(heap_t* heap, offset_index_parent_group_t* group, uint32_t pos);
void offset_index_parent_group_update_bounds(offset_index_parent_group_t* group);


//==============
// Offset-Index
//==============

typedef struct
{
offset_index_group_t* root;
}offset_index_t;

bool offset_index_add_offset(heap_t* heap, offset_index_t* index, size_t offset);
size_t offset_index_drop_root(heap_t* heap, offset_index_t* index);
bool offset_index_lift_root(heap_t* heap, offset_index_t* index);


//================
// Block-Map-Item
//================

typedef struct
{
size_t size;
union
	{
	struct
		{
		size_t offset: SIZE_BITS-1;
		size_t single: 1;
		};
	offset_index_t index;
	};
}block_map_item_t;


//=================
// Block-Map-Group
//=================

typedef cluster_group_t block_map_group_t;

int16_t block_map_group_add_block(heap_t* heap, block_map_group_t* group, heap_block_info_t const* info, bool again);
bool block_map_group_get_block(heap_t* heap, block_map_group_t* group, size_t min_size, heap_block_info_t* info);
size_t block_map_group_get_first_size(block_map_group_t* group);
size_t block_map_group_get_last_size(block_map_group_t* group);
void block_map_group_remove_block(heap_t* heap, block_map_group_t* group, heap_block_info_t const* info);


//======================
// Block-Map-Item-Group
//======================

typedef struct
{
cluster_group_t header;
block_map_item_t items[CLUSTER_GROUP_SIZE];
}block_map_item_group_t;

int16_t block_map_item_group_add_block(heap_t* heap, block_map_item_group_t* group, heap_block_info_t const* info);
bool block_map_item_group_add_item(block_map_item_group_t* group, heap_block_info_t const* info, uint32_t pos);
void block_map_item_group_append_items(block_map_item_group_t* group, block_map_item_t const* items, uint32_t count);
void block_map_item_group_cleanup(heap_t* heap, block_map_item_group_t* group, size_t ignore);
block_map_item_group_t* block_map_item_group_create(heap_t* heap);
bool block_map_item_group_get_block(heap_t* heap, block_map_item_group_t* group, size_t min_size, heap_block_info_t* info, bool passive);
size_t block_map_item_group_get_first_size(block_map_item_group_t* group);
uint32_t block_map_item_group_get_item_pos(block_map_item_group_t* group, size_t size, bool* exists_ptr);
size_t block_map_item_group_get_last_size(block_map_item_group_t* group);
void block_map_item_group_insert_items(block_map_item_group_t* group, uint32_t pos, block_map_item_t const* items, uint32_t count);
void block_map_item_group_remove_block(heap_t* heap, block_map_item_group_t* group, heap_block_info_t const* info);
size_t block_map_item_group_remove_item_at(block_map_item_group_t* group, uint32_t pos, bool passive);
void block_map_item_group_remove_items(block_map_item_group_t* group, uint32_t pos, uint32_t count);


//========================
// Block-Map-Parent-Group
//========================

typedef struct
{
cluster_group_t header;
size_t first_size;
size_t last_size;
block_map_group_t* children[CLUSTER_GROUP_SIZE];
}block_map_parent_group_t;

int16_t block_map_parent_group_add_block(heap_t* heap, block_map_parent_group_t* group, heap_block_info_t const* info, bool again);
int16_t block_map_parent_group_add_block_internal(heap_t* heap, block_map_parent_group_t* group, heap_block_info_t const* info, bool again);
void block_map_parent_group_append_groups(block_map_parent_group_t* group, block_map_group_t* const* append, uint32_t count);
bool block_map_parent_group_combine_child(heap_t* heap, block_map_parent_group_t* group, uint32_t pos);
block_map_parent_group_t* block_map_parent_group_create(heap_t* heap, uint32_t level);
block_map_parent_group_t* block_map_parent_group_create_with_child(heap_t* heap, block_map_group_t* child);
bool block_map_parent_group_get_block(heap_t* heap, block_map_parent_group_t* group, size_t min_size, heap_block_info_t* info, bool passive);
uint32_t block_map_parent_group_get_item_pos(block_map_parent_group_t* group, size_t size, uint32_t* pos_ptr, bool must_exist);
void block_map_parent_group_insert_groups(block_map_parent_group_t* group, uint32_t pos, block_map_group_t* const* insert, uint32_t count);
void block_map_parent_group_move_children(block_map_parent_group_t* group, uint32_t from, uint32_t to, uint32_t count);
void block_map_parent_group_move_empty_slot(block_map_parent_group_t* group, uint32_t from, uint32_t to);
void block_map_parent_group_remove_block(heap_t* heap, block_map_parent_group_t* group, heap_block_info_t const* info);
void block_map_parent_group_remove_groups(block_map_parent_group_t* group, uint32_t pos, uint32_t count);
bool block_map_parent_group_shift_children(block_map_parent_group_t* group, uint32_t pos, uint32_t count);
bool block_map_parent_group_split_child(heap_t* heap, block_map_parent_group_t* group, uint32_t pos);
void block_map_parent_group_update_bounds(block_map_parent_group_t* group);


//===========
// Block-Map
//===========

typedef struct
{
block_map_group_t* root;
}block_map_t;

bool block_map_add_block(heap_t* heap, block_map_t* map, heap_block_info_t const* info);
bool block_map_drop_root(heap_t* heap, block_map_t* map);
bool block_map_get_block(heap_t* heap, block_map_t* map, size_t min_size, heap_block_info_t* info);

static inline size_t block_map_get_last_size(block_map_t* map)
{
if(!map->root)
	return 0;
return block_map_group_get_last_size(map->root);
}

static inline void block_map_init(block_map_t* map)
{
map->root=NULL;
}

bool block_map_lift_root(heap_t* heap, block_map_t* map);
void block_map_remove_block(heap_t* heap, block_map_t* map, heap_block_info_t const* info);


#ifdef __cplusplus // extern "C"
}
#endif

#endif // _HEAP_H
