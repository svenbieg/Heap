//=================
// cluster_group.c
//=================

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap


//=======
// Using
//=======

#include "cluster_group.h"
#include "parent_group.h"


//========
// Access
//========

size_t cluster_group_get_item_count(cluster_group_t* group)
{
cluster_group_t get;
get.value=group->value;
if(get.level==0)
	return get.child_count;
return parent_group_get_item_count((parent_group_t*)group);
}
