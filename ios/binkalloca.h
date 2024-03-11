// Copyright Epic Games, Inc. All Rights Reserved.
// this ugliness is for cross platform alloca
#include <stdlib.h>
#ifndef alloca
#if defined(_MSC_VER)
  #include <malloc.h>
#elif defined(__GNUC__)
  #include <alloca.h>
#endif
#endif
#define radalloca(amt) ((void*)((((UINTa)alloca((amt)+15))+15)&~15))
