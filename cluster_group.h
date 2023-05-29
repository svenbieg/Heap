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
		uint16_t level;
		uint16_t child_count;
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


//==============
// Modification
//==============

void cluster_group_set_child_count(cluster_group_t* group, uint16_t child_count);

#endif // _CLUSTER_GROUP_H
