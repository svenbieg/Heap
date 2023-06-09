//=================
// cluster_group.c
//=================

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

void cluster_group_init(cluster_group_t* group, uint16_t level, uint16_t child_count)
{
cluster_group_t set={ .level=level, .child_count=child_count };
group->value=set.value;
}


//========
// Access
//========

uint16_t cluster_group_get_child_count(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.child_count;
}

size_t cluster_group_get_item_count(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
if(get.level==0)
	return get.child_count;
return parent_group_get_item_count((parent_group_t*)group);
}

uint16_t cluster_group_get_level(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.level;
}

bool cluster_group_is_dirty(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.dirty;
}

bool cluster_group_is_locked(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.locked;
}


//==============
// Modification
//==============

void cluster_group_set_child_count(cluster_group_t* group, uint16_t child_count)
{
cluster_group_t set={ .value=group->value };
set.child_count=child_count;
group->value=set.value;
}

void cluster_group_set_dirty(cluster_group_t* group, bool dirty)
{
cluster_group_t set={ .value=group->value };
set.dirty=dirty;
group->value=set.value;
}

void cluster_group_set_locked(cluster_group_t* group, bool lock)
{
cluster_group_t set={ .value=group->value };
assert(set.locked!=lock);
set.locked=lock;
group->value=set.value;
}
