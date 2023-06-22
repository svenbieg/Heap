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

inline void cluster_group_init(cluster_group_t* group, uint16_t level, uint16_t child_count)
{
cluster_group_t set={ .level=level, .child_count=child_count };
group->value=set.value;
}


//========
// Access
//========

inline uint16_t cluster_group_get_child_count(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.child_count;
}

inline size_t cluster_group_get_item_count(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
if(get.level==0)
	return get.child_count;
return parent_group_get_item_count((parent_group_t*)group);
}

inline uint16_t cluster_group_get_level(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.level;
}

inline bool cluster_group_is_dirty(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.dirty;
}

inline bool cluster_group_is_locked(cluster_group_t* group)
{
cluster_group_t get={ .value=group->value };
return get.locked;
}


//==============
// Modification
//==============

inline void cluster_group_set_child_count(cluster_group_t* group, uint16_t child_count)
{
cluster_group_t set={ .value=group->value };
set.child_count=child_count;
group->value=set.value;
}

inline void cluster_group_set_dirty(cluster_group_t* group, bool dirty)
{
cluster_group_t set={ .value=group->value };
set.dirty=dirty;
group->value=set.value;
}

inline void cluster_group_set_locked(cluster_group_t* group, bool lock)
{
cluster_group_t set={ .value=group->value };
assert(set.locked!=lock);
set.locked=lock;
group->value=set.value;
}

#endif // _CLUSTER_GROUP_H
