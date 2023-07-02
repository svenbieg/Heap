//=================
// cluster_group.h
//=================

// Header of groups used for sorting.
// Memory-access needs to be 32 bit in IRAM.

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap


#ifndef _CLUSTER_GROUP_H
#define _CLUSTER_GROUP_H


//=======
// Using
//=======

#include "heap_internal.h"


//=======
// Group
//=======

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


//==================
// Con-/Destructors
//==================

void cluster_group_init(cluster_group_t* group, uint16_t level, uint16_t child_count);


//========
// Access
//========

uint16_t cluster_group_get_child_count(cluster_group_t* group);
size_t cluster_group_get_item_count(cluster_group_t* group);
uint16_t cluster_group_get_level(cluster_group_t* group);
bool cluster_group_is_dirty(cluster_group_t* group);
bool cluster_group_is_locked(cluster_group_t* group);


//==============
// Modification
//==============

void cluster_group_set_child_count(cluster_group_t* group, uint16_t child_count);
void cluster_group_set_dirty(cluster_group_t* group, bool dirty);
void cluster_group_set_locked(cluster_group_t* group, bool lock);


#endif // _CLUSTER_GROUP_H
