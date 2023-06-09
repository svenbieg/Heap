//=======
// pch.h
//=======

// Platform header for internal heap-functions

// Copyright 2023, Sven Bieg (svenbieg@web.de)
// http://github.com/svenbieg/heap

#ifndef _PCH_H
#define _PCH_H


//=======
// Using
//=======

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <Windows.h>
#undef assert
#define assert(x) if(!(x))RaiseException(ERROR_SUCCESS, 0, 0, 0);

#endif
